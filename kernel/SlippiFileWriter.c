#include "SlippiFileWriter.h"
#include "SlippiMemory.h"
#include "alloc.h"
#include "debug.h"
#include "string.h"
#include "ff_utf8.h"
#include "net.h"

#include "Config.h"
#include "usbstorage.h"
#include "ReplayLog.h"

// use common physical sector size so as to write efficiently
// and not excessively wear out the underlying flash storage
#define READ_BUF_SIZE 4096

#define THREAD_CYCLE_TIME_MS 100
#define THREAD_ERROR_TIME_MS 2000
#define LED_FLASH_TIME_MS 1000

#define FOOTER_BUFFER_LENGTH 200

static u32 SlippiHandlerThread(void *arg);

// Thread stuff
static u32 Slippi_Thread;
extern char __slippi_stack_addr, __slippi_stack_size;

// File writing stuff
extern u8 wifi_mac_address[6]; // Used to identify replays

// File object
FIL currentFile;

// A previous replay that could not be finished when the next game started.
// It is retried in the background so the next game can record meanwhile.
FIL parkedFile;

// vars for metadata generation
u32 gameStartTime;

// timer for drive led
u32 driveTimer;

// flag for drive led timer
bool driveTimerSet;

// replays LED setting
bool replaysLED;

extern FATFS *devices[2];

void SlippiFileWriterInit(bool led)
{
	replaysLED = led;

	Slippi_Thread = do_thread_create(
		SlippiHandlerThread,
		((u32 *)&__slippi_stack_addr),
		((u32)(&__slippi_stack_size)),
		0x78);
	thread_continue(Slippi_Thread);
}

void SlippiFileWriterUpdateRegisters()
{
	if (driveTimerSet && TimerDiffMs(driveTimer) >= LED_FLASH_TIME_MS)
	{
		clear32(HW_GPIO_OUT, GPIO_SLOT_LED);
		driveTimerSet = false;
	}
}

void SlippiFileWriterShutdown()
{
	thread_cancel(Slippi_Thread, 0);
}

void flashLED()
{
	driveTimer = read32(HW_TIMER);
	if (!driveTimerSet)
	{
		set32(HW_GPIO_OUT, GPIO_SLOT_LED);
		driveTimerSet = true;
	}
}

//we cant include time.h so hardcode what we need
struct tm
{
	int tm_sec;
	int tm_min;
	int tm_hour;
	int tm_mday;
	int tm_mon;
	int tm_year;
};
extern struct tm *gmtime(u32 *time);

char *generateFileName()
{
	// // Add game start time
	// u8 dateTimeStrLength = sizeof "20171015T095717";
	// char *dateTimeBuf = (char *)malloc(dateTimeStrLength);
	// strftime(&dateTimeBuf[0], dateTimeStrLength, "%Y%m%dT%H%M%S", localtime(&gameStartTime));

	// std::string str(&dateTimeBuf[0]);
	// return StringFromFormat("Slippi/Game_%s.slp", str.c_str());

	static char pathStr[50];
	struct tm *tmp = gmtime(&gameStartTime);

	_sprintf(
		&pathStr[0], "/Slippi/Game_%02X%02X%02X%02X%02X%02X_%04d%02d%02dT%02d%02d%02d.slp",
		wifi_mac_address[0], wifi_mac_address[1], wifi_mac_address[2], wifi_mac_address[3],
		wifi_mac_address[4], wifi_mac_address[5], tmp->tm_year + 1900, tmp->tm_mon + 1,
		tmp->tm_mday, tmp->tm_hour, tmp->tm_min, tmp->tm_sec);

	return pathStr;
}

FRESULT writeHeader(FIL *file)
{
	u8 header[] = {'{', 'U', 3, 'r', 'a', 'w', '[', '$', 'U', '#', 'l', 0, 0, 0, 0};

	u32 wrote;
	return f_write(file, header, sizeof(header), &wrote);
}

// FatFs latches the first disk error in fp->err and fails every later
// f_lseek/f_write on that handle, so without this a single failed USB transfer
// ends the replay and blocks every game after it. Rewinding first makes
// f_lseek rebuild the current cluster from the chain, since an aborted f_write
// can leave fp->clust ahead of fp->fptr.
static FRESULT seekFile(FIL *file, FSIZE_t ofs)
{
	if (file->err)
	{
		file->err = 0;
		f_lseek(file, 0);
	}
	return f_lseek(file, ofs);
}

