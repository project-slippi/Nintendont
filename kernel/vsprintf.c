/*
 *  linux/lib/vsprintf.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

/* vsprintf.c -- Lars Wirzenius & Linus Torvalds. */
/*
 * Wirzenius wrote this portably, Torvalds fucked it up :-)
 */

#include "vsprintf.h"
#include "Config.h"
#include "ff_utf8.h"

#ifdef SLIPPI_DEBUG
#include "SlippiDebug.h"
#include "net.h"
extern s32 debug_sock;
extern s32 top_fd;
#endif

extern int svc_write(char *buffer);

// Global variable set in kernel/main.c after reading the config
extern vu32 sdhc_log_enabled;

// Handle to the log file on the SD card
static FIL sdhc_log;
static int sdhc_log_status = -1;

#define ZEROPAD	1		/* pad with zero */
#define SIGN	2		/* unsigned/signed long */
#define PLUS	4		/* show plus */
#define SPACE	8		/* space if plus */
#define LEFT	16		/* left justified */
#define SPECIAL	32		/* 0x */
#define LARGE	64		/* use 'ABCDEF' instead of 'abcdef' */

#define do_div(n,base) ({ \
	int __res; \
	__res = ((unsigned long) n) % (unsigned) base; \
	n = ((unsigned long) n) / (unsigned) base; \
	__res; \
})

static inline int isdigit(int c) { return c >= '0' && c <= '9'; }
static inline int islower(int c) { return c >= 'a' && c <= 'z'; }
static inline int isxdigit(int c)
{
	return (c >= '0' && c <= '9')
	    || (c >= 'a' && c <= 'f')
	    || (c >= 'A' && c <= 'F');
}
static inline int toupper(int c)
{
	if (islower(c))
		c -= 'a'-'A';
	return c;
}
static int skip_atoi(const char **s)
{
	int i=0;

	while (isdigit(**s))
		i = i*10 + *((*s)++) - '0';
	return i;
}


static char *number(char *str, long num, int base, int size, int precision, int type)
{
	char c,sign,tmp[66];
	const char *digits="0123456789abcdefghijklmnopqrstuvwxyz";
	int i;

	if (type & LARGE)
		digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	if (type & LEFT)
		type &= ~ZEROPAD;
	if (base < 2 || base > 36)
		return 0;
	c = (type & ZEROPAD) ? '0' : ' ';
	sign = 0;
	if (type & SIGN) {
		if (num < 0) {
			sign = '-';
			num = -num;
			size--;
		} else if (type & PLUS) {
			sign = '+';
			size--;
		} else if (type & SPACE) {
			sign = ' ';
			size--;
		}
	}
	if (type & SPECIAL) {
		if (base == 16)
			size -= 2;
		else if (base == 8)
			size--;
	}
	i = 0;
	if (num == 0)
		tmp[i++]='0';
	else while (num != 0)
		tmp[i++] = digits[do_div(num,base)];
	if (i > precision)
		precision = i;
	size -= precision;
	if (!(type&(ZEROPAD+LEFT)))
		while(size-->0)
			*str++ = ' ';
	if (sign)
		*str++ = sign;
	if (type & SPECIAL) {
		if (base==8)
			*str++ = '0';
		else if (base==16) {
			*str++ = '0';
			*str++ = digits[33];
		}
	}
	if (!(type & LEFT))
		while (size-- > 0)
			*str++ = c;
	while (i < precision--)
		*str++ = '0';
	while (i-- > 0)
		*str++ = tmp[i];
	while (size-- > 0)
		*str++ = ' ';
	return str;
}

int _vsprintf(char *buf, const char *fmt, va_list args)
{
	int len;
	unsigned long num;
	int i, base;
	char * str;
	const char *s;

	int flags;		/* flags to number() */

	int field_width;	/* width of output field */
	int precision;		/* min. # of digits for integers; max
				   number of chars for from string */
	int qualifier;		/* 'h', 'l', or 'L' for integer fields */

	for (str=buf ; *fmt ; ++fmt) {
		if (*fmt != '%') {
			*str++ = *fmt;
			continue;
		}

		/* process flags */
		flags = 0;
		repeat:
			++fmt;		/* this also skips first '%' */
			switch (*fmt) {
				case '-': flags |= LEFT; goto repeat;
				case '+': flags |= PLUS; goto repeat;
				case ' ': flags |= SPACE; goto repeat;
				case '#': flags |= SPECIAL; goto repeat;
				case '0': flags |= ZEROPAD; goto repeat;
				}

		/* get field width */
		field_width = -1;
		if (isdigit(*fmt))
			field_width = skip_atoi(&fmt);
		else if (*fmt == '*') {
			++fmt;
			/* it's the next argument */
			field_width = va_arg(args, int);
			if (field_width < 0) {
				field_width = -field_width;
				flags |= LEFT;
			}
		}

		/* get the precision */
		precision = -1;
		if (*fmt == '.') {
			++fmt;
			if (isdigit(*fmt))
				precision = skip_atoi(&fmt);
			else if (*fmt == '*') {
				++fmt;
				/* it's the next argument */
				precision = va_arg(args, int);
			}
			if (precision < 0)
				precision = 0;
		}

		/* get the conversion qualifier */
		qualifier = -1;
		if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L') {
			qualifier = *fmt;
			++fmt;
		}

		/* default base */
		base = 10;

		switch (*fmt) {
		case 'c':
			if (!(flags & LEFT))
				while (--field_width > 0)
					*str++ = ' ';
			*str++ = (unsigned char) va_arg(args, int);
			while (--field_width > 0)
				*str++ = ' ';
			continue;

		case 's':
			s = va_arg(args, char *);
			//if (!s)
			//	s = "<NULL>";

			len = strnlen(s, precision);

			if (!(flags & LEFT))
				while (len < field_width--)
					*str++ = ' ';
			for (i = 0; i < len; ++i)
				*str++ = *s++;
			while (len < field_width--)
				*str++ = ' ';
			continue;

		case 'p':
			if (field_width == -1) {
				field_width = 2*sizeof(void *);
				flags |= ZEROPAD;
			}
			str = number(str,
				(unsigned long) va_arg(args, void *), 16,
				field_width, precision, flags);
			continue;


		case 'n':
			if (qualifier == 'l') {
				long * ip = va_arg(args, long *);
				*ip = (str - buf);
			} else {
				int * ip = va_arg(args, int *);
				*ip = (str - buf);
			}
			continue;

		case '%':
			*str++ = '%';
			continue;

		/* integer number formats - set up the flags and "break" */
		case 'o':
			base = 8;
			break;

		case 'X':
			flags |= LARGE;
		case 'x':
			base = 16;
			break;

		case 'd':
		case 'i':
			flags |= SIGN;
		case 'u':
			break;

		default:
			*str++ = '%';
			if (*fmt)
				*str++ = *fmt;
			else
				--fmt;
			continue;
		}
		if (qualifier == 'l')
			num = va_arg(args, unsigned long);
		else if (qualifier == 'h') {
			num = (unsigned short) va_arg(args, int);
			if (flags & SIGN)
				num = (short) num;
		} else if (flags & SIGN)
			num = va_arg(args, int);
		else
			num = va_arg(args, unsigned int);
		str = number(str, num, base, field_width, precision, flags);
	}
	*str = '\0';
	return str-buf;
}



