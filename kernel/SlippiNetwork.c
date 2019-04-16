/* SlippiNetwork.c
 * Slippi thread for handling network transactions.
 */

#include "SlippiNetwork.h"
#include "SlippiDebug.h"
#include "SlippiMemory.h"
#include "common.h"
#include "string.h"
#include "debug.h"
#include "net.h"
#include "ff_utf8.h"

#include "SlippiNetworkBroadcast.h"

// Game can transfer at most 784 bytes / frame
// That means 4704 bytes every 100 ms. Let's aim to handle
// double that, making our read buffer 10000 bytes for 100 ms.
// The cycle time was lowered to 11 ms (sendto takes about 10ms
// on average), Because of this I lowered the buffer from what
// it needed to be at 100 ms
#define READ_BUF_SIZE 2500
#define THREAD_CYCLE_TIME_MS 1
#define CLIENT_TERMINATE_S 10

// Thread stuff
static u32 SlippiNetwork_Thread;
extern char __slippi_network_stack_addr, __slippi_network_stack_size;
static u32 SlippiNetworkHandlerThread(void *arg);

// Server state
static int server_sock ALIGNED(32);
static struct sockaddr_in server ALIGNED(32) = {
	.sin_family	= AF_INET,
	.sin_port	= 666,
	{
		.s_addr	= INADDR_ANY,
	},
};

// Client state
static int client_sock ALIGNED(32);
static u32 client_alive_ts;

// Structure representing the state of some remote client
struct SlippiClient
{
	s32 socket;
	char pad1[0x1c];
	u32 timestamp;
	char pad2[0x1c];
	u32 token;
	char pad3[0x1c];
	u64 cursor;
	char pad4[0x18];
};
struct SlippiClient currentClient ALIGNED(32);

// Global network state
extern s32 top_fd;		// from kernel/net.c
u32 SlippiServerStarted = 0;	// used by kernel/main.c

/* SlippiNetworkInit()
 * Dispatch the server thread. This should only be run once in kernel/main.c
 * after NCDInit() has actually brought up the networking stack and we have
 * some connectivity.
 */
void SlippiNetworkShutdown() { thread_cancel(SlippiNetwork_Thread, 0); }
s32 SlippiNetworkInit()
{
	server_sock = -1;
	client_sock = -1;

	dbgprintf("net_thread is starting ...\r\n");
	SlippiNetwork_Thread = do_thread_create(
		SlippiNetworkHandlerThread,
		((u32 *)&__slippi_network_stack_addr),
		((u32)(&__slippi_network_stack_size)),
		0x78);
	thread_continue(SlippiNetwork_Thread);
	SlippiServerStarted = 1;
	return 0;
}


/* startServer()
 * Create a new server socket, bind, then start listening.
 */
s32 startServer()
{
	s32 res;

	server_sock = socket(top_fd, AF_INET, SOCK_STREAM, IPPROTO_IP);
	dbgprintf("server_sock: %d\r\n", server_sock);

	res = bind(top_fd, server_sock, (struct sockaddr *)&server);
	if (res < 0)
	{
		close(top_fd, server_sock);
		server_sock = -1;
		dbgprintf("WARN: bind() failed with: %d\r\n", res);
		return res;
	}
	
	res = listen(top_fd, server_sock, 1);
	if (res < 0)
	{
		close(top_fd, server_sock);
		server_sock = -1;
		dbgprintf("WARN: listen() failed with: %d\r\n", res);
		return res;
	}
	return 0;
}


/* waitForHandshake()
 * Poll the client socket with a 5s timeout, expecting a message from the client.
 * If the socket is readable (we received a message), return true.
 * Otherwise, if the 5s elapse without a message, return false.
 */
s32 waitForHandshake(s32 socket)
{
	s32 res;
	STACK_ALIGN(struct pollsd, client_poll, 1, 32);

	// If the socket is readable, POLLIN is written to revents
	client_poll[0].socket = client_sock;
	client_poll[0].events = POLLIN;
	res = poll(top_fd, client_poll, 1, 5000);

	// If poll() returns an error, do something to handle it
	if (res < 0)
	{
		dbgprintf("WARN: poll() returned < 0 (%08x)\r\n", res);
	}

	dbgprintf("client_poll[0].revents=%08x\r\n", client_poll[0].revents);
	if ((client_poll[0].revents & POLLIN))
		return 1;
	else
		return 0;
}


/* listenForClient()
 * If no client is connected, block until a client to connects to the server.
 * If a client immediately sends us a message, upgrade them to the new interface.
 * Otherwise, assume the client is old and fallback to old behaviour.
 */
