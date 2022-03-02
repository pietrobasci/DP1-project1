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
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>

//#include <rpc/types.h>  //in MacOs
#include <rpc/xdr.h>

#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../errlib.h"
#include "../sockwrap.h"

//#define SRVPORT 1500
//#define MSG_NOSIGNAL 0x2000 /* don't raise SIGPIPE */  //in MacOs

#define LISTENQ 15
#define MAXBUFL 65535 //1023
#define MAXFNML 511
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


int send_file(int connfd, char buf[], char* file)
{
    struct stat properties;
    
    if (stat(file, &properties) == 0 && !S_ISDIR(properties.st_mode)){
        FILE *f;
        if ((f = fopen(file, "rb")) != NULL){
            int i, len, ret_send;
            uint32_t size, tm;

            /* Send OK MSG */
            len = strlen(MSG_OK);
            ret_send = sendn(connfd, MSG_OK, len, MSG_NOSIGNAL);
            if (ret_send!=len) {
                trace( err_msg("(%s) - cannot send MSG_OK",prog_name) );
                return -1;
            }
            trace( err_msg("(%s) --- MSG_OK sent",prog_name) );
            
            /* Send file's size */
            size = htonl(properties.st_size);
            len = sizeof(size);
            ret_send = sendn(connfd, &size, len, MSG_NOSIGNAL);
            if (ret_send!=len) {
                trace( err_msg("(%s) - cannot send file's size",prog_name) );
                return -1;
            }
            trace( err_msg("(%s) --- size to send: %d",prog_name, ntohl(size)) );

            /* Send file */
            size = properties.st_size;
            i=0;
            while (size > 0) {
                int k = fread(buf, sizeof(char), MAXBUFL, f);
                buf[k] = '\0';
                i++;
                trace( err_msg("(%s) --- block'%d', '%d'",prog_name, i, k) );
                ret_send = sendn(connfd, buf, k, MSG_NOSIGNAL);
                if (ret_send != k) {
                    trace( err_msg("(%s) - cannot send part of the file",prog_name) );
                    return -1;
                }
                size -= k;
            }
            trace( err_msg("(%s) --- file '%s' sent ('%d' bytes)",prog_name, file, properties.st_size) );
            fclose(f);
            
            /* Send timestamp */
            tm = properties.st_mtime;
            tm = htonl(tm);
            len = sizeof(tm);
            ret_send = sendn(connfd, &tm, len, MSG_NOSIGNAL);
            if (ret_send!=len) {
                trace( err_msg("(%s) - cannot send timestamp",prog_name) );
                return -1;
            }
            trace( err_msg("(%s) --- timestamp sent (%d)",prog_name, ntohl(tm)) );
            return 0;
            
        }
    } else {
        trace( err_msg("(%s) - file '%s' not found",prog_name, file) );
    }
    return -1;
}


