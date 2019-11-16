#ifndef __SLIPPI_NETWORK_H__
#define __SLIPPI_NETWORK_H__

#include "global.h"

// Server states
#define CONN_STATUS_UNKNOWN 0
#define CONN_STATUS_NO_SERVER 1
#define CONN_STATUS_NO_CLIENT 2
#define CONN_STATUS_CONNECTED 3

// This is the max amount of bytes that will be transferred on a single cycle. See comment in
// SlippiNetwork for more info. Should definitely be greater than at least 1500
// Update 11/15/2019: I increased this to allow specifically WiFi connections to "catch up" faster
// when the fall behind. Seems like the memory limit for this is about 61k before Nintendont fails
// to build. Trying 61k with 4 ICs still causes my network to back up and fail.
#define MAX_TX_SIZE 25000

#define FB_TOKEN	0x00000000
union Token {
	u8 bytes[4];
	u32 word;
};

/* struct handshakeReply
 *
 * Message sent back to a client after we receive a handshake.
 * Contains:
 *	- A new token representing this session
 *	- The nickname of this console
 *	- Nintendont version (maj_version << 16 | min_version)
 */
struct handshakeReply
{
	unsigned char cmd[10];		// Command name
	u32 token;			// New token for client
	u32 version;			// Nintendont version
	unsigned char nickname[32];	// Nickname of this console
};

/* struct SlippiClient
 *
 * Represents the state of some remote client. Members are padded to 32-byte
 * boundaries in case IOS is sensitive to that somewhere.
 *
 * Some of this information needs to be preserved across client hang up.
 * If a client [unintentionally] hangs up, we can use this data to restore
 * their progress in reading back replay data.
 *
 *	socket		- file descriptor for this client
 *	timestamp	- updated when a client ACKs one of our messages
 *	token		- a unique token/ID for this client
 *	cursor		- last known read cursor position
 *	version		- client compatibility version
 *	matchID		- The ID of the current match
 */

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

	u32 version;
	char pad6[0x1c];

	u32 matchID;
	char pad7[0x1c];
} ALIGNED(32);


// Values for the client "version" field
enum clientVersion
{
	CLIENT_ERROR	= 1,	// An error occured, or client is incompatible
	CLIENT_FALLBACK,	// Client relies on fallback to old interface
	CLIENT_LATEST,		// Client is compatible with latest interface
};


/* Slippi message format for communication with remote hosts.
 */
struct SlippiPacket
{
	char header[4];		// "SLIP"
	u32 type;		// See 'enum messageType'
	u32 len;		// Length of the 'data' array member

	// Keep some space around for later.
	// This also lets us pad data to a 32-byte boundary.
	u8 reserved[0x14];

	char data[0];		// Contents of the message
} ALIGNED(32);

// Function declarations
s32 SlippiNetworkInit();
void SlippiNetworkShutdown();
void SlippiNetworkSetNewFile(const char* path);
int getConnectionStatus();

#endif
