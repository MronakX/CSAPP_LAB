/* $begin tinymain */
/*
 * 1900012995 GuoDawei
 * proxy.c simple proxy that can connect between client and server
 * the basic structure is from tiny.c
 * 
 * client send request to proxy, format: request(only GET is legal) + uri + version + headers
 * then proxy resend it to server, include all headers. Version change to http1.0
 * some headers are changed too.
 * 
 * the proxy get all message from server, resend to client.
 * 
 * cache system: used a cache with 10 lines;
 * each line has a obj array, a uri array. 
 * Actually, uri is the tag of cache.
 * Use LRU stategy(same to csim.c in CacheLab)
 * add writer-reader lock system to avoid race(just from textbook).
 */
#include <stdio.h>
#include "csapp.h"
#include <stdlib.h>
//#include "sbuf.h"
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define CACHE_NUM 10
/* always send these headers told by Writeup */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *conn_hdr = "Connection: close\r\n";
static const char *prox_hdr = "Proxy-Connection: close\r\n";

/* some functions I use in proxy model*/
void doit(int fd);
void read_requesthdrs(rio_t *rp, char *req_header_buf);
int conn_server(char *hostname, char *port, char *http_header, int fd, char *uri);
int parse_uri(char *uri, char *hostname, char *suffix, char *port);
void clienterror(int fd, char *cause, char *errnum, 
        char *shortmsg, char *longmsg);
void *thread(void *vargp);

/* some functions for cache, all includes lock system */
void create_cache();
void LRU_update(int i);
void LRU_reset(int i);
int find_hit(char *uri);
int find_empty();
int find_evict();
int write_cache(int i, char *uri, char *obj, size_t size);
int trans_from_cache(int connfd, int i);

/* define cache struct */
typedef struct
{
	int valid;
	char obj[MAX_OBJECT_SIZE];
    char uri_tag[MAXLINE];
	int LRU_Counter;
    size_t size_of_obj;

    int readcnt;
    sem_t mutex, w;
}Cache_Line;

Cache_Line* cache_lines = NULL;

/* use malloc to create cache
 * initialize all things inside.
 */
void create_cache()
{
    cache_lines = (Cache_Line*)malloc(sizeof(Cache_Line) * CACHE_NUM);
    for(int i=0; i<CACHE_NUM; i++)
    {
        cache_lines[i].LRU_Counter = 0;
        cache_lines[i].valid = 0;
        cache_lines[i].readcnt = 0;
        cache_lines[i].size_of_obj = 0;
        memset(cache_lines[i].obj, 0, sizeof(cache_lines[i].obj));
        memset(cache_lines[i].uri_tag, 0, sizeof(cache_lines[i].uri_tag));

        sem_init(&cache_lines[i].mutex, 0, 1);
        sem_init(&cache_lines[i].w, 0, 1);
    }
}

/* all unused cache's LRU_counter++ */
void LRU_update(int i)
{
    P(&cache_lines[i].w);
    cache_lines[i].LRU_Counter++;
    V(&cache_lines[i].w);
}

/* used cache's LRU_counter = 0 */
void LRU_reset(int i)
{
    P(&cache_lines[i].w);
    cache_lines[i].LRU_Counter = 0;
    V(&cache_lines[i].w);
}

/* when uri hit, fetch it from cache */
int find_hit(char *uri)
{
    //int find_empty = 0;
    int find_hit = 0;
    int index;
    for(int i=0; i<CACHE_NUM; i++)
    {
        P(&cache_lines[i].mutex);
        cache_lines[i].readcnt++;
        if(cache_lines[i].readcnt == 1)    
            P(&cache_lines[i].w);
        V(&cache_lines[i].mutex);

        //printf("cache num %d: uri = %s\n", i, cache_lines[i].uri_tag);
        //printf("target_uri = %s\n", uri);
        if(strcmp(cache_lines[i].uri_tag, uri) == 0)//hit
        {
            //printf("%s\n", cache_lines[i].obj);
            find_hit = 1;
            index = i;
        }

        P(&cache_lines[i].mutex);
        cache_lines[i].readcnt--;
        if(cache_lines[i].readcnt == 0)
            V(&cache_lines[i].w);
        V(&cache_lines[i].mutex);

        LRU_update(i);

        if(find_hit)
            break;
    }
    if(find_hit)
    {
        for(int i=index; i<CACHE_NUM; i++)
            LRU_update(i);
        LRU_reset(index);
        return index;
    }
    return -1;
}

