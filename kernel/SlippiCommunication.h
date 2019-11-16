#ifndef __SLIPPI_COMMUNICATION_H__
#define __SLIPPI_COMMUNICATION_H__

#include "global.h"

#define KEEPALIVE_MSG_BUF_SIZE 20
#define REPLAY_MSG_BUF_SIZE MAX_TX_SIZE + 500 // Max transfer size plus some for other data
#define HANDSHAKE_MSG_BUF_SIZE 128

typedef struct SlippiCommMsg {
	u32 size;
	u8* msg;
} SlippiCommMsg;

// Classes of messages;
enum messageType
{
	MSG_HANDSHAKE	= 1,
	MSG_REPLAY	= 2,
	MSG_KEEPALIVE = 3,
};

typedef struct ClientMsg {
	u8 type;
	void* payload;
} ClientMsg;

typedef struct HandshakeClientPayload {
	u64 cursor;
	u32 clientToken;
	bool isRealtime;
} HandshakeClientPayload;

// Create server messages to send
SlippiCommMsg genKeepAliveMsg();
SlippiCommMsg genReplayMsg(u8* data, u32 len, u64 readPos, u64 nextPos, bool forcePos);
SlippiCommMsg genHandshakeMsg(u32 token, u64 readPos);

// Read client messages received
ClientMsg readClientMessage(u8* buf, u32 len);

#endif
