/*****************************************************************************
 *  µSync, a small synchronization function set.                             *
 *  Copyright (C)2009 SquidMan (Alex Marshall)       <SquidMan72@gmail.com>  *
 *****************************************************************************/

#ifndef _USYNC_H
#define _USYNC_H

//#define _USYNC_HOLD_DATA

#ifdef __cplusplus
extern "C" {
#endif

/* We only use 8 bytes per sync object without holding data. */
/* Only 12bytes with (on a 32bit system that doesn't raise sizes to align) */
/* Requires malloc, free, memcmp, memcpy, and memset. */

typedef struct {
	unsigned char	enabled[8]; /* uSYNC#L_ */
#ifdef _USYNC_HOLD_DATA
	void*		data;
#endif
} uSyncObject;

int uSyncInit();
#ifdef _USYNC_HOLD_DATA
uSyncObject* uSyncCreate(void* data);
#else
uSyncObject* uSyncCreate();
#endif
int uSyncDelete(uSyncObject* obj);
#ifdef _USYNC_HOLD_DATA
void* uSyncObtain(uSyncObject* obj);
#else
int uSyncObtain(uSyncObject* obj);
#endif
int uSyncRelease(uSyncObject* obj);

#ifdef __cplusplus
};
#endif

#endif