/* when uri miss, find a first empty cache*/
int find_empty()
{
    int find_empty = 0;
    int index = 0;
    for(int i=0; i<CACHE_NUM; i++)
    {
        P(&cache_lines[i].mutex);
        cache_lines[i].readcnt++;
        if(cache_lines[i].readcnt == 1)    
            P(&cache_lines[i].w);
        V(&cache_lines[i].mutex);

        if(cache_lines[i].valid == 0)//miss and !evict
        {
            find_empty = 1;
            index = i;
        }

        P(&cache_lines[i].mutex);
        cache_lines[i].readcnt--;
        if(cache_lines[i].readcnt == 0)
            V(&cache_lines[i].w);
        V(&cache_lines[i].mutex);

        LRU_update(i);

        if(find_empty)
            break;
    }   

    if(find_empty)
    {
        for(int i=index; i<CACHE_NUM; i++)
            LRU_update(i);
        return index;
    }
    return -1;
}

/* when uri miss and all cache is valid, then evict the rarely-used one */
int find_evict()
{
    int max_LRU = -1;
    int index = 0;
    for(int i=0; i<CACHE_NUM; i++)
    {
        P(&cache_lines[i].mutex);
        cache_lines[i].readcnt++;
        if(cache_lines[i].readcnt == 1)    
            P(&cache_lines[i].w);
        V(&cache_lines[i].mutex);

        if(cache_lines[i].LRU_Counter > max_LRU)
        {
            index = i;
            max_LRU = cache_lines[i].LRU_Counter;
        }

        P(&cache_lines[i].mutex);
        cache_lines[i].readcnt--;
        if(cache_lines[i].readcnt == 0)
            V(&cache_lines[i].w);
        V(&cache_lines[i].mutex);

        LRU_update(i);
    }   
    return index;
}

/* write(store) a website to cache_lines[i] */
int write_cache(int i, char *uri, char *obj, size_t size)
{
    P(&cache_lines[i].w);

    strcpy(cache_lines[i].uri_tag, uri);
    memcpy(cache_lines[i].obj, obj, size);

    cache_lines[i].LRU_Counter = 0;
    cache_lines[i].valid = 1;
    cache_lines[i].size_of_obj = size;
    V(&cache_lines[i].w);
    return 1;
}

/* cache match, then send the content in cache to client straightly */
int trans_from_cache(int connfd, int i)
{
    P(&cache_lines[i].mutex);
    cache_lines[i].readcnt++;
    if(cache_lines[i].readcnt == 1)    
        P(&cache_lines[i].w);
    V(&cache_lines[i].mutex);

    rio_writen(connfd, cache_lines[i].obj, cache_lines[i].size_of_obj);
    printf("trans succeed!\n");
    P(&cache_lines[i].mutex);
    cache_lines[i].readcnt--;
    if(cache_lines[i].readcnt == 0)
        V(&cache_lines[i].w);
    V(&cache_lines[i].mutex);
}

/* main just from tiny.c in textbook */
int main(int argc, char **argv) 
{
    signal(SIGPIPE, SIG_IGN);
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
    create_cache();
    /* multithread, just from textbook */
    while (1) 
    {
        clientlen = sizeof(clientaddr);
        connfdp = malloc(sizeof(long long));
        if((*connfdp = accept(listenfd, (SA *)&clientaddr, &clientlen)) < 0) //line:netp:tiny:accept
            continue;
        //printf("connfd = %d\n", *connfdp);
        pthread_create(&tid, NULL, thread, connfdp);
    }
}
/* $end tinymain */

/* every thread calls doit */
void *thread(void *vargp)
{
    int connfd = *((int *)vargp);
    pthread_detach(pthread_self());
    free(vargp);
    doit(connfd);
    close(connfd);
    return NULL;
}

