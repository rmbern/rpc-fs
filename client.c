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

int main () {
  FileHandle fh = ropen();
  sleep(600);
}
