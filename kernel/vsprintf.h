#ifndef __VSPRINTF_H__
#define __VSPRINTF_H__

#include <stdarg.h>
#include "string.h"

//int dbgprintf( const char *fmt, ...);
int _vsprintf(char *buf, const char *fmt, va_list args);
int _sprintf( char *buf, const char *fmt, ... );
void CheckOSReport(void);

int sdhc_log_init(void);
void sdhc_log_destroy(void);
#endif