FRESULT completeFile(FIL *file, s32 lastFrame, u32 writtenByteCount)
{
	u8 footer[FOOTER_BUFFER_LENGTH];
	u32 writePos = 0;

	// Write opener
	u8 footerOpener[] = {'U', 8, 'm', 'e', 't', 'a', 'd', 'a', 't', 'a', '{'};
	u8 writeLen = sizeof(footerOpener);
	memcpy(&footer[writePos], footerOpener, writeLen);
	writePos += writeLen;

	// Write startAt
	// TODO: Figure out how to specify time zone
	char timeStr[] = "2011-10-08T07:07:09";
	int timeStrLen = strlen(timeStr);
	struct tm *tmp = gmtime(&gameStartTime);
	_sprintf(
		&timeStr[0], "%04d-%02d-%02dT%02d:%02d:%02d", tmp->tm_year + 1900,
		tmp->tm_mon + 1, tmp->tm_mday, tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
	u8 startAtOpener[] = {'U', 7, 's', 't', 'a', 'r', 't', 'A', 't', 'S', 'U', (u8)timeStrLen};
	writeLen = sizeof(startAtOpener);
	memcpy(&footer[writePos], startAtOpener, writeLen);
	writePos += writeLen;
	writeLen = timeStrLen;
	memcpy(&footer[writePos], timeStr, writeLen);
	writePos += writeLen;

	// Write lastFrame
	u8 lastFrameOpener[] = {'U', 9, 'l', 'a', 's', 't', 'F', 'r', 'a', 'm', 'e', 'l'};
	writeLen = sizeof(lastFrameOpener);
	memcpy(&footer[writePos], lastFrameOpener, writeLen);
	writePos += writeLen;
	memcpy(&footer[writePos], &lastFrame, 4);
	writePos += 4;

	// Write console nickname
	u8 nickLen = strlen(SlippiGetConsoleNick());
	if (nickLen > 32) nickLen = 32;
	u8 consoleNickOpener[] = { 'U', 11, 'c', 'o', 'n', 's', 'o', 'l', 'e', 'N', 'i', 'c', 'k', 'S', 'U', nickLen };
	writeLen = sizeof(consoleNickOpener);
	memcpy(&footer[writePos], consoleNickOpener, writeLen);
	writePos += writeLen;
	memcpy(&footer[writePos], SlippiGetConsoleNick(), nickLen);
	writePos += nickLen;

	// Write closing
	u8 closing[] = {
		'U', 7, 'p', 'l', 'a', 'y', 'e', 'r', 's', '{', '}',
		'U', 8, 'p', 'l', 'a', 'y', 'e', 'd', 'O', 'n', 'S', 'U',
		10, 'n', 'i', 'n', 't', 'e', 'n', 'd', 'o', 'n', 't',
		'}', '}'};
	writeLen = sizeof(closing);
	memcpy(&footer[writePos], closing, writeLen);
	writePos += writeLen;

	// Write footer
	// Always seek first in case there was a previous failure with partial write
	FRESULT fRes = seekFile(file, writtenByteCount + 15);
	if (fRes != FR_OK)
	{
		dbgprintf("Slippi: failed to seek before writing footer, errno: %d\r\n", fRes);
		return fRes;
	}

	u32 wrote;
	fRes = f_write(file, footer, writePos, &wrote);
	if (fRes != FR_OK)
	{
		dbgprintf("Slippi: failed to write footer, errno: %d\r\n", fRes);
		return fRes;
	}

	// Write length
	fRes = seekFile(file, 11);
	if (fRes != FR_OK)
	{
		dbgprintf("Slippi: failed to seek before writing length, errno: %d\r\n", fRes);
		return fRes;
	}

	fRes = f_write(file, &writtenByteCount, 4, &wrote);
	if (fRes != FR_OK)
		dbgprintf("Slippi: failed to write length, errno: %d\r\n", fRes);
	
	return fRes;
}

// Writes the footer if the replay is valid, then closes it. Safe to retry.
static FRESULT finishFile(FIL *file, bool valid, s32 lastFrame, u32 writtenByteCount)
{
	FRESULT res = valid ? completeFile(file, lastFrame, writtenByteCount) : FR_OK;
	return res == FR_OK ? f_close(file) : res;
}

static u32 SlippiHandlerThread(void *arg)
{
	dbgprintf("Slippi Thread ID: %d\r\n", thread_get_id());

	static SlpGameReader reader;
	static u8 readBuf[READ_BUF_SIZE];
	static u64 memReadPos = 0;

	u32 writtenByteCount = 0;
	u32 lastGameStartTime = 0;
	u64 fileGamePos = ~0ULL;
	// Buffer position up to which every game has been saved, so a remount
	// does not record those games again
	u64 savedThrough = 0;
	s32 lastFrame;
	driveTimer = read32(HW_TIMER);
	driveTimerSet = false;

	bool failedToMount = false;
	bool parkedOpen = false;
	bool parkedValid = false;
	s32 parkedLastFrame = 0;
	u32 parkedByteCount = 0;
	u64 parkedEndPos = 0;
	u32 writeFailures = 0;
	u32 finishFailures = 0;
	bool gameEnded = false;
	bool currentFileOpen = false;
	bool currentFileValid = false;
	const bool use_usb = ConfigGetUseUSB() != 1;
	bool mounted = use_usb ? USBStorage_IsInserted_SlippiThread() : true;
	if (use_usb && mounted)
		USBStorage_LogBootDevice();

	while (1)
	{
		// Cycle time, look at const definition for more info
		mdelay(THREAD_CYCLE_TIME_MS);

		if (use_usb)
		{
			if (!USBStorage_IsInserted_SlippiThread())
			{
				if (mounted)
				{
					// unmount (cannot fail so no need to check return value)
					f_mount_char(NULL, "usb:", 1);
				}

				failedToMount = false;
				currentFileOpen = false;
				currentFileValid = false;
				gameEnded = false;
				parkedOpen = false;
				mounted = false;
				continue;
			}
			else if (!mounted && !failedToMount)
			{
				FRESULT mountResult = f_mount_char(devices[1], "usb:", 1);
				if (mountResult != FR_OK)
				{
					ReplayLog("replay: mounting the drive failed: %d", mountResult);

					// only attempt to mount once, user can retry by re-inserting the device.
					failedToMount = true;
					continue;
				}

				// Create folder if it doesn't exist yet
				FRESULT mkdirResult = f_mkdir_secondary_drive("/Slippi");
				if (mkdirResult != FR_OK && mkdirResult != FR_EXIST)
				{
					ReplayLog("replay: creating /Slippi failed: %d", mkdirResult);

					// only attempt to mount once, user can retry by re-inserting the device.
					failedToMount = true;
					continue;
				}

				// Go back to the oldest unsaved game whose start is still in the
				// buffer, so a game survives the drive being bumped out or inserted
				// late. A game that started too long ago is skipped.
				memReadPos = SlippiOldestGameStartFrom(savedThrough);
				mounted = true;
				u64 behind = SlippiRestoreReadPos() - memReadPos;
				if (behind > 0)
					ReplayLog("replay: drive mounted, recording from a game that started %u KB ago", (u32)(behind / 1024));
				else
					ReplayLog("replay: drive mounted");
			}
			if (!mounted)
				continue;
		}

		while (1)
		{
			if (parkedOpen && finishFile(&parkedFile, parkedValid, parkedLastFrame, parkedByteCount) == FR_OK)
			{
				ReplayLog("replay: saved the previous replay, %u bytes", parkedByteCount);
				parkedOpen = false;
				if (parkedEndPos > savedThrough)
					savedThrough = parkedEndPos;
			}

			// Finish a replay as soon as its game ends, retrying every cycle. The
			// last game of a set has no next game to trigger the dangling-file path.
			if (gameEnded)
			{
				FRESULT finishResult = finishFile(&currentFile, currentFileValid, lastFrame, writtenByteCount);
				if (finishResult == FR_OK)
				{
					ReplayLog("replay: saved, %u bytes", writtenByteCount);
					if (memReadPos > savedThrough)
						savedThrough = memReadPos;
					gameEnded = false;
					currentFileValid = false;
					currentFileOpen = false;
					finishFailures = 0;
					if (replaysLED)
						flashLED();
				}
				else if (finishFailures++ == 0)
				{
					ReplayLog("replay: finishing the replay failed: %d", finishResult);
				}
			}

			// Read from memory and write to file
			SlpMemError err = SlippiMemoryRead(&reader, readBuf, READ_BUF_SIZE, memReadPos);
			if (err)
			{
				memReadPos = SlippiRestoreReadPos();

				// A game that already ended keeps retrying its footer above; a
				// replay in the middle of a game can no longer be completed
				if (gameEnded)
					break;
				if (currentFileValid)
					ReplayLog("replay: lost the game data (%d) after %u bytes, replay left partial", err, writtenByteCount);
				currentFileValid = false;
				if (currentFileOpen)
				{
					FRESULT closeResult = f_close(&currentFile);
					if (closeResult != FR_OK)
					{
						dbgprintf("Slippi: failed to close incompletable file, errno: %d\r\n", closeResult);
					}
					else
					{
						currentFileOpen = false;
					}
				}
				break;
			}

			if (reader.lastReadResult.bytesAvailable < READ_BUF_SIZE && !(currentFileValid && reader.lastReadResult.isGameEnd))
			{
				if (replaysLED)
					flashLED();
				break;
			}

			if (reader.lastReadResult.isNewGame)
			{
				if (currentFileOpen)
				{
					FRESULT danglingResult = finishFile(&currentFile, currentFileValid, lastFrame, writtenByteCount);
					if (danglingResult == FR_OK)
					{
						if (currentFileValid)
							ReplayLog("replay: saved, %u bytes", writtenByteCount);
						if (memReadPos > savedThrough)
							savedThrough = memReadPos;
					}
					else if (!parkedOpen)
					{
						// FIL holds no buffer of its own (_FS_TINY), so a copy carries the
						// open file. Keep retrying it while the next game records.
						ReplayLog("replay: finishing the previous replay failed: %d, retrying in the background", danglingResult);
						parkedFile = currentFile;
						parkedOpen = true;
						parkedValid = currentFileValid;
						parkedLastFrame = lastFrame;
						parkedByteCount = writtenByteCount;
						parkedEndPos = memReadPos;
					}
					else
					{
						// Only during an outage longer than a whole game
						ReplayLog("replay: finishing the previous replay failed: %d, left partial", danglingResult);
					}
					finishFailures = 0;
					gameEnded = false;
					currentFileValid = false;
					currentFileOpen = false;
				}

				dbgprintf("Creating File...\r\n");
				// Names have one-second resolution, and catching up after a stall can
				// reach two games in the same second. FA_CREATE_ALWAYS would then
				// overwrite the first, so keep start times distinct. A retry for the
				// same game keeps its name so it replaces the failed attempt.
				if (memReadPos != fileGamePos)
				{
					gameStartTime = GetCurrentTime();
					if (gameStartTime <= lastGameStartTime)
						gameStartTime = lastGameStartTime + 1;
					lastGameStartTime = gameStartTime;
					fileGamePos = memReadPos;
				}
				char *fileName = generateFileName();
				// Maybe can remove FA_READ since network thread doesn't share &currentFile
				FRESULT fileOpenResult = f_open_secondary_drive(&currentFile, fileName, FA_CREATE_ALWAYS | FA_WRITE | FA_READ);
				if (fileOpenResult != FR_OK)
				{
					ReplayLog("replay: creating %s failed: %d", fileName, fileOpenResult);
					break;
				}

				currentFileOpen = true;
				writtenByteCount = 0;
				writeFailures = 0;
				ReplayLog("replay: recording %s", fileName);
				
				FRESULT writeHeaderResult = writeHeader(&currentFile);
				if (writeHeaderResult != FR_OK)
				{
					ReplayLog("replay: writing the header failed: %d", writeHeaderResult);
					break;
				}

				currentFileValid = true;
			}

			if (!currentFileValid)
			{
				// we can reach this state if we SlippiRestoreReadPos into 
				// the middle of a game due to usb insertion or SlpMemError
				// skip over and don't write anything until we see the start of a new game
				if (replaysLED)
					flashLED();
				memReadPos += reader.lastReadResult.bytesRead;
				break;
			}

			// Always seek first in case there was a previous failure with partial write
			UINT wrote;
			FRESULT writeResult = seekFile(&currentFile, writtenByteCount + 15);
			if (writeResult == FR_OK)
				writeResult = f_write(&currentFile, readBuf, reader.lastReadResult.bytesRead, &wrote);
			if (writeResult != FR_OK)
			{
				// Retried every cycle, so log the first failure and the recovery
				if (writeFailures++ == 0)
					ReplayLog("replay: write at byte %u failed: %d", writtenByteCount, writeResult);
				break;
			}
			else
			{
				if (writeFailures > 0)
				{
					ReplayLog("replay: writing resumed after %u retries", writeFailures);
					writeFailures = 0;
				}

				// Only increment mem read position when the write fully succeeds
				memReadPos += wrote;
				writtenByteCount += wrote;

				if (reader.lastReadResult.isGameEnd)
				{
					dbgprintf("Completing File...\r\n");
					lastFrame = reader.metadata.lastFrame;
					gameEnded = true;
					continue; // finished at the top of the loop
				}
				else if (replaysLED)
					flashLED();
			}
		}
	}

	return 0;
}