void listenForClient()
{
	s32 res;
	STACK_ALIGN(u32, token, 1, 32);

	// We already have a client
	if (client_sock >= 0)
		return;

	// Block here until we accept a client connection
	client_sock = accept(top_fd, server_sock);

	if (client_sock >= 0)
	{
		dbgprintf("Client connection detected, waiting for message...\r\n");

		// If the client sends a message, upgrade them to the new interface
		res = waitForHandshake(client_sock);
		if (res)
		{
			res = recvfrom(top_fd, client_sock, token, 4, 0);
			if (res == 4)
				dbgprintf("Got %d bytes from client; new interface!\r\n", res);
			else
				dbgprintf("Got invalid number of bytes from client (%d)\r\n", res);

		}
		// Otherwise, fallback to the old interface
		else
		{
			dbgprintf("No message; fallback to old interface\r\n", res);
		}


		int flags = 1;
		res = setsockopt(top_fd, client_sock, IPPROTO_TCP, TCP_NODELAY,
				(void *)&flags, sizeof(flags));
		dbgprintf("[TCP_NODELAY] Client setsockopt result: %d\r\n", res);

		client_alive_ts = read32(HW_TIMER);
	}
	// Otherwise, accept() returned some error - kill the server here?
	else
	{
		// Close server socket such that it can be re-initialized
		// TODO: When eth is disconnected, this still doesn't bring the connection back
		// TODO: server_sock keeps getting a -39. Figure out how to solve this
		close(top_fd, server_sock);
		server_sock = -1;
		dbgprintf("WARN: accept() returned an error: %d\r\n", client_sock);
	}
}


/* hangUpConnection()
 * Close our connection with the current client.
 */
void hangUpConnection() {
	// The error codes I have seen so far are -39 (ethernet disconnected) and -56 (client hung up)
	// Currently disable this timer thing because it doesn't seem like we can recover from them
	// anyway
	// if (TimerDiffSeconds(client_alive_ts) >= CLIENT_TERMINATE_S) {

	// Hang up client
	dbgprintf("Client disconnect detected\r\n");
	client_alive_ts = 0;
	close(top_fd, client_sock);
	client_sock = -1;
	reset_broadcast_timer();
}


/* handleFileTransfer()
 * Deal with sending Slippi data over the network:
 *
 *	1. Attempt read from Slippi buffer
 *	2. If we consume some data from buffer, send it to the client
 *	3. If sendto() isn't successful, hang up on a client
 *	4. Update our read cursor in Slippi buffer
 */
static u8 readBuf[READ_BUF_SIZE];
static u64 memReadPos = 0;
static SlpGameReader reader;
s32 handleFileTransfer()
{
	// Do nothing if we aren't connected to a client
	int status = getConnectionStatus();
	if (status != CONN_STATUS_CONNECTED)
		return 0;

	SlpMemError err = SlippiMemoryRead(&reader, readBuf, READ_BUF_SIZE, memReadPos);
	if (err)
	{
		if (err == SLP_READ_OVERFLOW)
		{
			memReadPos = SlippiRestoreReadPos();
			dbgprintf("WARN: Overflow read error detected. Reset to: %X\r\n", memReadPos);
		}
		mdelay(1000);

		// For specific errors, bytes will still be read. Not returning to deal with those
	}

	// If there's no new data to send, just return
	if (reader.lastReadResult.bytesRead == 0)
		return 0;

	s32 res = sendto(top_fd, client_sock, readBuf, reader.lastReadResult.bytesRead, 0);

	// Naive client hangup detection
	if (res < 0)
	{
		hangUpConnection();
		return res;
	}

	// Indicate client still active
	client_alive_ts = read32(HW_TIMER);

	// Only update read position if transfer was successful
	memReadPos += reader.lastReadResult.bytesRead;

	return 0;
}


/* getConnectionStatus()
 * Return the current status of the networking thread.
 */
int getConnectionStatus()
{
	if (server_sock < 0)
		return CONN_STATUS_NO_SERVER;
	if (client_sock < 0)
		return CONN_STATUS_NO_CLIENT;
	else if (client_sock >= 0)
		return CONN_STATUS_CONNECTED;

	return CONN_STATUS_UNKNOWN;
}


/* checkAlive()
 * Give some naive indication of whether or not a client has hung up. If our
 * sendto() here returns some error, this probably indicates that we can stop
 * talking to the current client and reset the socket.
 */
static char alive_msg[] ALIGNED(32) = "HELO";
s32 checkAlive(void)
{
	int status = getConnectionStatus();

	// Do nothing if we aren't connected to a client
	if (status != CONN_STATUS_CONNECTED)
		return 0;

	// Only check alive if we haven't detected any communication
	if (TimerDiffSeconds(client_alive_ts) < 2)
		return 0;

	s32 res;
	res = sendto(top_fd, client_sock, alive_msg, sizeof(alive_msg), 0);

	if (res == sizeof(alive_msg))
	{
		client_alive_ts = read32(HW_TIMER);
		// 250 ms wait. The goal here is that the keep alive message
		// will be sent by itself without anything following it
		mdelay(250);

		return 0;
	}
	else if (res <= 0)
	{
		hangUpConnection();

		// We no longer always hang up the connection, sleep a little bit to prevent send
		// attempts from sending too fast
		mdelay(250);
		return -1;
	}

	return 0;
}


/* SlippiNetworkHandlerThread()
 * This is the main loop for the server.
 *   - Initialize the server
 *   - Accept a client (if we aren't already tracking one)
 *   - Only transmit to client when there's some data left in SlipMem
 *   - When there's no valid data, periodically send some keep-alive
 *     messages to the client so we can determine if they've hung up
 */
static u32 SlippiNetworkHandlerThread(void *arg)
{
	while (1)
	{
		int status = getConnectionStatus();
		switch (status)
		{
		case CONN_STATUS_NO_SERVER:
			startServer();
			break;
		case CONN_STATUS_NO_CLIENT:
			listenForClient();
			break;
		case CONN_STATUS_CONNECTED:
			handleFileTransfer();
			checkAlive();
			break;
		}

		mdelay(THREAD_CYCLE_TIME_MS);
	}

	return 0;
}
