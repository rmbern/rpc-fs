#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h> 
#include <sys/un.h> 
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h> 
#include <stdio.h> 
#include <netdb.h> 
#include <string.h> 
#include <time.h>
#include <errno.h>
#include "common.h" 

FileHandle ropen(char * machineName, char * filename, int flags, int mode)
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

    int getaddr_result = getaddrinfo(machineName, 
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

    // Send parameters for call to open
    C2S_Message * msg = malloc(sizeof(C2S_Message) + strlen(filename)+1);
                                                  // ^^ +1 for null term string
    memset(msg, 0, sizeof(C2S_Message) + strlen(filename)+1);
    msg->operation = 'O';
    msg->length = strlen(filename)+1;
    msg->flags = flags;
    msg->mode = mode;
    strncpy(msg->buffer, filename, msg->length);

    int header_bytes_written = write(sd, msg, sizeof(C2S_Message));
    if (header_bytes_written < 0)
    {
      perror("Sending header for server to write to file.");
      exit(1);
    }

    int data_bytes_written = write(sd, msg->buffer, msg->length);
    if (data_bytes_written < 0)
    {
      perror("Sending header for server to write to file.");
      exit(1);
    }
    free(msg);

    // NOTE: CLIENT SENDS SERVER EOF WHEN THREAD DIES.
    // TODO: IS THIS UB?

    S2C_Message res;
    memset(&res, 0, sizeof(S2C_Message));

    if (read(sd, &res, sizeof(S2C_Message)) < 0)
    {
      perror("Reading res from server");
      exit(1);
    }
    
    if (res.err)
    {
      fprintf(stderr, "Server returned errno %d\n", res.err);
      errno = res.err;
    }
    return sd;
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
  if (write(fh, &msg, sizeof(C2S_Message)) < 0)
  {
    perror("Sending read request to server");
    exit(1);
  }
  // Server should send size consecutive bytes to the
  // client.
  
  // Place these bytes into a file.
  // TODO: RECIEVE BYTES READ FROM SERVER
  int bytes_read = read(fh, buffer, size);
  if (bytes_read < 0)
  {
    perror("Receiving file data from server.");
    exit(1);
  }

  // NOTE: ALL API CALLS MUST CONTAIN THIS RESPONSE READ.
  S2C_Message res;
  memset(&res, 0, sizeof(S2C_Message));
  if (read(fh, &res, sizeof(S2C_Message)) < 0)
  {
    perror("Reading errno from server");
    exit(1);
  }
  
  if (res.err)
  {
    fprintf(stderr, "Server returned errno %d\n", res.err);
    errno = res.err;
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
  int header_bytes_written = write(fh, msg, sizeof(C2S_Message));
  if (header_bytes_written < 0)
  {
    perror("Sending header for server to write to file.");
    exit(1);
  }
  int data_bytes_written = write(fh, msg->buffer, msg->length);
  if (data_bytes_written < 0)
  {
    perror("Sending header for server to write to file.");
    exit(1);
  }

  free(msg);
  // NOTE: ALL API CALLS MUST CONTAIN THIS RESPONSE READ.
  S2C_Message res;
  memset(&res, 0, sizeof(S2C_Message));
  if (read(fh, &res, sizeof(S2C_Message)) < 0)
  {
    perror("Reading errno from server");
    exit(1);
  }

  if (res.err)
  {
    fprintf(stderr, "Server returned errno %d\n", res.err);
    errno = res.err;
  }
  return res.byte_count;

}

int rseek(FileHandle fh, int whence, long offset)
{
  C2S_Message msg;
  memset(&msg, 0, sizeof(C2S_Message));
  msg.operation = 'S'; // S for seek
  msg.whence = whence;
  msg.offset = offset;

  if (write(fh, &msg, sizeof(C2S_Message)) < 0)
  {
    perror("Informing server of offset/whence.");
    exit(1);
  }

  // NOTE: ALL API CALLS MUST CONTAIN THIS ERRNO READ.
  S2C_Message res;
  memset(&res, 0, sizeof(S2C_Message));
  if (read(fh, &res, sizeof(S2C_Message)) < 0)
  {
    perror("Reading errno from server");
    exit(1);
  }
  if (res.err)
  {
    fprintf(stderr, "Server returned errno %d\n", res.err);
    errno = res.err;
  }
  // TODO: Return seek amount
  return res.byte_count;

}

int rclose (FileHandle fh)
{
  C2S_Message msg;
  memset(&msg, 0, sizeof(C2S_Message));
  msg.operation = 'C';
  if (write(fh, &msg, sizeof(C2S_Message)) < 0)
  {
    perror("Sending request for server to write to file.");
    exit(1);
  }

  // NOTE: ALL API CALLS MUST CONTAIN THIS ERRNO READ.
  S2C_Message res;
  memset(&res, 0, sizeof(S2C_Message));
  if (read(fh, &res, sizeof(S2C_Message)) < 0)
  {
    perror("Reading errno from server");
    exit(1);
  }
  if (res.err)
  {
    fprintf(stderr, "Server returned errno %d\n", res.err);
    errno = res.err;
  }
  return res.err;

}
