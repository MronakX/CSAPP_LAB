/* $begin tinymain */
/*
* tiny.c - A simple, iterative HTTP/1.0 Web server that uses the 
*     GET method to serve static and dynamic content.
*/
#include <stdio.h>
#include "csapp.h"
#include <stdlib.h>
//#include "sbuf.h"
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
//#define SBUFSIZE 128
//#define NTHREADS 128
/* always send these headers told by Writeup */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *conn_hdr = "Connection: close\r\n";
static const char *prox_hdr = "Proxy-Connection: close\r\n";
static int default_port = 80;

void doit(int fd);
void read_requesthdrs(rio_t *rp, char *req_header_buf);
void get_http_header(char *http_header, char *hostname, char *suffix, char *wasted);
int conn_server(char *hostname, char *port, char *http_header, int fd, char *buf);
int parse_uri(char *uri, char *hostname, char *suffix, char *port);
void clienterror(int fd, char *cause, char *errnum, 
        char *shortmsg, char *longmsg);
void *thread(void *vargp);
void *create_cache();

void sigchld_handler(int sig) { // reap all children
    int bkp_errno = errno;
    while(waitpid(-1, NULL, WNOHANG)>0);
    errno=bkp_errno;
}

typedef struct
{
	int valid;
	int tag;
	int LRU_Counter;
}Cache_Line;


/* main just from tiny.c in textbook */
int main(int argc, char **argv) 
{
    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, sigchld_handler);
    printf("returned to main\n");
    int listenfd, *connfdp;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    printf("bp0\n");
    /* Check command line args */
    if (argc != 2) 
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    listenfd = open_listenfd(argv[1]);
    while (1) 
    {
        clientlen = sizeof(clientaddr);
        connfdp = Malloc(sizeof(int));
        if((*connfdp = accept(listenfd, (SA *)&clientaddr, &clientlen)) < 0) //line:netp:tiny:accept
            continue;
        printf("connfd = %d\n", *connfdp);
        if(getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0) != 0)
            continue;
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        Pthread_create(&tid, NULL, thread, connfdp);
    }
}
/* $end tinymain */

void *thread(void *vargp)
{
    int connfd = *((int *)vargp);
    Pthread_detach(pthread_self());
    Free(vargp);
    doit(connfd);
    Close(connfd);
}
/*
* doit - handle one HTTP request/response transaction
*/
/* $begin doit */
void doit(int fd) 
{
    char buf[1<<15], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char hostname[MAXLINE], suffix[MAXLINE], port[MAXLINE];
    char http_header[MAXLINE];
    char wasted[MAXLINE];
    rio_t rio;
    printf("doit!\n");
    /* Read request line and headers */
    Rio_readinitb(&rio, fd);
    size_t nn;
    if ((nn = Rio_readlineb(&rio, buf, 1<<15)) == 0)  //line:netp:doit:readrequest
        return;
    if(nn > (1<<14))
    {
        printf("url too long\n");
        return;
    }
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version);       //line:netp:doit:parserequest
    char buf2[MAXLINE];
    size_t n;
    char *ptr;
    char key[MAXLINE];
    char value[MAXLINE];
    printf("bp0\n");
    if (strcasecmp(method, "GET")) {                     //line:netp:doit:beginrequesterr
        clienterror(fd, method, "501", "Not Implemented",
                    "Tiny does not implement this method");
        return;
    } 
    while(1)
    {
        /*  */
        Rio_readlineb(&rio, buf2, MAXLINE);
        printf("buf2 = %s", buf2);
        if(buf2[0] == '\r' && buf2[1] == '\n')
            break;
        if(!(ptr = strstr(buf2, ":")))
        {
            printf("invalid addr\n");
            return;
        }
        *ptr = '\0';
        strcpy(key, buf2);
        *ptr = ':';
        strcpy(value, ptr+1);

        strcat(wasted, key);
        strcat(wasted, ":");
        strcat(wasted, value);
    }
    printf("after\n");                   
    printf("before parse_uri\n");
    /* Parse URI from GET request */
    if(parse_uri(uri, hostname, suffix, port) == -1)     //line:netp:doit:staticcheck
        return;
    printf("before get header\n");
    get_http_header(http_header, hostname, suffix, wasted);
    printf("before conn_server\n");
    if(conn_server(hostname, port, http_header, fd, buf) == -1)
    {
        printf("return from conn_server\n");
        return;
    }
}
/* $end doit */

