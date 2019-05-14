#ifndef __SLIPPI_COMMUNICATION_H__
#define __SLIPPI_COMMUNICATION_H__

#include "global.h"

typedef struct SlippiCommMsg {
  u32 size;
  u8* msg;
} SlippiCommMsg;

// Classes of messages;
enum messageType
{
	MSG_HANDSHAKE	= 1,
	MSG_REPLAY	= 2,
	MSG_KEEP_ALIVE = 3,
};

SlippiCommMsg genKeepAliveMsg();
SlippiCommMsg genReplayMsg(u8* data, u32 len, u64 readPos);
SlippiCommMsg getHandshakeMsg();

#endif
