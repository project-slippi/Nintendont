/* SlippiNetwork.c
 * Slippi thread for handling network transactions.
 */

#include "SlippiNetwork.h"
#include "SlippiNetworkBroadcast.h"
#include "SlippiDebug.h"
#include "SlippiMemory.h"
#include "SlippiCommunication.h"

#include "common.h"
#include "string.h"
#include "debug.h"
#include "net.h"
#include "ff_utf8.h"
#include "Config.h"


/* The game can transfer at most 784 bytes / frame.
 *
 * That means 4704 bytes every 100 ms. Let's aim to handle double that, making
 * our read buffer 10000 bytes for 100 ms. The cycle time was lowered to 11 ms
 * (sendto takes about 10ms on average). Because of this I lowered the buffer
 * from what it needed to be at 100 ms
 */

// Size of our local read buffer. This is the max amount of bytes we are willing to read from
// SlippiMemory. We make sure to give some room off of MAX_TX_SIZE to fit the ubjson comm data
#define READ_BUF_SIZE	MAX_TX_SIZE - 500

#define THREAD_CYCLE_TIME_MS	1	// Thread loop interval (ms)
#define HANDSHAKE_TIMEOUT_MS	5000	// Handshake timeout (ms)
#define CHECK_ALIVE_S		2	// Interval for HELO packets (s)

// Thread state
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

// Buffer for replying to some handshake message 
struct handshakeReply handshake_reply ALIGNED(32) = { { "SLIP_HSHK\x00" }, 0, NIN_VERSION, { 0 }, };

// Global network state
extern s32 top_fd;			// from kernel/net.c
u32 SlippiServerStarted = 0;		// used by kernel/main.c

// Shared state from SlippiMemory.c
extern struct recordingState gameState;


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


/* waitForMessage()
 * Helper function - polls a socket with some timeout, waiting for a message 
 * to arrive. If the socket is readable (we received a message), immediately 
 * return true. Otherwise, if the call times out (no message arrived), return 
 * false. The caller is responsible for actually reading bytes off the socket.
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
 * Avoids generating FB_TOKEN. Takes 'u32 except', which we explicitly avoid 
 * generating.
 */
u32 generateToken(u32 except)
{
	union Token tok = { 0 };

	while ((tok.word == FB_TOKEN) || (tok.word == except))
		IOSC_GenerateRand(tok.bytes, 4);
	return tok.word;
}


/* getConnectionStatus()
 * Return the current status of the networking thread.
 * Typically, we set 'client.socket' to -1 when a client doesn't exist, and
 * the 'server_socket' to -1 when the server isn't running.
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


/* killClient()
 * Close our connection with the current client.
 * Save some state if the client supports session management.
 */
void killClient()
{
	dbgprintf("WARN: Client disconnected (socket %d, token=%08x)\r\n",
			client.socket, client.token);
	close(top_fd, client.socket);

	// If this is a new client, save session when they disconnect
	if (client.version == CLIENT_LATEST)
	{
		memcpy(&client_prev, &client, sizeof(struct SlippiClient));
		dbgprintf("Saved cursor=0x%08x from session\r\n", 
				(u32)client.cursor);
	}

	client.socket = -1;
	client.timestamp = 0;
	client.cursor = 0;
	client.version = 0;
	client.token = 0;

	reset_broadcast_timer();
}


/* checkAlive()
 * Give some naive indication of whether or not a client has hung up. If our
 * sendto() here returns some error, this probably indicates that we can stop
 * talking to the current client and reset the socket.
 *
 * TODO: Find a way of accomplishing this without sending any data?
 */
s32 checkAlive(void)
{
	int status = getConnectionStatus();

	// Do nothing if we aren't connected to a client
	if (status != CONN_STATUS_CONNECTED)
		return 0;

	// Only check if we haven't detected any communication
	if (TimerDiffSeconds(client.timestamp) < 2)
		return 0;

	// Send a keep alive message to the client
	SlippiCommMsg keepAliveMsg = genKeepAliveMsg();
	s32 res = sendto(top_fd, client.socket, keepAliveMsg.msg, keepAliveMsg.size, 0);

	// Update timestamp on success, otherwise kill the current client
	if (res == keepAliveMsg.size)
		client.timestamp = read32(HW_TIMER);
	else if (res <= 0)
		killClient();

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

	// Initialize handshake reply with nickname and Nintendont version
	int nickLen = strlen(slippi_settings->nickname);
	if (nickLen > 32) nickLen = 32;
	memcpy(&handshake_reply.nickname, slippi_settings->nickname, nickLen);
	sync_after_write(&handshake_reply, sizeof(handshake_reply));

	server_retries = 0;
	return server_sock;
}


/* createClient()
 * Given some client socket accept()'ed by the server, create client state.
 * Waits for a handshake message from a client. If there is no handshake, 
 * assume the client will use fallback behavior.
 *
 *	- If the client's handshake token matches a previous session, restore
 *	  state for them (otherwise, create a new session)
 *	- Always rotate the token and return a new one to the client
 */
