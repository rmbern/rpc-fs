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

// TODO: SIGINT handler to close all fd's
void * connection_thread(void * args)
{
  pthread_detach(pthread_self());

  int sd = *(int*)args;
  free(args); 

  // Create server side file handle
  // TODO: Manage globally for sighandler to clean up
  //     Right now, we can lose data at the "tail
  //     end" of our file buffer.

  int fd = open("TMP", O_APPEND | O_CREAT | O_RDWR);
  if (fd == -1)
  {
    perror("File open\n");
    exit(1);
  }

  char recieved_msg[100];
  while(1)
  {
    memset(recieved_msg, 0, sizeof(recieved_msg));
    char command_byte = -1;
    int result = read(sd, &command_byte, 1);
    if (result == 0)
    {
      printf("EOF or broken TCP connection detected.\n");
      printf("Connection thread exiting\n");
      close(sd);
      close(fd);
      pthread_exit(0);
    }
    else if (result < 0)
    {
      perror("Command Byte socket read\n");
      goto ERROR;
    } 
    printf("%c\n", command_byte);
    int recieved_size = -1;
    int send_size = -1;
    char err = 0;
    char * buff;
    switch(command_byte)
    {
      case 'R': // R for Read
        result = read(sd, &recieved_size, sizeof(int));
        if (result == 0)
        {
          err = errno;
          printf("EOF or broken TCP connection detected.\n");
          printf("Connection thread exiting\n");
          close(sd);
          close(fd);
          pthread_exit(0);
        }
        else if (result < 0)
        {
          err = errno;
          perror("Determining buffer size for read command\n");
          goto ERROR;
        }
        buff = malloc(recieved_size);
        memset(buff, 0, recieved_size);

        if(pthread_mutex_lock(&g_fh_lock) != 0)
        {
          perror("Mutex lock for read");
          exit(1);
        }

        if (read(fd, buff, recieved_size) < 0)
        {
          err = errno;
          perror("File read.");
          goto ERROR;
        }

        if(pthread_mutex_unlock(&g_fh_lock) != 0)
        {
          perror("Mutex unlock for read");
          exit(1);
        }

        if (write(sd, buff, recieved_size) < 0)
        {
          err = errno;
          perror("Writing to client.");
          goto ERROR;
        }
        free(buff);
        break;
      case 'W':
        // TODO: abstract these reads out to fn?
        result = read(sd, &recieved_size, sizeof(int));
        if (result == 0)
        {
          err = errno;
          printf("EOF or broken TCP connection detected.\n");
          printf("Connection thread exiting\n");
          close(sd);
          close(fd);
          pthread_exit(0);
        }
        else if (result < 0)
        {
          err = errno;
          perror("Determining buffer size for read command\n");
          goto ERROR;
        }

        buff = malloc(recieved_size);
        memset(buff, 0, recieved_size);
        result = read(sd, buff, recieved_size);
        if (result == 0)
        {
          err = errno;
          printf("EOF or broken TCP connection detected.\n");
          printf("Connection thread exiting\n");
          close(sd);
          close(fd);
          pthread_exit(0);
        }
        else if (result < 0)
        {
          err = errno;
          perror("Determining buffer size for read command\n");
          goto ERROR;
        }

        if (pthread_mutex_lock(&g_fh_lock) != 0)
        {
          perror("Mutex lock for write");
          exit(1);
        }
        if (write(fd, buff, recieved_size) < 0)
        {
          err = errno;
          perror("Writing to file\n");
          goto ERROR;
        }
        if (pthread_mutex_unlock(&g_fh_lock) != 0)
        {
          perror("Mutex unlock for write");
          exit(1);
        }

        printf("WROTE %s TO FILE\n", buff);
        break;
      default:
        fprintf(stderr, "BAD COMMAND RECIEVED FROM CLIENT\n");
        break;
    }

    printf("%s\n", recieved_msg); 
    // Send client server's errno at the end of 
    // quick google search says errno is thread
    // safe. Yay!
ERROR:
    if(write(sd, &err, sizeof(char)) < 0)
    {
      perror("Sending client errno");
      exit(1);
    }
    if(err)
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
