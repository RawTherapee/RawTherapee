/*********************************************************************
 * error.h
 *********************************************************************/

#ifndef _ERROR_H_
#define _ERROR_H_

#include <cstdio>
#include <cstdarg>

void KLTError(const char *fmt, ...);
void KLTWarning(const char *fmt, ...);

#endif

