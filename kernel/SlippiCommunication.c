#include "SlippiCommunication.h"
#include "SlippiNetwork.h"

#include "ubj/ubj.h"

#include "string.h"
#include "debug.h"
#include "Config.h"

// Outbound UBJSON messages are kept in these buffers
static u8 keepAliveMsgBuf[KEEPALIVE_MSG_BUF_SIZE];
static u8 replayMsgBuf[REPLAY_MSG_BUF_SIZE];
static u8 handshakeMsgBuf[HANDSHAKE_MSG_BUF_SIZE];

// Payloads that sometimes need to be returned by readClientMessage
static HandshakeClientPayload handshakeClientPayload;


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
SlippiCommMsg genReplayMsg(u8* data, u32 len, u64 readPos, u64 nextPos, bool forcePos)
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
	ubjw_write_key(ctx, "nextPos");
	ubjw_write_buffer(ctx, (u8*)&nextPos, UBJ_UINT8, 8);
	ubjw_write_key(ctx, "forcePos");
	ubjw_write_bool(ctx, forcePos);
	ubjw_write_key(ctx, "data");
	ubjw_write_buffer(ctx, data, UBJ_UINT8, len);

	return finishMessage(ctx, replayMsgBuf);
}


/* genHandshakeMsg()
 * Create a new handshake message.
 */
SlippiCommMsg genHandshakeMsg(u32 token, u64 readPos)
{
	ubjw_context_t* ctx = initMessage(handshakeMsgBuf, HANDSHAKE_MSG_BUF_SIZE);

	// Write message type
	ubjw_write_key(ctx, "type");
	ubjw_write_uint8(ctx, MSG_HANDSHAKE);

	// Write payload
	ubjw_write_key(ctx, "payload");
	ubjw_begin_object(ctx, UBJ_MIXED, 0);
	ubjw_write_key(ctx, "nick");
	ubjw_write_string(ctx, slippi_settings->nickname);
	ubjw_write_key(ctx, "nintendontVersion");
	ubjw_write_string(ctx, NIN_VERSION_SHORT_STRING);
	ubjw_write_key(ctx, "clientToken");
	ubjw_write_buffer(ctx, (u8*)&token, UBJ_UINT8, 4);
	ubjw_write_key(ctx, "pos");
	ubjw_write_buffer(ctx, (u8*)&readPos, UBJ_UINT8, 8);

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

	if (len <= 8) {
		return msg;
	}

	msg.type = buf[8];
	switch (msg.type) {
	case MSG_HANDSHAKE: ; // wtf C? apparently I need this semicolon
		HandshakeClientPayload *payload = &handshakeClientPayload;

		memcpy(&payload->cursor, &buf[33], 8);
		memcpy(&payload->clientToken, &buf[60], 4);
		
		payload->isRealtime = buf[76] == 0x54; // Check if realtime bool equals "T"

		// TODO: Figure out why dbgprintf crashes Nintendont in this file
		// dbgprintf("[Tok] Val: %d\r\n", payload.clientToken);

		msg.payload = payload;
		break;
	}

	return msg;
}
