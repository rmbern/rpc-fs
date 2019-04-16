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

  C2S_Message msg;
  memset(&msg, 0, sizeof(C2S_Message));
  msg.operation = 'R'; // R for Read
  msg.length = size;
  if (write(fh.sd, &msg, sizeof(C2S_Message)) < 0)
  {
    perror("Sending read request to server");
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

  // We can't keep the message on the stack here, unlike
  // the other commands, because we send over a variable
  // length buffer with relevant data in it.
  C2S_Message * msg = malloc(sizeof(C2S_Message) + size);
  memset(msg, 0, sizeof(C2S_Message) + size);
  msg->operation = 'W'; // W for write
  msg->length = size;

  if(memcpy(msg->buffer, buff, size) == NULL)
  {
    perror("Buffer copy for write request");
    exit(1);
  }
  printf("WRITE BUFFER: <%s>\n", msg->buffer);
  // Server should write size consecutive bytes to file
  // TODO: RECIEVE BYTES WRITTEN FROM SERVER
  int header_bytes_written = write(fh.sd, msg, sizeof(C2S_Message));
  if (header_bytes_written < 0)
  {
    perror("Sending header for server to write to file.");
    exit(1);
  }
  int data_bytes_written = write(fh.sd, msg->buffer, msg->length);
  if (data_bytes_written < 0)
  {
    perror("Sending header for server to write to file.");
    exit(1);
  }

  free(msg);
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
  return header_bytes_written + data_bytes_written;

}
int rseek(FileHandle fh, int whence, long offset)
{
  C2S_Message msg;
  memset(&msg, 0, sizeof(C2S_Message));
  msg.operation = 'S'; // S for seek
  msg.whence = whence;
  msg.offset = offset;

  if (write(fh.sd, &msg, sizeof(C2S_Message)) < 0)
  {
    perror("Informing server of offset/whence.");
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

int rclose (FileHandle fh)
{
  // NOTE: ALL API CALLS MUST CONTAIN THIS ERRNO READ.
  // TODO: Abstract out to own function??
  C2S_Message msg;
  memset(&msg, 0, sizeof(C2S_Message));
  msg.operation = 'C';
  if (write(fh.sd, &msg, sizeof(C2S_Message)) < 0)
  {
    perror("Sending request for server to write to file.");
    exit(1);
  }

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
  return 0;

}

int main ()
{
  FileHandle fh = ropen();

  char buff[5];

  // TEST CASE 1: BASIC READ
  printf("CASE 1: READ\n");
  memset(buff, 0, 5);
  int bytes = rread(fh, buff, 4);
  printf("%d:%s\n", bytes, buff);
  // END CASE 1

  // TEST CASE 2: BASIC WRITE
  printf("CASE 2: WRITE\n");
  memset(buff, 0, 5);
  strncpy(buff, "POOP", 5);
  bytes = rwrite(fh, buff, 4);
  printf("%d:%s\n", bytes, buff);
  // END CASE 2

  // TEST CASE 3: SEEK TO BEGINNING
  // DEPENDENT ON STREAM POINTER
  // IN ITS STATE FROM TEST CASE 2.
  printf("CASE 3: SEEK\n");
  rseek(fh, SEEK_SET, 0);
  memset(buff, 0, 5);
  strncpy(buff, "ABCD", 5);
  bytes = rwrite(fh, buff, 4);
  printf("%d:%s\n", bytes, buff);
  // END CASE 3

  // TEST CASE 4: FILE CLOSE
  printf("CASE 4: CLOSE\n");
  rclose(fh);
  memset(buff, 0, 5);
  strncpy(buff, "NOPE", 5);
  bytes = rwrite(fh, buff, 4);
  printf("%d:%s\n", bytes, buff);
  // END CASE 4


  sleep(100);
}
