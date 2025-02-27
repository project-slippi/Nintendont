#include "SlippiFileWriter.h"
#include "SlippiMemory.h"
#include "alloc.h"
#include "debug.h"
#include "string.h"
#include "ff_utf8.h"
#include "net.h"

#include "Config.h"
#include "usbstorage.h"

// Game can transfer at most 784 bytes / frame
// That means 4704 bytes every 100 ms. Let's aim to handle
// double that, making our read buffer 10000 bytes
#define READ_BUF_SIZE 10000
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

char *generateFileName(bool isNewFile)
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

void writeHeader(FIL *file)
{
	u8 header[] = {'{', 'U', 3, 'r', 'a', 'w', '[', '$', 'U', '#', 'l', 0, 0, 0, 0};

	u32 wrote;
	f_write(file, header, sizeof(header), &wrote);
	f_sync(file);
}

void completeFile(FIL *file, SlpGameReader *reader, u32 writtenByteCount)
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
	memcpy(&footer[writePos], &reader->metadata.lastFrame, 4);
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
	u32 wrote;
	f_write(file, footer, writePos, &wrote);
	f_sync(file);

	f_lseek(file, 11);
	FRESULT fileWriteResult = f_write(file, &writtenByteCount, 4, &wrote);
	f_sync(file);
}

static u32 SlippiHandlerThread(void *arg)
{
	dbgprintf("Slippi Thread ID: %d\r\n", thread_get_id());

	static SlpGameReader reader;
	static u8 readBuf[READ_BUF_SIZE];
	static u64 memReadPos = 0;

	u32 writtenByteCount = 0;
	driveTimer = read32(HW_TIMER);
	driveTimerSet = false;

	bool failedToMount = false;
	bool hasFile = false;
	const bool use_usb = ConfigGetUseUSB() != 1;
	bool mounted = use_usb ? USBStorage_IsInserted_SlippiThread() : true;

	while (1)
	{
		// Cycle time, look at const definition for more info
		mdelay(THREAD_CYCLE_TIME_MS);

		if (use_usb)
		{
			if (!USBStorage_IsInserted_SlippiThread())
			{
				if (mounted)
					f_mount_char(NULL, "usb:", 1);

				failedToMount = false;
				hasFile = false;
				mounted = false;
				continue;
			}
			else if (!mounted && !failedToMount)
			{
				if (f_mount_char(devices[1], "usb:", 1) == FR_OK)
				{
					// ignore anything already in the buffer. users should not expect to record a
					// game if the usb device is inserted after game start.
					memReadPos = SlippiRestoreReadPos();

					mounted = true;
				}
				else
				{
					// only attempt to mount once, user can retry by re-inserting the device.
					failedToMount = true;
				}
			}
			if (!mounted)
				continue;
		}

		// Read from memory and write to file
		SlpMemError err = SlippiMemoryRead(&reader, readBuf, READ_BUF_SIZE, memReadPos);
		if (err)
		{
			if (err == SLP_READ_OVERFLOW)
				memReadPos = SlippiRestoreReadPos();
				
			mdelay(LED_FLASH_TIME_MS + 1000); // we always want LED visibly off if this happens
			
			// For specific errors, bytes will still be read. Not continueing to deal with those
		}

		if (reader.lastReadResult.isNewGame)
		{
			// Create folder if it doesn't exist yet
			f_mkdir_secondary_drive("/Slippi");

			gameStartTime = GetCurrentTime();

			dbgprintf("Creating File...\r\n");
			char *fileName = generateFileName(true);
			// Maybe can remove FA_READ since network thread doesn't share &currentFile
			FRESULT fileOpenResult = f_open_secondary_drive(&currentFile, fileName, FA_CREATE_ALWAYS | FA_WRITE | FA_READ);
			if (fileOpenResult != FR_OK)
			{
				dbgprintf("Slippi: failed to open file: %s, errno: %d\r\n", fileName, fileOpenResult);
				mdelay(LED_FLASH_TIME_MS - THREAD_CYCLE_TIME_MS - 100); // short enough so we can recover with running out of LED time.
				continue;
			}
			if (replaysLED)
				flashLED();

			hasFile = true;
			writtenByteCount = 0;
			writeHeader(&currentFile);
		}

		if (reader.lastReadResult.bytesRead == 0)
		{
			if (replaysLED)
				flashLED();
			continue;
		}

		// dbgprintf("Bytes read: %d\r\n", reader.lastReadResult.bytesRead);

		if (!hasFile)
		{
			// we can reach this state if the user inserts a usb device during a game.
			// skip over and don't write anything until we see the start of a new game
			if (replaysLED)
				flashLED();
			memReadPos += reader.lastReadResult.bytesRead;
			continue;
		}

		UINT wrote;
		FRESULT writeResult = f_write(&currentFile, readBuf, reader.lastReadResult.bytesRead, &wrote);
		if (replaysLED && writeResult == FR_OK && wrote > 0)
			flashLED();
		f_sync(&currentFile);

		if (wrote == 0)
			continue;

		// Only increment mem read position when the data is correctly written
		memReadPos += wrote;
		writtenByteCount += wrote;

		if (reader.lastReadResult.isGameEnd)
		{
			dbgprintf("Completing File...\r\n");
			completeFile(&currentFile, &reader, writtenByteCount);
			f_close(&currentFile);
			hasFile = false;
		}
	}

	return 0;
}
