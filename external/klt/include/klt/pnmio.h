/*********************************************************************
 * pnmio.h
 *********************************************************************/

#ifndef _PNMIO_H_
#define _PNMIO_H_

#include <cstdio>

/**********
 * With pgmReadFile and pgmRead, setting img to NULL causes memory
 * to be allocated
 */

/**********
 * used for reading from/writing to files
 */
unsigned char* pgmReadFile(
  const char *fname,
  unsigned char *img,
  int *ncols, 
  int *nrows);
void pgmWriteFile(
  const char *fname,
  const unsigned char *img,
  int ncols,
  int nrows);
void ppmWriteFileRGB(
  const char *fname,
  const unsigned char *redimg,
  const unsigned char *greenimg,
  const unsigned char *blueimg,
  int ncols,
  int nrows);

/**********
 * used for communicating with stdin and stdout
 */
unsigned char* pgmRead(
  FILE *fp,
  unsigned char *img,
  int *ncols, int *nrows);
void pgmWrite(
  FILE *fp,
  const unsigned char *img,
  int ncols,
  int nrows);
void ppmWrite(
  FILE *fp,
  const unsigned char *redimg,
  const unsigned char *greenimg,
  const unsigned char *blueimg,
  int ncols,
  int nrows);

#endif
