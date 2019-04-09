#include <sys/types.h>
#include <sys/socket.h> 
#include <sys/un.h> 
#include <unistd.h>
#include <stdlib.h> 
#include <stdio.h> 
#include <netdb.h> 
#include <string.h> 
#include <time.h>
#include "common.h" 

FileHandle ropen()
{
  static int sd = -2; // -1 reserved for sys error
  if (sd == -2)
  {
    struct addrinfo request;
    memset(&request, 0, sizeof(struct sockaddr));
    request.ai_flags = 0;
    request.ai_family = AF_INET;
    request.ai_socktype = SOCK_STREAM;
    request.ai_protocol = 0;
    request.ai_addrlen = 0;
    request.ai_canonname = NULL;
    request.ai_next = NULL;

    struct addrinfo * responses;

    int getaddr_result = getaddrinfo("localhost", 
                                     "5000", 
                                     &request,
                                     &responses);
    handle_getaddr_error(getaddr_result);

    sd = socket(AF_INET, // Will use the network later
                SOCK_STREAM, // TCP
                0); // Use configured reasonable defaults 

    if (sd == -1)
    {
      perror("Socket creation\n");
      exit(1);
    }

    struct sockaddr address;
    memset(&address, 0, sizeof(struct sockaddr));

    if (connect(sd,
                responses->ai_addr, 
                sizeof(struct sockaddr)) == -1)
    {
      perror("Socket connect");
      exit(1);
    }
    printf("CONNECTED\n");
    // NOTE: CLIENT SENDS SERVER EOF WHEN THREAD DIES.
    // TODO: IS THIS UB?
    FileHandle fh;
    fh.sd = sd;
    fh.fd = NULL; 
    
    return fh;
  }
}

int rread(FileHandle fh, void * buffer, int size)
{
  // TODO: TEST A TRUNCATION:
  //       IE SOMETHING LIKE ASKING TO READ 1000 BYTES FROM
  //       A 100 BYTE FILE
 
  // Using fh socket, tell server to read size bytes
  // from the remote file.
  char command = 'R'; // R for Read
  if (write(fh.sd, &command, sizeof(char)) < 0)
  {
    perror("Sending request for server to read file.");
    exit(1);
  }
  if (write(fh.sd, &size, sizeof(int)) < 0)
  {
    perror("Informing server of how many bytes to read.");
    exit(1);
  }
  // Server should send size consecutive bytes to the
  // client.
  
  // Place these bytes into a file.
  // TODO: RECIEVE BYTES READ FROM SERVER
  int bytes_read = read(fh.sd, buffer, size);
  if (bytes_read < 0)
  {
    perror("Receiving file data from server.");
    exit(1);
  }

  // NOTE: ALL API CALLS MUST CONTAIN THIS ERRNO READ.
  // TODO: Abstract out to own function??
  char err = -1;
  if (read(fh.sd, &err, sizeof(char)) < 0)
  {
    perror("Reading errno from server");
    exit(1);
  }
  
  if (err)
  {
    fprintf(stderr, "Server returned errno %d\n", err);
    exit(1);
  }
  return bytes_read;
}

int rwrite(FileHandle fh, void * buff, int size)
{
  // Using fh socket, tell server to read size bytes
  // from the remote file.
  char command = 'W'; // W for write 
  if (write(fh.sd, &command, sizeof(char)) < 0)
  {
    perror("Sending request for server to write to file.");
    exit(1);
  }
  if (write(fh.sd, &size, sizeof(int)) < 0)
  {
    perror("Informing server of how many bytes to write.");
    exit(1);
  }
  // Server should write size consecutive bytes to file
  // TODO: RECIEVE BYTES WRITTEN FROM SERVER
  int bytes_written = write(fh.sd, buff, size);
  if (bytes_written < 0)
  {
    perror("Sending data for server to write to file.");
    exit(1);
  }

  // NOTE: ALL API CALLS MUST CONTAIN THIS ERRNO READ.
  // TODO: Abstract out to own function??
  char err = -1;
  if (read(fh.sd, &err, sizeof(char)) < 0)
  {
    perror("Reading errno from server");
    exit(1);
  }

  if (err)
  {
    fprintf(stderr, "Server returned errno %d\n", err);
    exit(1);
  }
  return bytes_written;

}
int rseek(FileHandle fh, int whence, long offset)
{
  char command = 'S'; // S for seek 
  if (write(fh.sd, &command, sizeof(char)) < 0)
  {
    perror("Sending request for server to write to file.");
    exit(1);
  }

  if (write(fh.sd, &offset, sizeof(off_t)) < 0)
  {
    perror("Informing server of offset.");
    exit(1);
  }

  char whence_to_send = -1;
  switch(whence)
  {
    case SEEK_SET: 
      whence_to_send = 'S'; // S for SET in SEEK_SET
      break;
    case SEEK_CUR: // C for CUR in SEEK_CUR
      whence_to_send = 'C';
      break;
    case SEEK_END: // E for END in SEEK_END
      whence_to_send = 'E';
      break;
    default:
      fprintf(stderr, "Error! Attempted to send incorrect whence!\n");
      exit(1);
  }

  if (write(fh.sd, &whence_to_send, sizeof(char)) < 0)
  {
    perror("Informing server of whence.");
    exit(1);
  }

  // NOTE: ALL API CALLS MUST CONTAIN THIS ERRNO READ.
  // TODO: Abstract out to own function??
  char err = -1;
  if (read(fh.sd, &err, sizeof(char)) < 0)
  {
    perror("Reading errno from server");
    exit(1);
  }
  if (err)
  {
    fprintf(stderr, "Server returned errno %d\n", err);
    exit(1);
  }
  printf("SEEK ERR: %d\n", err);
  // TODO: Return seek amount
  return 0;

}

int main () {
  FileHandle fh = ropen();

  char buff[5];

  // TEST CASE 1: BASIC READ
  memset(buff, 0, 5);
  int bytes = rread(fh, buff, 4);
  printf("%d:%s\n", bytes, buff);
  // END CASE 1

  // TEST CASE 2: BASIC WRITE
  memset(buff, 0, 5);
  strncpy(buff, "POOP", 5);
  bytes = rwrite(fh, buff, 4);
  printf("%d:%s\n", bytes, buff);
  // END CASE 2

  // TEST CASE 3: SEEK TO BEGINNING
  // DEPENDENT ON STREAM POINTER
  // IN ITS STATE FROM TEST CASE 2.
  rseek(fh, SEEK_SET, 0);
  memset(buff, 0, 5);
  strncpy(buff, "ABCD", 5);
  bytes = rwrite(fh, buff, 4);
  printf("%d:%s\n", bytes, buff);
  // END CASE 3


  sleep(100);
}
