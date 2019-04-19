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

/* Game can transfer at most 784 bytes / frame.
 *
 * That means 4704 bytes every 100 ms. Let's aim to handle double that, making
 * our read buffer 10000 bytes for 100 ms. The cycle time was lowered to 11 ms
 * (sendto takes about 10ms on average). Because of this I lowered the buffer
 * from what it needed to be at 100 ms
 */

#define READ_BUF_SIZE		2500
#define THREAD_CYCLE_TIME_MS	1	// Thread loop interval (ms)

#define HANDSHAKE_TIMEOUT_MS	5000	// Handshake timeout (ms)
#define CHECK_ALIVE_S		2	// Interval for HELO packets (s)

// Thread stuff
static u32 SlippiNetwork_Thread;
extern char __slippi_network_stack_addr, __slippi_network_stack_size;
static u32 SlippiNetworkHandlerThread(void *arg);

// State of the server running in this thread
#define SERVER_PORT	666
static int server_sock ALIGNED(32);
static struct sockaddr_in server ALIGNED(32) = {
	.sin_family	= AF_INET,
	.sin_port	= SERVER_PORT,
	{
		.s_addr	= INADDR_ANY,
	},
};

// State of the currently-connected client
struct SlippiClient client ALIGNED(32);

// Saved state of the previous client
struct SlippiClient client_prev ALIGNED(32);

// Global network state
extern s32 top_fd;			// from kernel/net.c
u32 SlippiServerStarted = 0;		// used by kernel/main.c


/* SlippiNetworkInit()
 * Dispatch the server thread. This should only be run once in kernel/main.c
 * after NCDInit() has actually brought up the networking stack and we have
 * some connectivity.
 */
