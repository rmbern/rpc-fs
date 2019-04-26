#include "client.h"
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

int main()
{
  // Run from clamshell
  FileHandle fh = ropen("crabshell.rutgers.edu",
                        "CENSORED", 
                        O_CREAT | O_RDWR, 0777);

  char buff[5];

  // TEST CASE 1: BASIC READ
  printf("CASE 1: READ\n");
  memset(buff, 0, 5);
  int bytes = rread(fh, buff, 5);
  printf("Bytes read <%d> : Buffer <%s>\n", bytes, buff);
  // END CASE 1

  sleep(3);

  // TEST CASE 2: BASIC WRITE
  printf("CASE 2: WRITE\n");
  memset(buff, 0, 5);
  strncpy(buff, "EFGH", 5);
  bytes = rwrite(fh, buff, 5);
  printf("Bytes written <%d> : Buffer <%s>\n", bytes, buff);
  // END CASE 2

  sleep(3);

  // TEST CASE 3: SEEK TO BEGINNING
  // DEPENDENT ON STREAM POINTER
  // IN ITS STATE FROM TEST CASE 2.
  printf("CASE 3: SEEK\n");
  int seek = rseek(fh, SEEK_SET, 0);
  memset(buff, 0, 5);
  strncpy(buff, "ABCD", 5);
  bytes = rwrite(fh, buff, 5);
  printf("Bytes written <%d> : Buffer <%s> : Seek locaton <%d>\n", bytes, buff, seek);
  // END CASE 3

  sleep(3);

  // TEST CASE 4: FILE CLOSE
  printf("CASE 4: CLOSE\n");
  int close_return = rclose(fh);
  memset(buff, 0, 5);
  strncpy(buff, "NOPE", 5);
  printf("Attempting to write to closed file\n");
  bytes = rwrite(fh, buff, 4);
  // END CASE 4
  printf("Can't reach here after trying to write to closed file!\n");
  printf("Socket is closed, resulting in a broken pipe!\n");
  printf("cat file called CENSORED to see results!\n");
}
