#ifndef _SLIPPI_H
#define _SLIPPI_H

/* These header files *MUST* match across these projects:
 *
 *	project-slippi/slippi-wiiconf/source/Slippi.h
 *	project-slippi/Nintendont/common/include/Slippi.h
 *
 * If you update one, make sure you update the other!
 */

#define SLIPPI_DAT_FILE		"/slippi_console.dat"
#define SD_SLIPPI_DAT_FILE	"sd:/slippi_console.dat"
#define USB_SLIPPI_DAT_FILE	"usb:/slippi_console.dat"

/* struct slippi_settings
 * rtc_bias - 32-bits, "difference between RTC and local time" in seconds.
 *	      Specifically, the number of seconds since 1/1/2000 00:00:00.
 * nickname - 32 byte, user-configurable nickname for this console.
 *	      
 */

struct slippi_settings {

	// 32-bit, "difference between RTC and local time" in seconds.
	// This is the number of seconds since 1/1/2000 00:00:00.
	u32 rtc_bias;

	// 32-byte, user-configurable nickname for this console
	char nickname[32];
};

#endif // _SLIPPI_H
