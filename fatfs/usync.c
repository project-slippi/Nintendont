/*****************************************************************************
 *  µSync, a small synchronization function set.                             *
 *  Copyright (C)2009 SquidMan (Alex Marshall)       <SquidMan72@gmail.com>  *
 *****************************************************************************/

#include "usync.h"
#if !defined(BM_BROADWAY)
#include <stdio.h>
#include <stdlib.h>
#endif
#if defined(BM_BROADWAY)
#include <malloc.h>
#endif

#include <string.h>

static int uSyncEnabled = 0;

#define _compare(a, b, size)	(!memcmp(a, b, size))
#define _copy(a, b, size)	(memcpy(a, b, size))
#define _clear(a, size)		(memset(a, 0, size))

static int validate_usync_object(uSyncObject* obj)
{
	if(_compare(obj->enabled, "uSYNC", 5)) {
		return 1;
	}
	return 0;
}

static void enable_usync_object(uSyncObject* obj)
{
	_copy(obj->enabled, "uSYNC", 5);
}

int uSyncInit()
{
	uSyncEnabled = 1;
	return uSyncEnabled;
}

#ifdef _USYNC_HOLD_DATA
uSyncObject* uSyncCreate(void* data)
#else
uSyncObject* uSyncCreate()
#endif
{
	uSyncObject* obj;
	if(!uSyncEnabled)
		return 0;
	obj = malloc(sizeof(uSyncObject));
	if(obj == 0)
		return 0;
	_clear(obj, sizeof(uSyncObject));
	obj->enabled[5] = 0;
#ifdef _USYNC_HOLD_DATA
	obj->data = data;
#endif
	enable_usync_object(obj);
	return obj;
}

int uSyncDelete(uSyncObject* obj)
{
	if(!uSyncEnabled)
		return 0;
	_clear(obj->enabled, 8);
#ifdef _USYNC_HOLD_DATA
	obj->data = 0;
#endif
	free(obj);
	return 1;
}

#ifdef _USYNC_HOLD_DATA
void* uSyncObtain(uSyncObject* obj)
#else
int uSyncObtain(uSyncObject* obj)
#endif
{
	int canhas;
	if(!uSyncEnabled)
		return 0;
	if(!validate_usync_object(obj))
		return 0;
	if(obj->enabled[5]) {
		return 0;
	}else{
		obj->enabled[5] ^= 1;		/* If 2 processes get here at once,     */
		canhas = obj->enabled[5];	/* worst case scenario, neither get it. */
#ifdef _USYNC_HOLD_DATA
		return (void*)(((unsigned long int)obj->data) * canhas);
#else
		return 1 * canhas;
#endif
	}
}

int uSyncRelease(uSyncObject* obj)
{
	if(!uSyncEnabled)
		return 0;
	if(!validate_usync_object(obj))
		return 0;
	if(obj->enabled[5]) {
		obj->enabled[5] = 0;
		return 1;
	}else{
		return 1;
	}
}
