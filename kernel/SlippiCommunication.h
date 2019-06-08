#ifndef __SLIPPI_COMMUNICATION_H__
#define __SLIPPI_COMMUNICATION_H__

#include "global.h"

#define KEEPALIVE_MSG_BUF_SIZE 	20
#define REPLAY_MSG_BUF_SIZE 		MAX_TX_SIZE
#define HANDSHAKE_MSG_BUF_SIZE 		128

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
} HandshakeClientPayload;

// Create server messages to send
SlippiCommMsg genKeepAliveMsg();
SlippiCommMsg genReplayMsg(u8* data, u32 len, u64 readPos);
SlippiCommMsg genHandshakeMsg(u32 token);

// Read client messages received
ClientMsg readClientMessage(u8* buf, u32 len);

#endif
