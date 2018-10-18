#include "SlippiMemory.h"
#include "SlippiDebug.h"
#include "common.h"
#include "debug.h"
#include "string.h"

// Memory Settings: Let Slippi use 0x12B80000 - 0x12E80000
#define SlipMemClear() memset32(SlipMem, 0, SlipMemSize)
u8 *SlipMem = (u8 *)0x12B80000;
u32 SlipMemSize = 0x00300000;
volatile u64 SlipMemCursor = 0x0000000000000000;

u16 getPayloadSize(SlpGameReader *reader, u8 command);
void setPayloadSizes(SlpGameReader *reader, u32 readPos);
void resetMetadata(SlpGameReader *reader);
void updateMetadata(SlpGameReader *reader, u8 *message, u32 messageLength);

/* This should only be dispatched once in kernel/main.c after NCDInit() has
 * actually brought up the networking stack and we have connectivity. */
void SlippiMemoryInit()
{
	SlipMemClear();
}

void SlippiMemoryWrite(const u8 *buf, u32 len)
{
	u32 normalizedCursor = SlipMemCursor % SlipMemSize;

	// make sure we read through
	sync_before_read((void *)buf, len);

	// Handle overflow logic. Once we are going to overflow, wrap around to start
	// of memory region
	if ((normalizedCursor + len) > SlipMemSize)
	{


		// First, fill out the remaining memory
		u32 fillMemLen = SlipMemSize - normalizedCursor;
		memcpy(&SlipMem[normalizedCursor], buf, fillMemLen);
		sync_after_write(&SlipMem[normalizedCursor], fillMemLen);

		// Second, write the rest that hasn't been written to the start
		memcpy(SlipMem, &buf[fillMemLen], len - fillMemLen);
		sync_after_write(SlipMem, (len-fillMemLen));

		SlipMemCursor += len;
		return;
	}

	memcpy(&SlipMem[normalizedCursor], buf, len);
	sync_after_write(&SlipMem[normalizedCursor], len);
	SlipMemCursor += len;
}

SlpMemError SlippiMemoryRead(SlpGameReader *reader, u8 *buf, u32 bufLen, u64 readPos)
{
	// Reset previous read result
	reader->lastReadResult.bytesRead = 0;
	reader->lastReadResult.isGameEnd = false;
	reader->lastReadResult.isNewGame = false;

	SlpMemError errCode = SLP_MEM_OK;

	// Return error code on overflow read
	u64 posDiff = SlipMemCursor - readPos;
	if (posDiff >= SlipMemSize)
		return SLP_READ_OVERFLOW;

	u32 bytesRead = 0;
	while (readPos != SlipMemCursor)
	{
		u32 normalizedReadPos = readPos % SlipMemSize;
		// dbgprintf("Read position: 0x%X\r\n", normalizedReadPos);

		u8 command = SlipMem[normalizedReadPos];

		// Special case handling: Unnexpected new game message - shouldn't happen
		if (bytesRead > 0 && command == SLP_CMD_RECEIVE_COMMANDS)
		{
			dbgprintf("WARN: Unnexpected new game message\r\n");
			ppc_msg("SLP UNEXP MSG\x00", 14);
			errCode = SLP_MEM_UNNEX_NG;
			break;
		}

		// Detect new file
		if (command == SLP_CMD_RECEIVE_COMMANDS)
		{
			reader->lastReadResult.isNewGame = true;
			resetMetadata(reader);
			setPayloadSizes(reader, normalizedReadPos);
			ppc_msg("SLP NEWGAME\x00", 12);
		}

		u16 payloadSize = getPayloadSize(reader, command);

		// Special case handling: Payload size not found for command - shouldn't happen
		if (payloadSize == 0)
		{
			dbgprintf("WARN: Payload size not detected. Read: %02X, ReadPos: %X\r\n", command, readPos);
			errCode = SLP_PL_MISSING;
			break;
		}

		u16 bytesToRead = payloadSize + 1;

		// Special case handling: buffer is full
		if ((bytesRead + bytesToRead) > bufLen)
			break;

		if ((normalizedReadPos + bytesToRead) > SlipMemSize)
		{
			// This is the overflow case, here we need to do two memcpy calls
			// dbgprintf("Read overflow detected...\r\n");
			// First, fill out the remaining memory
			u32 fillMemLen = SlipMemSize - normalizedReadPos;
			memcpy(&buf[bytesRead], &SlipMem[normalizedReadPos], fillMemLen);

			// Second, write the rest that hasn't been written to the start
			memcpy(&buf[bytesRead + fillMemLen], SlipMem, bytesToRead - fillMemLen);
		}
		else
		{
			// Copy payload data into buffer. We don't need to sync_after_write because
			// the memory will be read from the ARM side
			memcpy(&buf[bytesRead], &SlipMem[normalizedReadPos], bytesToRead);
		}

		// Handle updating metadata
		updateMetadata(reader, &buf[bytesRead], bytesToRead);

		// Increment both read positions
		readPos += bytesToRead;
		bytesRead += bytesToRead;

		// Special case handling: game end message processed
		if (command == SLP_CMD_RECEIVE_GAME_END)
		{
			ppc_msg("SLP GAME END\x00", 13);
			reader->lastReadResult.isGameEnd = true;
			break;
		}
	}

	reader->lastReadPos = readPos;
	reader->lastReadResult.bytesRead = bytesRead;

	return errCode;
}

