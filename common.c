#include <netdb.h> 
#include <sys/types.h> 
#include <sys/socket.h>
#include <stdio.h> 
#include <stdlib.h> 


void handle_getaddr_error(int getaddr_result)
{
    if (getaddr_result != 0)
    {
        fprintf(stderr, "getaddrinfo error!\n");
        switch (getaddr_result)
        {
            // This enum is in the getaddrinfo man page, but isn't in the
            // actual source library for some reason.
            //case EAI_ADDRFAMILY:
                //fprintf(stderr,"Invalid address.\n");
                //fprintf(stderr,"Code: EAI_ADDRFAMILY\n");
                //break;
            case EAI_AGAIN:
                fprintf(stderr,"Try again later.\n");
                fprintf(stderr,"Code: EAI_AGAIN\n");
                break;
            case EAI_BADFLAGS:
                fprintf(stderr,"Invalid request.\n");
                fprintf(stderr,"Code: EAI_BADFLAGS\n");
                break;
            case EAI_FAIL:
                fprintf(stderr,"Name server returned permanent failure\n");
                fprintf(stderr,"Code: EAI_FAIL\n");
                break;
            case EAI_FAMILY:
                fprintf(stderr,"Request address family not supported\n");
                fprintf(stderr,"Code: EAI_FAMILY\n");
                break;
            case EAI_MEMORY:
                fprintf(stderr,"Out of memory\n");
                fprintf(stderr,"Code: EAI_MEMORY\n");
                break;
            case EAI_SERVICE:
                fprintf(stderr,"This program's sock can't handle resolve service\n");
                fprintf(stderr,"Code: EAI_SERVICE\n");
                break;
            case EAI_SOCKTYPE:
                fprintf(stderr,"This program's socket type not supported\n");
                fprintf(stderr,"Code: EAI_SOCKTYPE\n");
                break;
            case EAI_SYSTEM:
                perror("getaddrinfo system error\n");
                break;
            default:
                fprintf(stderr,"Unspecified error\n");
                break;
        }
        fprintf(stderr,"If you recieved an EAI error code, refer to the getaddrinfo\n");
        fprintf(stderr,"man page for more info.\n");
        exit(1);
    }
}
