#include "ReplayLog.h"
#include "common.h"
#include "debug.h"
#include "ff_utf8.h"
#include "string.h"
#include "vsprintf.h"
#include "../common/include/NintendontVersion.h"

#define LOG_PATH "/slippi_replays.log"

// Fixed slots so the Slippi thread never allocates or waits
#define LOG_SLOTS 32
#define LOG_LINE_LENGTH 92

// A log left over from earlier sessions is restarted past this size
#define LOG_MAX_FILE_SIZE (1024 * 1024)
// A drive failing for hours cannot fill the SD card
#define LOG_MAX_SESSION_BYTES (256 * 1024)

#define LOG_FLUSH_INTERVAL_SECS 1

typedef struct
{
	u32 tick;
	char text[LOG_LINE_LENGTH];
} LogLine;

// Single producer (Slippi thread), single consumer (main thread)
static LogLine lines[LOG_SLOTS];
static volatile u32 head = 0;
static volatile u32 tail = 0;
static volatile u32 dropped = 0;
static u32 reportedDropped = 0;

static bool active = false;
static FIL logFile;
static u32 sessionBytes = 0;
static u32 lastFlush = 0;

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

#define barrier() asm volatile("" ::: "memory")

void ReplayLogInit(void)
{
	FRESULT res = f_open_main_drive(&logFile, LOG_PATH, FA_OPEN_ALWAYS | FA_WRITE);
	if (res == FR_OK && f_size(&logFile) > LOG_MAX_FILE_SIZE)
	{
		f_close(&logFile);
		res = f_open_main_drive(&logFile, LOG_PATH, FA_CREATE_ALWAYS | FA_WRITE);
	}
	if (res != FR_OK)
	{
		dbgprintf("ReplayLog: failed to open " LOG_PATH ", errno: %d\r\n", res);
		return;
	}

	f_lseek(&logFile, f_size(&logFile));
	lastFlush = read32(HW_TIMER);
	active = true;
	ReplayLog("--- Slippi Nintendont %s (%s) ---", NIN_VERSION, NIN_GIT_VERSION);
}

void ReplayLog(const char *fmt, ...)
{
	char buffer[0x100];
	va_list args;
	va_start(args, fmt);
	_vsprintf(buffer, fmt, args);
	va_end(args);

	dbgprintf("%s\r\n", buffer);

	if (!active)
		return;

	u32 next = (head + 1) % LOG_SLOTS;
	if (next == tail)
	{
		dropped++;
		return;
	}

	lines[head].tick = read32(HW_TIMER);
	strncpy(lines[head].text, buffer, LOG_LINE_LENGTH - 1);
	lines[head].text[LOG_LINE_LENGTH - 1] = 0;
	barrier();
	head = next;
}

bool ReplayLogPending(void)
{
	return active && head != tail && TimerDiffSeconds(lastFlush) >= LOG_FLUSH_INTERVAL_SECS;
}

static void writeLine(const char *line)
{
	UINT wrote;
	u32 len = strlen(line);
	f_write(&logFile, line, len, &wrote);
	sessionBytes += len;
}

void ReplayLogFlush(void)
{
	char buffer[LOG_LINE_LENGTH + 32];
	u32 now = GetCurrentTime();

	while (tail != head && sessionBytes < LOG_MAX_SESSION_BYTES)
	{
		barrier();
		LogLine *line = &lines[tail];
		u32 when = now - TicksToSecs(read32(HW_TIMER) - line->tick);
		struct tm *t = gmtime(&when);
		_sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d %s\r\n",
			t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
			t->tm_hour, t->tm_min, t->tm_sec, line->text);
		writeLine(buffer);
		barrier();
		tail = (tail + 1) % LOG_SLOTS;
	}

	u32 droppedNow = dropped;
	if (droppedNow != reportedDropped)
	{
		_sprintf(buffer, "%u lines dropped\r\n", droppedNow - reportedDropped);
		reportedDropped = droppedNow;
		writeLine(buffer);
	}

	if (sessionBytes >= LOG_MAX_SESSION_BYTES)
	{
		writeLine("log limit reached for this session\r\n");
		active = false;
	}

	f_sync(&logFile);
	lastFlush = read32(HW_TIMER);
}