u64 SlippiRestoreReadPos()
{
	return SlipMemCursor;
}

void resetMetadata(SlpGameReader *reader)
{
	// TODO: Test this
	SlpMetadata freshMetadata = {0};
	reader->metadata = freshMetadata;
}

void updateMetadata(SlpGameReader *reader, u8 *message, u32 messageLength)
{
	u8 command = message[0];
	if (messageLength <= 0 || command != SLP_CMD_RECEIVE_POST_FRAME_UPDATE)
	{
		// Only need to update if this is a post frame update
		return;
	}

	// Keep track of last frame
	reader->metadata.lastFrame = message[1] << 24 | message[2] << 16 | message[3] << 8 | message[4];

	// TODO: Add character usage
	// Keep track of character usage
	// u8 playerIndex = payload[5];
	// u8 internalCharacterId = payload[7];
	// if (!characterUsage.count(playerIndex) || !characterUsage[playerIndex].count(internalCharacterId)) {
	// 	characterUsage[playerIndex][internalCharacterId] = 0;
	// }
	// characterUsage[playerIndex][internalCharacterId] += 1;
}

void setPayloadSizes(SlpGameReader *reader, u32 readPos)
{
	// Clear previous payloadSizes
	memset(reader->payloadSizes, 0, PAYLOAD_SIZES_BUFFER_SIZE);

	u8 *payload = &SlipMem[readPos + 1];
	u8 length = payload[0];

	// Indicate the length of the SLP_CMD_RECEIVE_COMMANDS payload
	// Will always be index zero
	reader->payloadSizes[0] = length;

	int i = 1;
	while (i < length)
	{
		// Go through the receive commands payload and set up other commands
		u8 commandByte = payload[i];
		u16 commandPayloadSize = payload[i + 1] << 8 | payload[i + 2];
		reader->payloadSizes[commandByte - SLP_CMD_RECEIVE_COMMANDS] = commandPayloadSize;

		// dbgprintf("Index: 0x%02X, Size: 0x%02X\r\n", commandByte - SLP_CMD_RECEIVE_COMMANDS, commandPayloadSize);

		i += 3;
	}
}

u16 getPayloadSize(SlpGameReader *reader, u8 command)
{
	int payloadSizesIndex = command - SLP_CMD_RECEIVE_COMMANDS;
	if (payloadSizesIndex >= PAYLOAD_SIZES_BUFFER_SIZE || payloadSizesIndex < 0)
	{
		return 0;
	}

	return reader->payloadSizes[payloadSizesIndex];
}
