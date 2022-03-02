/*
 * Studente:
 * Pietro Basci
 * 266004
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

//#include <rpc/types.h>  //in MacOS
#include <rpc/xdr.h>

#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../errlib.h"
#include "../sockwrap.h"

//#define SRVPORT 1500
//#define MSG_NOSIGNAL 0x2000 /* don't raise SIGPIPE */  //in MacOS

#define LISTENQ 15
#define MAXBUFL 65535 //1023
#define MAXFNML 511
#define MAXREL 10
#define MAXOPL 7


#define MSG_GET "GET"
#define MSG_OK "+OK\r\n"
#define MSG_ERR "-ERR\r\n"

#define MAX_UINT16T 0xffff
//#define STATE_OK 0x00
//#define STATE_V  0x01

#ifdef TRACE
#define trace(x) x
#else
#define trace(x)
#endif

char *prog_name;


int main (int argc, char *argv[]) {
    
    int sockfd, nread, i, err=0;
    struct in_addr addr;
    short port;
    struct sockaddr_in servaddr;
    char buf[MAXBUFL+1];
    char *file[argc-3];	
    char res[MAXREL];
    
    /* for errlib to know the program name */
    prog_name = argv[0];
    
    //signal(SIGPIPE, SIG_IGN);

    /* check arguments */
    if (argc < 4)
        err_quit ("usage: %s <ip_address> <port> <file to transfer(1 at least)>\n", prog_name);
    
    Inet_aton(argv[1], &addr);
    port = atoi(argv[2]);
    for (i = 3; i < argc; i++)
        file[i-3] = argv[i];
    
    /* create socket */
    sockfd = Socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    trace ( err_msg("(%s) socket created",prog_name) );
    
    /* specify address to connect to */
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = addr.s_addr;
    
    trace( err_msg ("(%s) trying to connect...", prog_name) );
    
    Connect(sockfd, (SA*) &servaddr, sizeof(servaddr));
    trace( err_msg ("(%s) connected", prog_name) );

    for (i = 0; i < argc-3; i++){
	
        /* send GET command */
        snprintf(buf, MAXBUFL, "%s %s\r\n", MSG_GET, file[i]);
        Sendn (sockfd, buf, strlen(buf), MSG_NOSIGNAL);
        trace( err_msg ("(%s) --- command sent %s", prog_name, buf) );
        
        //nread = readline_unbuffered (sockfd, buf, MAXBUFL);
        nread = readline_unbuffered_timeout (sockfd, buf, MAXREL, 15);
        if (nread == 0) {
            err_ret("(%s) - no response received",prog_name);
            Close (sockfd);
            return 0;
        } else if (nread < 0) {
            err_ret ("(%s) error - readline() failed", prog_name);
            Close (sockfd);
            return 0;
        }
        buf[nread]='\0';
        trace( err_msg("(%s) --- received string '%s'",prog_name, buf) );

        if (buf[nread-2] == '\r' && buf[nread-1] == '\n'){
            buf[nread-2] = '\0';
            buf[nread-1] = '\0';
        }
        else {
            trace( err_msg("(%s) - wrong or missing response",prog_name) );
            Close (sockfd);
            return 0;
        }
        
        memset(res, 0, sizeof(res));

        /* get the response and send MSG_ERR in case of error */
        if (sscanf(buf,"%4c",res) != 1) {
            trace( err_msg("(%s) - wrong or missing response",prog_name) );
            //err_ret("(%s) - wrong or missing response",prog_name);
            Close (sockfd);
            return 0;
        }
        trace( err_msg("(%s) --- response: %s",prog_name,res) );
        
        /* verify response received */
        if (strncmp(res, MSG_OK, 3) == 0){
            uint32_t size, tm;
            
            /* get the size */
            int n = readn_timeout(sockfd, &size, sizeof(size), 15);
            if (n != sizeof(size)){
                err_ret("(%s) - cannot recive the size",prog_name);
                Close (sockfd);
                return 0;
            }
            size = ntohl(size);
            trace( err_msg("(%s) --- size received: %d",prog_name,size) );

            /* get the file */
            FILE *f;
            //char tmp[MAXFNML+1];
            //strcpy(tmp, "download_");
            //strcat(tmp, file[i]);

            if ((f = fopen(file[i], "wb")) != NULL) {
                int bleft = size, rd, nr;
                
                while (bleft > 0) {
                    if (bleft < MAXBUFL)
                        rd = bleft;
                    else
                        rd = MAXBUFL;

                    nr = readn_timeout(sockfd, buf, rd, 15);
                    if (nr != rd) {
                        trace( err_msg("(%s) - cannot recive a part of the file",prog_name) );
                        Close (sockfd);
                        return 0;
                    }

                    fwrite(buf, sizeof(char), rd, f);
                    bleft -= nr;
                }
                fclose(f);
            } else {
                trace( err_msg("(%s) - cannot open file '%s'",prog_name, file[i]) );
                Close (sockfd);
                return 0;
            }
            trace( err_msg("(%s) --- received file '%s' ('%d' bytes)",prog_name, file[i],size) );

            /* get the timestamp */
            n = readn_timeout(sockfd, &tm, sizeof(tm), 15);
            if (n != sizeof(tm)){
                trace( err_msg("(%s) - cannot recive the timestamp",prog_name) );
                Close (sockfd);
                return 0;
            }
            tm = ntohl(tm);
            trace( err_msg("(%s) --- timestamp received: %d",prog_name,tm) );

            printf("File '%s' (size: %d, last modification: %d) received.\n",file[i],size,tm); //

        } else if (strncmp(res, MSG_ERR, 4) == 0){
            printf("%s (File '%s' cannot be received.)\n",res, file[i]);
            //continue;
            Close (sockfd);
            return 0;
        } else {
            trace( err_msg("(%s) - invalid response '%s'",prog_name, buf) );
            Close (sockfd);
            return 0;
        }
        
    }

    Close (sockfd);
    trace( err_msg ("(%s) - connection closed by %s", prog_name, (err==0)?"client":"server") );
    
    return 0;
}