/*
* parse_uri - 
* uri = "http://www.cmu.edu:8080/hub/index.html"
* hostname = www.cmu.edu
* suffix = /hub/index.html
* port = 8080
*/
/* $begin parse_uri */  
int parse_uri(char *uri, char *hostname, char *suffix, char *port) 
{
    char *ptr = uri;
    if(!strstr(ptr, "http://"))
    {
        printf("%s is not a legal request", uri);
        return -1;
    }
    ptr += 7;
    char *start_of_port;
    /* hostname + : + port + /hub/index.html */
    if((start_of_port = strstr(ptr, ":")))
    {
        printf("case1:\n");
        char *end_of_port = strstr(start_of_port, "/");
        *start_of_port = '\0';
        *end_of_port = '\0';
        printf("ptr = %s\n", ptr);
        strcpy(hostname, ptr);
        printf("hostname = %s\n", hostname);
        strcpy(port, start_of_port+1);
        printf("port = %s\n", port);
        *end_of_port = '/';
        strcpy(suffix, end_of_port);
        printf("suffix = %s\n", suffix);
        printf("inside parse_uri, bp2\n");
    }
    /* hostname + /hub/index.html */
    else
    {
        printf("case2:\n");
        char * start_of_others;
        strcpy(port, "80");
        if((start_of_others = strstr(ptr, "/")))
        {
            *start_of_others = '\0';
            strcpy(hostname, ptr);
            *start_of_others = '/';
            strcpy(suffix, start_of_others);
        }
        /* hostname + \0*/
        else
            strcpy(hostname, ptr);

        printf("inseide parse_uri, bp3\n");
    }
    return 0;
}
/* $end parse_uri */

void get_http_header(char *http_header, char *hostname, char *suffix, char *wasted)
{
    char *host_hdr = "Host: %s\r\n";
    char *GET_hdr = "GET %s HTTP/1.0\r\n";
    char host_buf[MAXLINE];
    char GET_buf[MAXLINE];
    char *end_hdr = "\r\n";
    sprintf(host_buf, host_hdr, hostname);
    sprintf(GET_buf, GET_hdr, suffix);
    sprintf(http_header, "%s%s%s%s%s%s\r\n", GET_buf, host_buf, user_agent_hdr, conn_hdr, prox_hdr, wasted);
    /*
    sprintf(http_header, host_hdr, hostname);
    sprintf(http_header, user_agent_hdr);
    sprintf(http_header, conn_hdr);
    sprintf(http_header, prox_hdr);
    sprintf(http_header, "\r\n");
    */
    printf("http_header = %s", http_header);
}

int conn_server(char *hostname, char *port, char *http_header, int fd, char* buf)
{
    int proxy_fd;
    size_t n;
    struct addrinfo hints, *listp, *p;
    printf("hostname = %s\n", hostname);
    printf("port = %s\n", port);
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_socktype = SOCK_STREAM;  /* Open a connection */
    hints.ai_flags = AI_NUMERICSERV;  /* ... using a numeric port arg. */
    hints.ai_flags |= AI_ADDRCONFIG;  /* Recommended for connections */
    if(getaddrinfo(hostname, port, &hints, &listp)!= 0)
    {
        printf("mygetaddr error\n");
        return -1;
    }
    if((proxy_fd = open_clientfd(hostname, port)) < 0)//proxy -> server
    {
        printf("open_clientfd failed\n");
        return -1;
    }
    char buf2[MAXLINE];
    printf("proxy_fd = %d\n", proxy_fd);
    rio_t rio;
    Rio_readinitb(&rio, proxy_fd);
    printf("conn-bp1\n");
    Rio_writen(proxy_fd, http_header, MAXLINE);//qing qiu
    //Close(clientfd);
    while((n=Rio_readlineb(&rio,buf2,MAXLINE))!=0)//
    {
        printf("%s", buf2);
        Rio_writen(fd,buf2,n);
    }
    return 1;
}

void clienterror(int fd, char *cause, char *errnum, 
        char *shortmsg, char *longmsg) 
{
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    rio_writen(fd, buf, strlen(buf));
    rio_writen(fd, body, strlen(body));
}