/*
* doit - handle one HTTP request/response transaction
*/
/* $begin doit */
void doit(int fd) 
{
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char hostname[MAXLINE], suffix[MAXLINE], port[MAXLINE];
    char http_header[10*MAXLINE];
    char wasted[MAXLINE];
    rio_t rio;
    //printf("doit!\n");
    /* Read request line and headers */

    rio_readinitb(&rio, fd);
    size_t nn;
    if ((nn = rio_readlineb(&rio, buf, MAXLINE)) == 0)  //line:netp:doit:readrequest
        return;
    if(nn >= 4000)/* if uri >= 16kb, too long, return */
    {
        printf("uri too long\n");
        return;
    }

    //printf("%s", buf);
    char uri_before[MAXLINE];
    sscanf(buf, "%s %s %s", method, uri, version);       //line:netp:doit:parserequest
    int hit_num = -1;
    strcpy(uri_before, uri);
    /* if hit */
    if((hit_num = find_hit(uri)) != -1)
    {
        printf("find in cache\n");
        trans_from_cache(fd, hit_num);
        return;
    }

    char buf2[MAXLINE];
    size_t n;
    char *ptr;
    char key[MAXLINE];
    char value[MAXLINE];
    //printf("bp0\n");
    if (strcasecmp(method, "GET")) {                     //line:netp:doit:beginrequesterr
        clienterror(fd, method, "501", "Not Implemented",
                    "Tiny does not implement this method");
        return;
    } 
    
    /* parse all the other headers */
    while(1)
    {
        rio_readlineb(&rio, buf2, MAXLINE);
        //printf("buf2 = %s", buf2);
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
        if( (strcmp(key, "Host") != 0) && (strcmp(key, "Connection") != 0) && (strcmp(key, "User-Agent") != 0)
        && (strcmp(key, "Proxy-Connection") != 0) )
        {
            strcat(wasted, key);
            strcat(wasted, ":");
            strcat(wasted, value);   
        }
    }



    //printf("after\n");           

    /* Parse URI from GET request */
    if(parse_uri(uri, hostname, suffix, port) == -1)     //line:netp:doit:staticcheck
    {
        printf("parse uri error\n");
        return;
    }
    //printf("before get header\n");

    /* generate the header send to server */
    char *host_hdr = "Host: %s\r\n";
    char *GET_hdr = "GET %s HTTP/1.0\r\n";
    char host_buf[MAXLINE];
    char GET_buf[MAXLINE];
    //char *end_hdr = "\r\n";
    sprintf(host_buf, host_hdr, hostname);
    sprintf(GET_buf, GET_hdr, suffix);
    sprintf(http_header, "%s%s%s%s%s%s\r\n", GET_buf, host_buf, user_agent_hdr, conn_hdr, prox_hdr, wasted);
    //printf("before conn_server\n");


    if(conn_server(hostname, port, http_header, fd, uri_before) == -1)
    {
        printf("conn_server error\n");
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
        //printf("case1:\n");
        char *end_of_port = strstr(start_of_port, "/");
        if(!end_of_port)
            return -1;
        *start_of_port = '\0';
        *end_of_port = '\0';
        //printf("ptr = %s\n", ptr);
        strcpy(hostname, ptr);
        //printf("hostname = %s\n", hostname);
        strcpy(port, start_of_port+1);
        //printf("port = %s\n", port);
        *end_of_port = '/';
        strcpy(suffix, end_of_port);
        //printf("suffix = %s\n", suffix);
    }

    /* hostname + /hub/index.html */
    else
    {
        //printf("case2:\n");
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
        //printf("inseide parse_uri, bp3\n");
    }
    return 0;
}
/* $end parse_uri */

/* send webside content to client, and store it in proper cache */
int conn_server(char *hostname, char *port, char *http_header, int fd, char *uri)
{
    int proxy_fd;
    size_t n;
    //printf("hostname = %s\n", hostname);
    //printf("port = %s\n", port);

    /* I change open_clientfd in csapp.c
     * when called getaddrname(Get~ originally) in open_clientfd, and had a error
     * it called "return -1" instead of "exit(1)"
     * so the proxy can still running
     */
    if((proxy_fd = open_clientfd(hostname, port)) < 0)//proxy -> server
    {
        printf("open_clientfd failed\n");
        return -1;
    }
    char buf2[MAXLINE];
    //printf("proxy_fd = %d\n", proxy_fd);
    rio_t rio;
    rio_readinitb(&rio, proxy_fd);
    //printf("conn-bp1\n");
    rio_writen(proxy_fd, http_header, MAXLINE);//qing qiu

    char obj[MAX_OBJECT_SIZE];
    size_t cnt = 0;
    while((n=rio_readlineb(&rio,buf2,MAXLINE))!=0)//
    {
        rio_writen(fd,buf2,n);
        if(cnt+n < MAX_OBJECT_SIZE)
            memcpy(obj+cnt, buf2, n); /* strcpy -> memcpy */
        cnt += n;
        //printf("%s", buf2);
    }
    close(proxy_fd);
    //printf("obj = %s\n", obj);
    int index = -1;
    /* cache had its memory limit */
    if(cnt < MAX_OBJECT_SIZE)
    {
        if((index = find_empty()) == -1)
        {
            index = find_evict();
        }
        write_cache(index, uri, obj, cnt);
        //printf("cached!\n");
    }
    return 1;
}

/* print error msg, just from tiny.c */
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