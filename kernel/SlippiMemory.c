/* SlippiMemory.c
 * Defines a circular buffer ('SlipMem') used to store replay data until it
 * has been consumed, or until it has been overwritten with new replay data.
 * Functions for managing the buffer should live in this file.
 *
 *	Current `SlipMem` memory region: 0x12B80000 - 0x12E80000
 */

#include "SlippiMemory.h"
#include "common.h"
#include "debug.h"
#include "string.h"

// Dimensions for the circular buffer
#define SLIPMEM_SIZE	0x00300000
#define SLIPMEM_BASE	0x12b80000
#define SLIPMEM_TAIL	(SLIPMEM_HEAD + SLIPMEM_SIZE)
#define SlipMemClear()	memset32(SlipMem, 0, SLIPMEM_SIZE)

// Global state: pointer to the buffer, and a write cursor
u8 *SlipMem = (u8*)SLIPMEM_BASE;
volatile u64 SlipMemCursor = 0x0000000000000000;

// Global state: the current state of recording
struct recordingState gameState ALIGNED(32);

u16 getPayloadSize(SlpGameReader *reader, u8 command);
void setPayloadSizes(SlpGameReader *reader, u32 readPos);
void resetMetadata(SlpGameReader *reader);
void updateMetadata(SlpGameReader *reader, u8 *message, u32 messageLength);


/* SlippiMemoryInit()
 * Zero out the buffer during boot - called from kernel/main.c
 */
void SlippiMemoryInit() { 
	SlipMemClear(); 
	memset(&gameState, 0, sizeof(struct recordingState));
}


/* SlippiMemoryWrite()
 * Put some data onto the circular buffer.
 *
 * In order to manage session state for remote clients, we need to keep track
 * of when a particular game is currently being recorded.
 */
void SlippiMemoryWrite(const u8 *buf, u32 len)
{
	u32 normalizedCursor = SlipMemCursor % SLIPMEM_SIZE;

	if ((normalizedCursor + len) > SLIPMEM_SIZE)
	{
		// Wrap around the buffer if the write would overflow
		u32 fillMemLen = SLIPMEM_SIZE - normalizedCursor;
		memcpy(&SlipMem[normalizedCursor], buf, fillMemLen);
		memcpy(SlipMem, &buf[fillMemLen], len - fillMemLen);
	}
	else
	{
		// Otherwise, just write directly into the buffer
		memcpy(&SlipMem[normalizedCursor], buf, len);
	}

	// Keep track of individual matches by checking commands
	u8 command = SlipMem[normalizedCursor];
	if (command == SLP_CMD_RECEIVE_COMMANDS)
	{
		gameState.baseCursor = SlipMemCursor;
		gameState.inGame = true;
		dbgprintf("Match %08x started at baseCursor=0x%08x\r\n", 
				gameState.matchID, (u32)gameState.baseCursor);
	}
	if (command == SLP_CMD_RECEIVE_GAME_END)
	{
		gameState.inGame = false;
		gameState.matchID += 1;
	}

	// Always increment the global write cursor
	SlipMemCursor += len;
}


/* SlippiMemoryRead()
 * Consume some data from the circular buffer.
 *
 * This will continuously reads some Slippi commands from the buffer until:
 *	(a) Our read cursor catches up to the global write cursor
 *	(b) We've read a GAME_END message (there are no more messages)
 *	(c) We are going to overflow the user-provided buffer (`buf`)
 *	(d) We need to return an error because something unexpected happened
 */
