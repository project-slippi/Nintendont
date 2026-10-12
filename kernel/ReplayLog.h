#ifndef __REPLAY_LOG_H__
#define __REPLAY_LOG_H__

#include "global.h"

// Records USB drive and replay writer errors to sd:/slippi_replays.log so a
// failure at an event leaves something to attach to a bug report. Only
// active when replays go to USB and the game runs from SD.

// Main thread, after the SD card is mounted
void ReplayLogInit(void);

// Slippi thread. Lines are queued and written later by the main thread,
// since FatFs is not thread safe and the main thread owns the SD card.
void ReplayLog(const char *fmt, ...);

// Main thread, only while no disc read is in flight
bool ReplayLogPending(void);
void ReplayLogFlush(void);

// Main thread, only while no disc read is in flight. Writes straight to the
// file, for reports about the Slippi thread while it is stuck.
void ReplayLogMainThread(const char *fmt, ...);

#endif