static u64 fallbackCursor;
bool createClient(s32 socket)
{
	int flags = 1;
	u32 hs_token = 0;

	dbgprintf("HSHK: Waiting ...\r\n");
	bool gotHandshake = waitForMessage(socket, HANDSHAKE_TIMEOUT_MS);

	// If we don't get a handshake, create a fallback client and return
	if (!gotHandshake)
	{
		dbgprintf("HSHK: Timed out, using fallback\r\n");
		client.socket = socket;
		client.timestamp = read32(HW_TIMER);
		client.token = FB_TOKEN;
		client.cursor = fallbackCursor;
		client.matchID = gameState.matchID;
		client.version = CLIENT_FALLBACK;

		setsockopt(top_fd, client.socket, IPPROTO_TCP, TCP_NODELAY, (void*)&flags, sizeof(flags));
		dbgprintf("HSHK: Created fallback client with cursor 0x%08x...\r\n", (u32)client.cursor);
		return true;
	}

	// Read the client's handshake off the socket
	recvfrom(top_fd, socket, &hs_token, 4, 0);
	dbgprintf("HSHK: Got token %08x from client\r\n", hs_token);

	// If the token matches the previous one AND we're in the same game
	bool validToken = ((gameState.matchID == client_prev.matchID)
			&& (hs_token == client_prev.token)
			&& hs_token != FB_TOKEN);

	if (gameState.inGame)
	{
		if (validToken)
		{
			client.cursor = client_prev.cursor;
			client.matchID = client_prev.matchID;
			memset(&client_prev, 0, sizeof(struct SlippiClient));
			dbgprintf("HSHK: Matched token, restored cursor 0x%08x\r\n", 
					(u32)client.cursor);
		}
		else
		{
			client.cursor = gameState.baseCursor;
			client.matchID = gameState.matchID;
		}

	}
	else
	{
		client.cursor = SlippiRestoreReadPos();
	}

	client.token = generateToken(client_prev.token);
	client.socket = socket;
	client.timestamp = read32(HW_TIMER);
	client.version = CLIENT_LATEST;

	setsockopt(top_fd, client.socket, IPPROTO_TCP, TCP_NODELAY, (void*)&flags, sizeof(flags));

	// Send a reply to the client (with the new token for this session)
	handshake_reply.token = client.token;
	sync_after_write(&handshake_reply, sizeof(handshake_reply));
	sendto(top_fd, client.socket, &handshake_reply, sizeof(handshake_reply), 0);
	dbgprintf("HSHK: Sent token %08x to client\r\n", client.token);

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

/* handleFileTransfer()
 * Deal with sending Slippi data over the network:
 *
 *	1. Attempt read from Slippi buffer
 *	2. If we consume some data from buffer, send it to the client
 *	3. If sendto() isn't successful, hang up on a client
 *	4. Update our read cursor in Slippi buffer
 */
static u8 readBuf[READ_BUF_SIZE];
static SlpGameReader reader;
s32 handleFileTransfer()
{
	// Do nothing if we aren't connected to a client
	int status = getConnectionStatus();
	if (status != CONN_STATUS_CONNECTED)
		return 0;

	// Read some bytes into the local buffer
	SlpMemError err = SlippiMemoryRead(&reader, readBuf, READ_BUF_SIZE, client.cursor);
	if (err)
	{
		// On an overflow read, reset to the write cursor
		if (err == SLP_READ_OVERFLOW)
		{
			client.cursor = SlippiRestoreReadPos();
			dbgprintf("WARN: Overflow read error detected. Reset to: %X\r\n", client.cursor);
		}
		mdelay(1000);
		// For specific errors, bytes will still be read. Not returning to deal with those
	}

	// If there's no new data to send, just return
	if (reader.lastReadResult.bytesRead == 0)
		return 0;

	// Actually send data to the client
	SlippiCommMsg replayMsg = genReplayMsg(readBuf, reader.lastReadResult.bytesRead, client.cursor);
	s32 res = sendto(top_fd, client.socket, replayMsg.msg, replayMsg.size, 0);

	// If sendto() returns < 0, the client has disconnected
	if (res < 0)
	{
		dbgprintf("[SENDTO FAIL] Bytes read: %d | Last frame %d\r\n", reader.lastReadResult.bytesRead, reader.metadata.lastFrame);
		killClient();
		return res;
	}

	// dbgprintf("Bytes read: %d | Last frame %d\r\n", reader.lastReadResult.bytesRead, reader.metadata.lastFrame);

	// When we successfully transmit, update the client's cursor
	client.timestamp = read32(HW_TIMER);
	client.cursor += reader.lastReadResult.bytesRead;
	fallbackCursor = client.cursor;

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