void SlippiNetworkShutdown() { thread_cancel(SlippiNetwork_Thread, 0); }
s32 SlippiNetworkInit()
{
	server_sock = -1;
	client.socket = -1;

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
 * Create a new server socket, bind to it, then start listening on it.
 * If bind() or listen() return some error, increment the retry counter and
 * reset the server state.
 *
 * TODO: Probably shut down the networking thread if we fail to initialize.
 */
void stopServer() { close(top_fd, server_sock); server_sock = -1; }
#define MAX_SERVER_RETRIES	10
static int server_retries = 0;
s32 startServer()
{
	s32 res;

	// If things are broken, stop trying to initialize the server
	if (server_retries >= MAX_SERVER_RETRIES)
	{
		// Maybe shutdown the network thread here?
		dbgprintf("WARN: MAX_SERVER_RETRIES exceeded, giving up\r\n");
		return -1;
	}

	server_sock = socket(top_fd, AF_INET, SOCK_STREAM, IPPROTO_IP);
	if (server_sock < 0)
	{
		dbgprintf("WARN: server socket returned %d\r\n", server_sock);
		server_retries += 1;
		server_sock = -1;
		return server_sock;
	}

	res = bind(top_fd, server_sock, (struct sockaddr *)&server);
	if (res < 0)
	{
		stopServer();
		server_retries += 1;
		dbgprintf("WARN: bind() failed with: %d\r\n", res);
		return res;
	}
	res = listen(top_fd, server_sock, 1);
	if (res < 0)
	{
		stopServer();
		server_retries += 1;
		dbgprintf("WARN: listen() failed with: %d\r\n", res);
		return res;
	}

	server_retries = 0;
	return server_sock;
}


/* waitForMessage()
 * Poll a socket with some timeout, waiting for a message to arrive. If the
 * socket is readable (we received a message), immediately return true.
 * Otherwise, if the call times out (no message arrived), return false.
 */
bool waitForMessage(s32 socket, u32 timeout_ms)
{
	// Don't do anything if the socket is invalid
	if (socket < 0) return 0;

	STACK_ALIGN(struct pollsd, client_poll, 1, 32);
	client_poll[0].socket = socket;
	client_poll[0].events = POLLIN;

	s32 res = poll(top_fd, client_poll, 1, timeout_ms);

	// TODO: How to handle potential errors here?
	if (res < 0) dbgprintf("WARN: poll() returned %d\r\n", res);

	if ((client_poll[0].revents & POLLIN)) return true;
	else return false;
}


/* generateToken()
 * Generate a suitable token representing a client's session.
 * Avoids generating FB_TOKEN. Takes u32 'except', to avoid re-generating.
 */
u32 generateToken(u32 except)
{
	union Token tok = { 0 };

	while ((tok.word == FB_TOKEN) || (tok.word == except))
		IOSC_GenerateRand(tok.bytes, 4);
	return tok.word;
}


/* createClient()
 * Given some accepted socket, create the client's state.
 */
bool createClient(s32 socket)
{
	int flags = 1;
	u32 hs_token = 0;

	// If we don't get a handshake, create a fallback client and return
	dbgprintf("Waiting for handshake ...\r\n");
	bool gotHandshake = waitForMessage(socket, HANDSHAKE_TIMEOUT_MS);
	if (!gotHandshake)
	{
		dbgprintf("Client sent no handshake, falling back\r\n");
		client.socket = socket;
		client.timestamp = read32(HW_TIMER);
		client.token = FB_TOKEN;
		client.cursor = 0;
		client.version = CLIENT_FALLBACK;

		// Set TCP_NODELAY on the client
		setsockopt(top_fd, client.socket, IPPROTO_TCP, TCP_NODELAY,
				(void*)&flags, sizeof(flags));
		return true;
	}

	recvfrom(top_fd, socket, &hs_token, 4, 0);
	dbgprintf("Got handshake token %08x from client\r\n", hs_token);

	// If the handshake matches the last session, restore session state
	if ((hs_token == client_prev.token) && (hs_token != FB_TOKEN))
	{
		client.cursor = client_prev.cursor;
		memset(&client_prev, 0, sizeof(struct SlippiClient));
		dbgprintf("Matched previous token, restored cursor 0x%08x %08x\r\n",
				(client.cursor >> 32) & 0xffffffff,
				(client.cursor & 0xffffffff));
	}
	// Otherwise (no match, or client sent FB_TOKEN), create a new session
	else
	{
		dbgprintf("Creating a new session ...\r\n");
		client.cursor = 0;
	}

	client.token = generateToken(client_prev.token);
	client.socket = socket;
	client.timestamp = read32(HW_TIMER);
	client.version = CLIENT_LATEST;

	setsockopt(top_fd, client.socket, IPPROTO_TCP, TCP_NODELAY,
			(void*)&flags, sizeof(flags));

	// Send the new token to the client
	sendto(top_fd, client.socket, &client.token, 4, 0);
	dbgprintf("Sent new token %08x to client\r\n", client.token);

	return true;
}


/* listenForClient()
 * If no remote host is connected, block until a client to connects to the
 * server. Potentially do some handshake to negotiate things with a client.
 * Then, fill out a new entry for the client state/session.
 */
void listenForClient()
{
	// We already have an active client
	if (client.socket >= 0)
		return;

	// Block here until we accept a new client connection
	s32 socket = accept(top_fd, server_sock);

	// If the socket isn't valid, accept() returned some error
	if (socket < 0)
	{
		dbgprintf("WARN: accept returned %d, server restart\r\n", socket);
		stopServer();
		return;
	}

	// Actually create a new client
	dbgprintf("Detected connection, creating client ...\r\n");
	createClient(socket);
}


/* killClient()
 * Close our connection with the current client.
 */
void killClient()
{
	dbgprintf("WARN: Client disconnected (socket %d, token=%08x)\r\n",
			client.socket, client.token);
	close(top_fd, client.socket);

	// Save session when a client dies
	if (client.version == CLIENT_LATEST)
		memcpy(&client_prev, &client, sizeof(struct SlippiClient));

	client.socket = -1;
	client.timestamp = 0;
	client.cursor = 0;
	client.version = 0;
	client.token = 0;

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

	s32 res = sendto(top_fd, client.socket, readBuf, reader.lastReadResult.bytesRead, 0);

	// Naive client hangup detection
	if (res < 0)
	{
		killClient();
		return res;
	}

	// Indicate client still active
	client.timestamp = read32(HW_TIMER);

	// Only update read position if transfer was successful
	memReadPos += reader.lastReadResult.bytesRead;

	// Save the latest cursor in client's session data
	if (client.version == CLIENT_LATEST)
		client.cursor = memReadPos;

	return 0;
}


/* getConnectionStatus()
 * Return the current status of the networking thread.
 */
int getConnectionStatus()
{
	if (server_sock < 0)
		return CONN_STATUS_NO_SERVER;
	if (client.socket < 0)
		return CONN_STATUS_NO_CLIENT;
	else if (client.socket >= 0)
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

	// Only check if we haven't detected any communication
	if (TimerDiffSeconds(client.timestamp) < 2)
		return 0;

	// Send a 'HELO' packet to the client
	s32 res = sendto(top_fd, client.socket, alive_msg, sizeof(alive_msg), 0);

	// Update timestamp on success, otherwise kill the current client
	if (res == sizeof(alive_msg))
		client.timestamp = read32(HW_TIMER);
	else if (res <= 0)
		killClient();

	// Nothing interesting is happening [probably], so we're free to sleep
	mdelay(250);
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