SlpMemError SlippiMemoryRead(SlpGameReader *reader, u8 *buf, u32 bufLen, u64 readPos)
{
	// Reset previous read result
	reader->lastReadResult.bytesRead = 0;
	reader->lastReadResult.isGameEnd = false;
	reader->lastReadResult.isNewGame = false;

	// Return an error code if the read will overflow
	u64 posDiff = SlipMemCursor - readPos;
	if (posDiff >= SLIPMEM_SIZE)
		return SLP_READ_OVERFLOW;

	u32 bytesRead = 0;
	SlpMemError errCode = SLP_MEM_OK;

	// Only read messages if we're behind the global write cursor
	while (readPos != SlipMemCursor)
	{
		// Update our read cursor and read the current command
		u32 normalizedReadPos = readPos % SLIPMEM_SIZE;
		u8 command = SlipMem[normalizedReadPos];

		// Throw an error if RECEIVE_COMMANDS is received out-of-order
		if (bytesRead > 0 && command == SLP_CMD_RECEIVE_COMMANDS)
		{
			dbgprintf("WARN: Unnexpected new game message\r\n");
			errCode = SLP_MEM_UNNEX_NG;
			break;
		}

		// Detect a new recording session; setup payload sizes
		if (command == SLP_CMD_RECEIVE_COMMANDS)
		{
			reader->lastReadResult.isNewGame = true;
			resetMetadata(reader);
			setPayloadSizes(reader, normalizedReadPos);
		}

		// Look up the size of the current command
		u16 payloadSize = getPayloadSize(reader, command);

		// Stop reading and return an error if we can't get the size
		if (payloadSize == 0)
		{
			dbgprintf("WARN: Payload size not detected. Read: %02X, ReadPos: %X\r\n", command, readPos);
			errCode = SLP_PL_MISSING;
			break;
		}

		// Calculate the number of bytes to read from the buffer
		u16 bytesToRead = payloadSize + 1;

		// Stop reading if we are going to overflow the user's buffer
		if ((bytesRead + bytesToRead) > bufLen)
			break;

		// Wrap around the buffer if the read would overflow
		if ((normalizedReadPos + bytesToRead) > SLIPMEM_SIZE)
		{
			u32 fillMemLen = SLIPMEM_SIZE - normalizedReadPos;
			memcpy(&buf[bytesRead], &SlipMem[normalizedReadPos], fillMemLen);
			memcpy(&buf[bytesRead + fillMemLen], SlipMem, bytesToRead - fillMemLen);
		}
		// Otherwise, just read directly from the buffer
		else
			memcpy(&buf[bytesRead], &SlipMem[normalizedReadPos], bytesToRead);

		// If this is a POST_FRAME message, update the replay metadata
		updateMetadata(reader, &buf[bytesRead], bytesToRead);

		// Increment both read positions
		readPos += bytesToRead;
		bytesRead += bytesToRead;

		// Stop reading if we've processed GAME_END (the last message)
		if (command == SLP_CMD_RECEIVE_GAME_END)
		{
			reader->lastReadResult.isGameEnd = true;
			break;
		}
	}

	// Save the read cursor and number of bytes we've read
	reader->lastReadPos = readPos;
	reader->lastReadResult.bytesRead = bytesRead;

	return errCode;
}


/* SlippiRestoreReadPos()
 * Returns the current position of the global write cursor.
 */
u64 SlippiRestoreReadPos() { return SlipMemCursor; }


/* resetMetadata()
 * Clear out the current metadata.
 */
void resetMetadata(SlpGameReader *reader)
{
	// TODO: Test this
	SlpMetadata freshMetadata = {0};
	reader->metadata = freshMetadata;
}


/* updateMetadata()
 * Update the latest frame number in metadata after a POST_FRAME message.
 */
void updateMetadata(SlpGameReader *reader, u8 *message, u32 messageLength)
{
	u8 command = message[0];

	// Only need to update if this is a post frame update
	if (messageLength <= 0 || command != SLP_CMD_RECEIVE_POST_FRAME_UPDATE)
		return;

	// Keep track of last frame
	reader->metadata.lastFrame = message[1] << 24 | message[2] << 16 |
				     message[3] << 8  | message[4];

	// TODO: Add character usage
	// Keep track of character usage
	// u8 playerIndex = payload[5];
	// u8 internalCharacterId = payload[7];
	// if (!characterUsage.count(playerIndex) || !characterUsage[playerIndex].count(internalCharacterId)) {
	// 	characterUsage[playerIndex][internalCharacterId] = 0;
	// }
	// characterUsage[playerIndex][internalCharacterId] += 1;
}


/* setPayloadSizes()
 * Process a RECEIVE_COMMANDS message and set the payload sizes.
 */
void setPayloadSizes(SlpGameReader *reader, u32 readPos)
{
	// Clear previous payloadSizes
	memset(reader->payloadSizes, 0, PAYLOAD_SIZES_BUFFER_SIZE);

	u8 *payload = &SlipMem[readPos + 1];
	u8 length = payload[0];

	// Indicate the length of the SLP_CMD_RECEIVE_COMMANDS payload
	reader->payloadSizes[SLP_CMD_RECEIVE_COMMANDS] = length;

	// Each structure is three bytes (u8 command, u16 size)
	int i = 1;
	while (i < length)
	{
		// Go through the receive commands payload and set up other commands
		u8 commandByte = payload[i];
		u16 commandPayloadSize = payload[i + 1] << 8 | payload[i + 2];
		reader->payloadSizes[commandByte] = commandPayloadSize;

		// dbgprintf("Index: 0x%02X, Size: 0x%02X\r\n",
		//	commandByte - SLP_CMD_RECEIVE_COMMANDS, commandPayloadSize);

		i += 3;
	}
}


/* getPayloadSize()
 * Return the size of a particular command.
 */
u16 getPayloadSize(SlpGameReader *reader, u8 command)
{
	int payloadSizesIndex = command;
	if (payloadSizesIndex >= PAYLOAD_SIZES_BUFFER_SIZE || payloadSizesIndex < 0)
		return 0;

	return reader->payloadSizes[payloadSizesIndex];
}
