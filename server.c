#include <sys/types.h> 
#include <sys/socket.h> 
#include <sys/un.h> 
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <unistd.h>
#include <netdb.h>
#include <pthread.h>
#include <errno.h>
#include "common.h" 
pthread_mutex_t g_fh_lock;

C2S_Message * recieve_message(int sd, int fd)
// We need the sd to get the message over the network,
// and the file descriptor to close the file if the
// connection dies.
{
  C2S_Message * message = malloc(sizeof(C2S_Message));
  memset(message, 0, sizeof(C2S_Message));
  int result = read(sd, message, sizeof(C2S_Message));
  if (result < 0)
  {
    perror("message socket read\n");
    exit(1);
  } 
  if (result == 0)
  {
    printf("EOF or socket close detected\n");
    printf("Connection thread exiting.\n");
    close(fd);
    close(sd);
    pthread_exit(0);
  }
  else if (result < 0)
  {
    fprintf(stderr, "Unknown error from message recieve\n");
    exit(1);
  }
  
  message = realloc(message, sizeof(C2S_Message) + message->length);
  memset(message->buffer, 0, message->length);
  int buff_result = 0;
  if (message->operation == 'W'
      ||
      message->operation == 'O') // We need to fill the msg buffer
  {
    if(read(sd, message->buffer, message->length) < 0)
    {
      perror("message socket read for buffer\n");
      exit(1);
    }
  }
  return message;
}

void * connection_thread(void * args)
{
  pthread_detach(pthread_self());

  int sd = *(int*)args;
  free(args); 
  
  int fd = -1; // changes after open
  while(1)
  {
    C2S_Message * msg = recieve_message(sd,fd);
    S2C_Message res;
    memset(&res, 0, sizeof(S2C_Message));
    // The gotos below are to resolve any confusion or ambiguity with
    // the intent of a break being in both a loop and a switch statement.
    switch(msg->operation)
    {
      case 'O': // O for Open
        if(pthread_mutex_lock(&g_fh_lock) != 0)
        {
          res.err = errno;
          perror("Mutex lock for open");
          goto RESPONSE;
        }
        fd = open(msg->buffer, msg->flags, msg->mode);
        if (fd == -1)
        {
          perror("File open\n");
          exit(1);
        }
        if(pthread_mutex_unlock(&g_fh_lock) != 0)
        {
          res.err = errno;
          perror("Mutex unlock for open");
          goto RESPONSE;
        } 
        goto RESPONSE;
      case 'R': // R for Read
        memset(msg->buffer, 0, msg->length);

        if(pthread_mutex_lock(&g_fh_lock) != 0)
        {
          res.err = errno;
          perror("Mutex lock for read");
          goto RESPONSE;
        }

        if (res.byte_count = read(fd, msg->buffer, msg->length) < 0)
        {
          res.err = errno;
          perror("File read.");
          goto RESPONSE;
        }

        if(pthread_mutex_unlock(&g_fh_lock) != 0)
        {
          res.err = errno;
          perror("Mutex unlock for read");
          goto RESPONSE;
        }

        // This should be part of the S2C_Message, but
        // the deadline nears...
        if(write(sd, msg->buffer, msg->length) < 0)
        {
          res.err = errno;
          perror("Sending read string to client");
          goto RESPONSE;
        }
        goto RESPONSE;
      case 'W':
        if (pthread_mutex_lock(&g_fh_lock) != 0)
        {
          res.err = errno;
          perror("Mutex lock for write");
          goto RESPONSE;
        }
        res.byte_count = write(fd, msg->buffer, msg->length);
        if (res.byte_count < 0)
        {
          res.err = errno;
          perror("Writing to file\n");
          goto RESPONSE;
        }
        if (pthread_mutex_unlock(&g_fh_lock) != 0)
        {
          res.err = errno;
          perror("Mutex unlock for write");
          goto RESPONSE;
        }
        goto RESPONSE;

      case 'S': // S for seek
        if(pthread_mutex_lock(&g_fh_lock) != 0)
        {
          perror("Mutex lock for seek");
          goto RESPONSE;
        }
        res.byte_count = lseek(fd, msg->offset, msg->whence);
        if (res.byte_count < 0)
        {
          res.err = errno;
          perror("File seek\n");
          goto RESPONSE;
        }
        if (pthread_mutex_unlock(&g_fh_lock) != 0)
        {
          perror("Mutex unlock for seek");
          goto RESPONSE;
        }
        goto RESPONSE;
      case 'C': // C for close
        if (close(fd) != 0)
        {
          res.err = errno;
          perror("File close");
          goto RESPONSE;
        }
        else
        {
          res.err = 0;
          if(write(sd, &msg, sizeof(S2C_Message)) < 0)
          {
            perror("Sending client errno");
            exit(1);
          }
          close(sd);
          pthread_exit(0);
        }
        goto RESPONSE;
      default:
        fprintf(stderr, "BAD COMMAND RECIEVED FROM CLIENT\n");
        goto RESPONSE;
    }

    // Send client server's errno at the end of 
    // quick google search says errno is thread
    // safe. Yay!
RESPONSE:
    free(msg);
    if(write(sd, &res, sizeof(S2C_Message)) < 0)
    {
      perror("Sending client response");
      exit(1);
    }
    if(res.err)
    {
      exit(1);
    }
  }
}
int main () 
{
  pthread_mutex_init(&g_fh_lock, 0);
  struct addrinfo request;
  memset(&request, 0, sizeof(struct sockaddr));
  request.ai_flags = AI_PASSIVE;
  request.ai_family = AF_INET;
  request.ai_socktype = SOCK_STREAM;
  request.ai_protocol = 0;
  request.ai_addrlen = 0;
  request.ai_canonname = NULL;
  request.ai_next = NULL;

  struct addrinfo * responses;

  int getaddr_result = getaddrinfo(NULL, 
                                   "5000", 
                                   &request,
                                   &responses);

  handle_getaddr_error(getaddr_result);

  int sd = socket(AF_INET, 
                  SOCK_STREAM, // TCP
                  0); // Use configured reasonable defaults 

  if (sd == -1)
  {
    perror("Socket creation\n");
    exit(1);
  }
  int on = 1;
  if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == - 1)
  {
    perror("Socket configure\n");
    exit(1);
  }

  struct sockaddr address;
  memset(&address, 0, sizeof(struct sockaddr));

  if (bind(sd,
       responses->ai_addr, 
       sizeof(struct sockaddr)) == -1)
  {
    perror("Socket bind\n");
    exit(1);
  }

  if (listen(sd, 1) == -1)
  {
    perror("Socket listen\n");
    exit(1);
  }

  // Accept connections continuously
  while (1)
  {
    int sock_connection;
    socklen_t * socklen = malloc(sizeof(socklen_t));
    *socklen = responses->ai_addrlen;
    sock_connection = accept(sd, responses->ai_addr, socklen);
    if (sock_connection < 0)
    {
      perror("Socket accept");
      exit(1);
    }
    free(socklen); 

    int * args = malloc(sizeof(int));
    *args = sock_connection;
    pthread_t thread; // Not interested in thread id's so a single
                      // pthread_t struct will suffice.
    if (pthread_create(&thread, 0, connection_thread, args) == -1)
    {
      perror("connection thread spawn\n"); 
      exit(1);
    }
  }

  freeaddrinfo(responses);
  pthread_exit(0);
}
