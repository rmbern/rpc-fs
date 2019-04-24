#include <stdio.h>
#include "common.h"

FileHandle ropen(char * machineName, char * filename, int flags, int mode);
int rread(FileHandle fh, void * buffer, int size);
int rwrite(FileHandle fh, void * buff, int size);
int rseek(FileHandle fh, int whence, long offset);
int rclose (FileHandle fh);
