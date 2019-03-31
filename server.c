#include <sys/types.h> 
#include <sys/socket.h> 
#include <sys/un.h> 
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <unistd.h>
#include <netdb.h>
#include <pthread.h>
#include "common.h" 

// TODO: SIGINT handler to close all fd's
void * connection_thread(void * args)
{
    pthread_detach(pthread_self());

    int sd = *(int*)args;
    free(args); 

    // Create server side file handle
    // TODO: Manage globally for sighandler to clean up
    //       Right now, we can lose data at the "tail
    //       end" of our file buffer.
    FILE * fd = fopen("TMP", "w");
    if (fd == NULL)
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
            break; // Go to thread cleanup
        }
        else if (result < 0)
        {
            perror("First socket read\n");
            exit(1);
        } 
        printf("%c\n", command_byte);
        if (command_byte != 'L' && command_byte != 'R')
        {
            fprintf(stderr, "Bad first value from client. Alerting client, and closing thread\n");
            char byte = -1;
            if (write(sd, &byte, 1) < 0)
            {
                perror("Socket write: bad direction value\n");
                exit(1);
            }
            break; // Go to thread cleanup
        }

        result = read(sd, recieved_msg, sizeof(recieved_msg));
        if (result == 0)
        {
            printf("EOF or broken TCP connection detected.\n");
            printf("Connection thread exiting\n");
            break; // Go to thread cleanup
        }

        else if (result < 0)
        {
            perror("Second socket read\n");
            exit(1);
        }

        // notify client that there were no errors. 
        char byte = 0;
        if (write(sd, &byte, 1) < 0)
        {
            perror("Socket write\n");
            exit(1);
        }

        if (write(sd, recieved_msg, sizeof(recieved_msg)) < 0)
        {
            perror("Socket write\n");
            exit(1);
        }
        printf("%s\n", recieved_msg); 
    }

    close(sd);
    fclose(fd);
    pthread_exit(0);
}

int main () 
{
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
