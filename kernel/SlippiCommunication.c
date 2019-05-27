#include "SlippiCommunication.h"
#include "SlippiNetwork.h"

#include "common.h"
#include "string.h"
#include "debug.h"
#include "ff_utf8.h"
#include "Config.h"

#include "ubj/ubj.h"


// Outbound UBJSON messages are kept in these buffers
static u8 keepAliveMsgBuf[KEEPALIVE_MSG_BUF_SIZE];
static u8 replayMsgBuf[REPLAY_MSG_BUF_SIZE];
static u8 handshakeMsgBuf[HANDSHAKE_MSG_BUF_SIZE];


/* initMessage()
 * Given a buffer 'buf' and length 'len, return a fresh UBJSON object context
 * associated with the buffer, for creating a new UBJSON message.
 * Reserve the first four bytes of the buffer (buf + 4) for the message size.
 */
ubjw_context_t* initMessage(u8* buf, u32 len)
{
	ubjw_context_t* ctx = ubjw_open_memory(buf + 4, buf + len);
	ubjw_begin_object(ctx, UBJ_MIXED, 0);
	return ctx;
}


/* finishMessage()
 * Close UBJSON object context, then write the final message size to the
 * beginning of the buffer.
 */
SlippiCommMsg finishMessage(ubjw_context_t* ctx, u8* buf)
{
	SlippiCommMsg resp;

	ubjw_end(ctx);
	size_t msgSize = ubjw_close_context(ctx);

	// Write message size to the front of the message
	memcpy(buf, (u8*)&msgSize, 4);

	resp.msg = buf;
	resp.size = msgSize + 4;

	return resp;
}


/* genKeepAliveMsg()
 * Create a new keep-alive message.
 */
SlippiCommMsg genKeepAliveMsg()
{
 	ubjw_context_t* ctx = initMessage(keepAliveMsgBuf, KEEPALIVE_MSG_BUF_SIZE);

	// Write message type
	ubjw_write_key(ctx, "type");
	ubjw_write_uint8(ctx, MSG_KEEPALIVE);

	return finishMessage(ctx, keepAliveMsgBuf);
}


/* genReplayMsg()
 * Create a new replay data message.
 */
SlippiCommMsg genReplayMsg(u8* data, u32 len, u64 readPos)
{
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


/* genHandshakeMsg()
 * Create a new handshake message.
 */
SlippiCommMsg genHandshakeMsg()
{
	ubjw_context_t* ctx = initMessage(handshakeMsgBuf, HANDSHAKE_MSG_BUF_SIZE);

	int nickLen = strlen(slippi_settings->nickname);
	if (nickLen > 32) nickLen = 32;

	// Write message type
	ubjw_write_key(ctx, "type");
	ubjw_write_uint8(ctx, MSG_HANDSHAKE);

	// Write payload
	ubjw_write_key(ctx, "nick");
	ubjw_write_buffer(ctx, (u8*)slippi_settings->nickname, UBJ_UINT8, nickLen);
	ubjw_write_key(ctx, "maj_ver");
	ubjw_write_int16(ctx, NIN_MAJOR_VERSION);
	ubjw_write_key(ctx, "min_ver");
	ubjw_write_int16(ctx, NIN_MINOR_VERSION);

	return finishMessage(ctx, handshakeMsgBuf);
}

ClientMsg readClientMessage(u8* buf, u32 len)
{
	// ubjr_context_t* rctx = ubjr_open_memory(buf,buf+len);
	// ubjr_dynamic_t filestruct = ubjr_read_dynamic(rctx);

	// dbgprintf("[Read Client] Type: %d\r\n", filestruct.type);

	// ubjr_cleanup_dynamic(&filestruct);
	// ubjr_close_context(rctx);

	ClientMsg msg = { 0, NULL };

	if (len < 8) {
		return msg;
	}

	msg.type = buf[8];
	switch (msg.type) {
	case MSG_HANDSHAKE: ; // wtf C?
		HandshakeClientPayload payload = { 0, 0 };
		payload.cursor = *(u64*)(&buf[33]);

		msg.payload = &payload;
		break;
	}

	return msg;
}
