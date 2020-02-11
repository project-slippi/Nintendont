#ifndef __SLIPPI_FILE_WRITER_H__
#define __SLIPPI_FILE_WRITER_H__

#include "global.h"
#include "ff_utf8.h"

void SlippiFileWriterInit(FATFS *device, WCHAR *name);
void SlippiFileWriterShutdown();

#endif
