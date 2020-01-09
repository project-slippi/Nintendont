/*

Nintendont (Loader) - Playing Gamecubes in Wii mode on a Wii U

Copyright (C) 2013  crediar

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation version 2.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/
#include "global.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ogc/usbgecko.h>

#define	EXI_BASE	0xCD006800
#define EXI			0xCD006814

#if 0
static bool wiiu_done = false;
#endif
// FIXME: Re-enable Wii U file logging after testing FatFs.
//static FILE *nl_log = NULL;
static u32 GeckoFound = 0;

void CheckForGecko( void ) { GeckoFound = usb_isgeckoalive( 1 ); }

void EXISendByte( char byte )
{

loop:
	*(vu32*)EXI			= 0xD0;
	*(vu32*)(EXI+0x10)	= 0xB0000000 | (byte<<20);
	*(vu32*)(EXI+0x0C)	= 0x19;

	while( *(vu32*)(EXI+0x0C)&1 );

	u32 loop =  *(vu32*)(EXI+0x10)&0x4000000;
	
	*(vu32*)EXI	= 0;

	if( !loop )
		goto loop;

	return;
}

/**
 * Log a debug message.
 * - Wii: Logs to USB Gecko.
 * - Wii U: Logs to a file on the root device.
 * @param str printf-style format string.
 * @param ... printf arguments.
 * @return vsnprintf() return value; 0 if nothing was written; or -1 on error.
 */
int gprintf( const char *str, ... )
{
	int ret = 0;

	// No USB Gecko found
	if (!GeckoFound) return 0;

	char astr[4096];
	va_list ap;
	va_start(ap, str);
	ret = vsnprintf(astr, sizeof(astr), str, ap);
	va_end(ap);

	// Send the string.
	const char *p = astr;
	for (; *p != '\0'; p++)
	{
		EXISendByte(*p);
	}

	// Everything went okay
	return ret;
}

void closeLog(void)
{
	// FIXME: Re-enable Wii U file logging after testing FatFs.
#if 0
	wiiu_done = true;

	if(nl_log != NULL)
		fclose(nl_log);
	nl_log = NULL;
#endif
}