int prot_a (int connfd) {
    
    char buf[MAXBUFL+1]; /* +1 to make room for \0 */
    char op[MAXOPL+1];
    char file[MAXFNML+1];
    int nread, res;
    
    while (1) {
        trace( err_msg("(%s) --- waiting for commands ...",prog_name) );
        
        nread = readline_unbuffered_timeout (connfd, buf, MAXBUFL, 15);
        if (nread == 0) {
            return 0;
        } else if (nread < 0) {
            trace( err_msg("(%s) error - readline() failed", prog_name) );
            /* return to the caller to wait for a new connection */
            return 0;
        }
        buf[nread]='\0';
        trace( err_msg("(%s) --- received string '%s'",prog_name, buf) );
        
        if (buf[nread-2] == '\r' && buf[nread-1] == '\n'){
            buf[nread-2] = '\0';
            buf[nread-1] = '\0';
        }
        else
            return -1;
        trace( err_msg("(%s) --- cleaned string '%s'",prog_name, buf) );
        
        memset(op, 0, sizeof(op));
        memset(file, 0, sizeof(file));
        char space;
        
        /* get the command and file name and send MSG_ERR in case of error */
        if (sscanf(buf,"%3c%c%511c",op,&space,file) != 3) {
            trace( err_msg("(%s) --- wrong or missing commands",prog_name) );
            int len = strlen(MSG_ERR);
            int ret_send = sendn(connfd, MSG_ERR, len, MSG_NOSIGNAL);
            if (ret_send!=len) {
                trace( err_msg("(%s) - cannot send MSG_ERR",prog_name) );
            }
            //continue;
            return -1;
        }
        trace( err_msg("(%s) --- command: %s %s",prog_name,op,file) );

        /* verify command received */
        if (strcmp(op, MSG_GET) == 0 && space == ' ' && strncmp(file, ".", 1) != 0 && strncmp(file, "/", 1) != 0){
            trace( err_msg("(%s) --- begin to transfer '%s'",prog_name,file) );
            res = send_file(connfd, buf, file);
            trace( err_msg("(%s) --- end of transfer",prog_name) );
        } else {
            trace( err_msg("(%s) --- invalid command",prog_name) );
            int len = strlen(MSG_ERR);
            int ret_send = sendn(connfd, MSG_ERR, len, MSG_NOSIGNAL);
            if (ret_send!=len) {
                trace( err_msg("(%s) - cannot send MSG_ERR",prog_name) );
            }
            //continue;
            return -1;
        }
        trace( err_msg("(%s) --- result of the operation: %s ---", prog_name, (res==0)?"OK":"FAILED" ));

        /* send err msg */
        if (res != 0) {
            int len = strlen(MSG_ERR);
            int ret_send = sendn(connfd, MSG_ERR, len, MSG_NOSIGNAL);
            if (ret_send!=len) {
                trace( err_msg("(%s) - cannot send MSG_ERR",prog_name) );
            }
            return -1;        //
        }
        
    }
}



int main (int argc, char *argv[]) {
    
    int listenfd, connfd, err=0;
    short port;
    struct sockaddr_in servaddr, cliaddr;
    socklen_t cliaddrlen = sizeof(cliaddr);
    
    /* for errlib to know the program name */
    prog_name = argv[0];
    
    //signal(SIGPIPE, SIG_IGN);           /* ignore SIGPIPE (in macOS) */
    
    /* check arguments */
    if (argc!=2)
        err_quit ("usage: %s <port>\n", prog_name);
    port=atoi(argv[1]);
    
    /* create socket */
    listenfd = Socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    
    /* specify address to bind to */
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    Bind(listenfd, (SA*) &servaddr, sizeof(servaddr));
    
    trace ( err_msg("(%s) socket created",prog_name) );
    trace ( err_msg("(%s) listening on %s:%u", prog_name, inet_ntoa(servaddr.sin_addr), ntohs(servaddr.sin_port)) );
    
    Listen(listenfd, LISTENQ);
    
    while (1) {
        trace( err_msg ("(%s) waiting for connections...", prog_name) );
        
        int retry = 0;
        do {
            connfd = accept (listenfd, (SA*) &cliaddr, &cliaddrlen);
            if (connfd<0) {
                if (INTERRUPTED_BY_SIGNAL ||
                    errno == EPROTO || errno == ECONNABORTED ||
                    errno == EMFILE || errno == ENFILE ||
                    errno == ENOBUFS || errno == ENOMEM    ) {
                    retry = 1;
                    err_ret ("(%s) error - accept() failed", prog_name);
                } else {
                    err_ret ("(%s) error - accept() failed", prog_name);
                    return 1;
                }
            } else {
                trace ( err_msg("(%s) - new connection from client %s:%u", prog_name, inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port)) );
                retry = 0;
            }
        } while (retry);
        
        err = prot_a(connfd);
        
        close (connfd);
        trace( err_msg ("(%s) - connection closed by %s", prog_name, (err==0)?"client":"server") );
    }
    return 0;
}

