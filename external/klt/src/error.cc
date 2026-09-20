/*********************************************************************
 * error.c
 *
 * Error and warning messages, and system commands.
 *********************************************************************/


/* Standard includes */
#include <cstdio>
#include <cstdlib>
#include <cstdarg>


/*********************************************************************
 * KLTError
 * 
 * Prints an error message and dies.
 * 
 * INPUTS
 * exactly like printf
 */

void KLTError(const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  fprintf(stderr, "KLT Error: ");
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
  va_end(args);
  exit(1);
}


/*********************************************************************
 * KLTWarning
 * 
 * Prints a warning message.
 * 
 * INPUTS
 * exactly like printf
 */

void KLTWarning(const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  fprintf(stderr, "KLT Warning: ");
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
  fflush(stderr);
  va_end(args);
}