/* dbgprintf()
 * Emit a log message to some I/O device depending on the configuration.
 * Logging to a USB Gecko is ALWAYS enabled during the boot process.
 */
extern u32 early_gecko_logging; 
extern u32 slippi_use_port_a;
int dbgprintf( const char *fmt, ...)
{
	// If no logging is enabled, do nothing.
	bool enable = (sdhc_log_enabled || slippi_use_port_a || early_gecko_logging);
	if (enable == false)
		return 0;

	// Otherwise, regardless of HOW we're logging, we always need to call
	// _vsprintf() to render our message into this buffer on the stack.

	u32 num_bytes;	
	va_list args;
	char buffer[0x100];

	va_start(args, fmt);
	_vsprintf(buffer, fmt, args);
	va_end(args);

	// If the user is logging to a USB Gecko, verify that the EXI bit in
	// EXICTRL/SRNPROT is enabled, then do the semihosting write.

	if (early_gecko_logging || slippi_use_port_a)
	{
		if ((*(vu32*)(0x0d800070) & 1) == 1) 
			svc_write(buffer);
	}

	// Regardless of whether or not we're using SD/EXI for logging, 
	// conditionally compile in this code when SLIPPI_DEBUG is defined.
	// Note that the call will spend extra time on-CPU blocking for this.
	// A nicer solution would probably buffer up data and wait for another
	// thread to send the message (if timing isn't especially critical).

#ifdef SLIPPI_DEBUG
	if ((top_fd > 0) && (debug_sock > 0)) 
		sendto(top_fd, debug_sock, buffer, strlen(buffer), 0);
#endif

	// Deal with writes to SD card

	if ((sdhc_log_enabled == 1) && (sdhc_log_status == FR_OK))
	{
		f_lseek(&sdhc_log, sdhc_log.obj.objsize);
		f_write(&sdhc_log, buffer, strlen(buffer), &num_bytes);
		f_sync(&sdhc_log);
	}

	return 0;
}

/* sdhc_log_init()
 * Called from _main() on boot when logging to SD card is enabled.
 * We only do this once so, if something weird happens during runtime and
 * we can't write, we're probably SOL.
 */
int sdhc_log_init()
{
	sdhc_log_status = f_open_main_drive(&sdhc_log, 
		"/slippi_ndebug.log", FA_OPEN_ALWAYS | FA_WRITE);

	if (sdhc_log_status != FR_OK)
		return -1;

	// Write a header to the log file
	u32 v = read32(0x3140);
	dbgprintf("Nintendont IOS%d v%d.%d\r\n", 
		v >> 16, (v >> 8) & 0xff, v & 0xff);
	dbgprintf("Built   : %s %s\r\n", __DATE__, __TIME__ );
	dbgprintf("Version : %s\r\n", NIN_GIT_VERSION);

	dbgprintf("Game path: %s\r\n", ConfigGetGamePath());
	return 0;
}

/* sdhc_log_destroy()
 * Called during shutdown - closes the log file. 
 */
void sdhc_log_destroy(void)
{
	if (sdhc_log_status == FR_OK)
	{
		sdhc_log_status = -1;
		f_close(&sdhc_log);
	}
}

void CheckOSReport(void)
{
	sync_before_read((void*)0x13160000, 0x8);
	u32 Length = read32(0x13160004);
	if (Length != 0x0)
	{
		char* Address = (char*)(P2C(read32(0x13160000)));
		sync_before_read((void*)((u32)(Address) & 0x1FFFFFFC), Length + 4);
		char* Msg = malloca(Length + 1, 0x20);
		strncpy(Msg, Address, Length);
		Msg[Length] = '\0';
		dbgprintf(Msg);
		free(Msg);
		write32(0x13160004, 0);
		sync_after_write((void*)0x13160004, 0x4);
	}
	return;
}
