/*
 * stdio/physfs abstraction layer 2002-08-29
 *
 * Adam D. Moss <adam@gimp.org> <aspirin@icculus.org>
 *
 * These wrapper macros and functions are designed to allow a program
 * to perform file I/O with identical semantics and syntax regardless
 * of whether PhysicsFS is being used or not.  Define USE_PHYSFS if
 * PhysicsFS is being usedl in this case you will still need to initialize
 * PhysicsFS yourself and set up its search-paths.
 */
#ifndef _ABS_FILE_H
#define _ABS_FILE_H
/*
PLEASE NOTE: This license applies to abs-file.h ONLY (to make it clear that
you may embed this wrapper code within commercial software); PhysicsFS itself
is (at the time of writing) released under a different license with
additional restrictions.

Copyright (C) 2002 Adam D. Moss (the "Author").  All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is fur-
nished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FIT-
NESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHOR BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CON-
NECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the Author of the
Software shall not be used in advertising or otherwise to promote the sale,
use or other dealings in this Software without prior written authorization
from the Author.
*/

#include <stdlib.h>
#include <stdio.h>

#ifdef USE_PHYSFS

#include <physfs.h>
#define MY_FILETYPE PHYSFS_file
#define MY_READ(p,s,n,fp) PHYSFS_read(fp,p,s,n)
#define MY_OPEN_FOR_READ(fn) PHYSFS_openRead(fn)
static int MY_GETC(MY_FILETYPE * fp) {
  unsigned char c;
  /*if (PHYSFS_eof(fp)) {
    return EOF;
  }
  MY_READ(&c, 1, 1, fp);*/
  if (MY_READ(&c, 1, 1, fp) != 1) {
    return EOF;
  }
  return c;
}
static char * MY_GETS(char * const str, int size, MY_FILETYPE * fp) {
  int i = 0;
  int c;
  do {
    if (i == size-1) {
      break;
    }
    c = MY_GETC(fp);
    if (c == EOF) {
      break;
    }
    str[i++] = c;
  } while (c != '\0' &&
	   c != -1 &&
	   c != '\n');
  str[i] = '\0';
  if (i == 0) {
    return NULL;
  }
  return str;
}
#define MY_CLOSE(fp) PHYSFS_close(fp)
#define MY_ATEOF(fp) PHYSFS_eof(fp)
#define MY_REWIND(fp) PHYSFS_seek(fp,0)

#else

#define MY_FILETYPE FILE
#define MY_READ(p,s,n,fp) fread(p,s,n,fp)
#define MY_OPEN_FOR_READ(n) fopen(n, "rb")
#define MY_GETC(fp) fgetc(fp)
#define MY_GETS(str,size,fp) fgets(str,size,fp)
#define MY_CLOSE(fp) fclose(fp)
#define MY_ATEOF(fp) feof(fp)
#define MY_REWIND(fp) rewind(fp)

#endif

#endif
