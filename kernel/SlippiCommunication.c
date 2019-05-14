#include "SlippiCommunication.h"
#include "SlippiNetwork.h"

#include "string.h"
#include "ubj/ubj.h"

#define KEEP_ALIVE_MSG_BUF_SIZE 20
#define REPLAY_MSG_BUF_SIZE MAX_TX_SIZE
#define HANDSHAKE_MSG_BUF_SIZE 100

static u8 keepAliveMsgBuf[KEEP_ALIVE_MSG_BUF_SIZE];
static u8 replayMsgBuf[REPLAY_MSG_BUF_SIZE];
static u8 handshakeMsgBuf[HANDSHAKE_MSG_BUF_SIZE];

ubjw_context_t* initMessage(u8* buf, u32 len) {
	ubjw_context_t* ctx = ubjw_open_memory(buf + 4, buf + len);

	ubjw_begin_object(ctx, UBJ_MIXED, 0);

  return ctx;
}

SlippiCommMsg finishMessage(ubjw_context_t* ctx, u8* buf) {
  ubjw_end(ctx);

	size_t msgSize = ubjw_close_context(ctx);

	// Write message size to the front of the message
	memcpy(buf, (u8*)&msgSize, 4);

  SlippiCommMsg resp;
  resp.msg = buf;
  resp.size = msgSize + 4;

  return resp;
}

SlippiCommMsg genKeepAliveMsg() {
  ubjw_context_t* ctx = initMessage(keepAliveMsgBuf, KEEP_ALIVE_MSG_BUF_SIZE);

  // Write message type
	ubjw_write_key(ctx, "type");
	ubjw_write_uint8(ctx, MSG_KEEP_ALIVE);

  return finishMessage(ctx, keepAliveMsgBuf);
}

SlippiCommMsg genReplayMsg(u8* data, u32 len, u64 readPos) {
  ubjw_context_t* ctx = initMessage(replayMsgBuf, REPLAY_MSG_BUF_SIZE);

  // Write message type
	ubjw_write_key(ctx, "type");
	ubjw_write_uint8(ctx, MSG_REPLAY);

	// Write payload object
	ubjw_write_key(ctx, "payload");
	ubjw_begin_object(ctx, UBJ_MIXED, 0);
	ubjw_write_key(ctx, "pos");
	ubjw_write_buffer(ctx, (u8*)&readPos, UBJ_UINT8, 8);
	ubjw_write_key(ctx, "data");
	ubjw_write_buffer(ctx, data, UBJ_UINT8, len);

  return finishMessage(ctx, replayMsgBuf);
}

SlippiCommMsg getHandshakeMsg() {
  ubjw_context_t* ctx = initMessage(handshakeMsgBuf, HANDSHAKE_MSG_BUF_SIZE);

  // Write message type
	ubjw_write_key(ctx, "type");
	ubjw_write_uint8(ctx, MSG_HANDSHAKE);

  return finishMessage(ctx, handshakeMsgBuf);
}
