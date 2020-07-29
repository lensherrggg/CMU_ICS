# ProxyLab

## 实验说明

本实验要求实现一个并发Web代理。涉及的知识点有

+ client-server模型
+ socket编程
+ Web服务器基础和HTTP协议基础
+ 多线程并发编程，读-写者模型
+ Cache相关知识（可选）

实验分为三步，第一步实现基本的转发，第二部实现并发，第三部加入cache功能。

调试./proxy的方法

+ 可以用`curl`这个工具来进行测试，例如:`curl -v --proxy http://localhost:15214 http://localhost:15213/home.html`就是向代理`http://localhost:15214`发送请求，得到后面那个uri的资源

## Part 1

第一部分大概需要做如下编程工作。服务器端接受请求，解析`GET http://www.cmu.edu/hub/index.html HTTP/1.1` 转换为 `GET /hub/index.html HTTP/1.0`, 同时拿到HOST 和 PORT，代理服务器自己作为CLIENT向目标发送HTTP 1.0请求。

第一部分的整体框架与课本Tiny Server类似

```c
#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *conn_hdr = "Connection: close\r\n";
static const char *prox_hdr = "Proxy-Connection: close\r\n";

void doit(int client_fd);
void build_requesthdrs(rio_t *rp, char *newreq, char *hostname);
void parse_uri(char *uri, char *hostname, char *path, int *port);
void *thread(void *vargp);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

int main(int argc, char **argv)
{
    int listenfd, connfd;
    // pthread_t tid;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    /* Check command-line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    signal(SIGPIPE, SIG_IGN);
    listenfd = Open_listenfd(argv[1]);

    while (1) {
        clientlen = sizeof(clientaddr);
        // connfd = Malloc(sizeof(int));
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

        /* Print accepted message */
        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);

        /* Sequential handle the lcient transaction */
        // Pthread_create(&tid, NULL, thread, connfd);
        doit(connfd);
        Close(connfd);
    }
}
```



```c
/* Thread routine */
void *thread(void *vargp)
{
    int connfd = *((int *)vargp);
    Pthread_detach(pthread_self());
    Free(vargp);
    doit(connfd);
    Close(connfd);
    return NULL;
}
```

主要处理函数部分doit()需要设置两个rio结构体from_client和to_endserver，一个用于接收和读取客户端发来的消息，一个用于将消息转发给服务器。

```c
/* Handle the client HTTP transaction */
void doit(int client_fd) 
{
    int endserver_fd;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char hostname[MAXLINE], path[MAXLINE];
    int port;
    rio_t from_client, to_endserver;

    /* Read request line and headers */
    Rio_readinitb(&from_client, client_fd);
    Rio_readlineb(&from_client, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version);

    if (strcasecmp(method, "GET")) {
        clienterror(client_fd, method, "501", "Not Implemented", "Proxy Server does not implement this method");
        return;
    }

    /* Parse URI then open a clientfd */
    parse_uri(uri, hostname, path, &port);
    char port_str[10];
    sprintf(port_str, "%d", port);
    
    endserver_fd = Open_clientfd(hostname, port_str);
    if (endserver_fd < 0) {
        printf("connection failed\n");
        return;
    }

    Rio_readinitb(&to_endserver, endserver_fd);

    char newreq[MAXLINE]; /* For endserver HTTP request headers */

    /* Set up first line eg.GET /hub/index.html HTTP/1.0 */
    sprintf(newreq, "GET %s HTTP/1.0\r\n", path);
    build_requesthdrs(&from_client, newreq, hostname);

    /* Send client header to real server */
    Rio_writen(endserver_fd, newreq, strlen(newreq));
    int n;

    while ((n = Rio_readlineb(&to_endserver, buf, MAXLINE)) > 0) {
        Rio_writen(client_fd, buf, n);
    }
  
  	Close(endserver_fd);
}
```

解析URI时要注意区分是否指明协议、有无端口号。

```c
/* Parse the URI to get hostname, file path and port */
void parse_uri(char *uri, char *hostname, char *path, int *port)
{
    *port = 80;
    /* uri http://www.cmu.edu/hub/index.html */
    char *pos1 = strstr(uri, "//");
    if (pos1 == NULL) {
        pos1 = uri;
    }
    else {
        pos1 += 2;
    }

    /* pos1 www.cmu.edu:8080/hub/index.html, pos2 :8080/hub/index.html */
    char *pos2 = strstr(pos1, ":");
    if (pos2 != NULL) {
        *pos2 = '\0';
        // strncpy(hostname, pos1, MAXLINE);
        sscanf(pos1, "%s", hostname);
        sscanf(pos2 + 1, "%d%s", port, path);
        *pos2 = ':';
    }
    else {
        /* pos2 /hub/index.html */
        pos2 = strstr(pos1, "/");
        /* pos1 www.cmu.edu */
        if (pos2 == NULL) {
            strncpy(hostname, pos1, MAXLINE);
            strcpy(path, "");
            return;
        }
        pos2 = '\0';
        strncpy(hostname, pos1, MAXLINE);
        *pos2 = '/';
        strncpy(path, pos2, MAXLINE);
    }
}
```



```c
/* Create the Request Header */
void build_requesthdrs(rio_t *rp, char *newreq, char *hostname)
{
    char buf[MAXLINE];

    while (Rio_readlineb(rp, buf, MAXLINE) > 0) {
        if (!strcmp(buf, "\r\n")) {
            break;
        }
        if (strstr(buf, "Host:") != NULL) {
            continue;
        }
        if (strstr(buf, "User-Agent:") != NULL) {
            continue;
        }
        if (strstr(buf, "Connection:") != NULL) {
            continue;
        }
        if (strstr(buf, "Proxy-Connection:") != NULL) {
            continue;
        }
        sprintf(newreq, "%s%s", newreq, buf);
        sprintf(newreq, "%sHost: %s\r\n", newreq, hostname);
        sprintf(newreq, "%s%s%s%s", newreq, user_agent_hdr, conn_hdr, prox_hdr);
        sprintf(newreq, "%s\r\n", newreq);
    }
}
```



```c
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
    char buf[MAXLINE], body[MAXBUF];
    
    /* Build the HTTP response body */
    sprintf(body, "<html><title>Proxy Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Proxy Web server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}
```

测试结果

```shell
$ curl -v --proxy http://localhost:15213 http://localhost:15214/home.html
*   Trying 127.0.0.1...
* TCP_NODELAY set
* Connected to localhost (127.0.0.1) port 15213 (#0)
> GET http://localhost:15214/home.html HTTP/1.1
> Host: localhost:15214
> User-Agent: curl/7.58.0
> Accept: */*
> Proxy-Connection: Keep-Alive
> 
* HTTP 1.0, assume close after body
< HTTP/1.0 200 OK
< Server: Tiny Web Server
< Content-length: 120
< Content-type: text/html
< 
<html>
<head><title>test</title></head>
<body> 
<img align="middle" src="godzilla.gif">
Dave O'Hallaron
</body>
</html>
* Closing connection 0
```



## Part 2

第二部分需要使代理能够并发的处理多个请求。参考课本12.5.5的案例。

注意：

+ 线程需要工作在detached模式防止内存泄漏
+ open_clientfd和open_listenfd是线程安全的

```c
int main(int argc, char **argv)
{
    int listenfd, *connfd;
    pthread_t tid;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    /* Check command-line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    signal(SIGPIPE, SIG_IGN);
    listenfd = Open_listenfd(argv[1]);

    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Malloc(sizeof(int));
        *connfd = Accept(listenfd, (SA *) &clientaddr, &clientlen);

        /* Print accepted message */
        Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);

        /* Sequential handle the client transaction */
        // doit(connfd);
        // Close(connfd);

        /* Multithreading */
        Pthread_create(&tid, NULL, thread, (void *) connfd);
    }
}
```

```c
/* Thread routine */
void *thread(void *vargp)
{
    int connfd = *((int *)vargp);
    /* 线程分离，让这个线程计数结束后自己回收资源 */
    Pthread_detach(pthread_self());
    Free(vargp);
    doit(connfd);
    Close(connfd);
    return NULL;
}
```



## Part 3

第三部分要求代理服务器能够将最近使用的Web对象缓存于内存，这样如果之前你完成了一次代理，之后又有一个客户端向相同的服务器进行相同的请求，你就只需要发送缓存的对象而无需再次与该服务器连接。

要实现Cache的方法，
 决定使用数组的方法，为了不浪费空间，决定采用分级数组的思想。（和MALLOC LAB很想）
 因为最大缓存对象是100KB， 一共有1M的缓存空间。
 我可以用5个100KB （500 KB）
 25 KB 可以用12个。(300 KB）
 随后10KB 可以用10个。 （100KB）
 还有5KB的用20个，（100 KB）
 1 KB 用 20个（20 KB）
 100B的 用40个 （4KB）

cache.h

```c
#include "csapp.h"
#include <sys/time.h>

#define TYPES 6
extern const int cache_block_size[];
extern const int cache_cnt[];

typedef struct cache_block {
    char *url;
    char *data;
    int datasize;
    int64_t time;
    pthread_rwlock_t rwlock;
} cache_block;

typedef struct cache_type{
    cache_block *cacheobjs;  
    int size;
} cache_type;


cache_type caches[TYPES];

//intialize cache with malloc
void init_cache();
//if miss cache return 0, hit cache write content to fd
int read_cache(char* url, int fd);
//save value to cache
void write_cache(char* url, char* data, int len);
//free cache
void free_cache();
```



cache.c

```c
#include "cache.h"

const int cache_block_size[] = {102, 1024, 5120 ,10240,25600, 102400};
const int cache_cnt[] = {40,20,20,10,12,5};
int64_t currentTimeMillis();

void init_cache()
{
    int i;
    for (i = 0; i < TYPES; i++) {
        caches[i].size = cache_cnt[i];
        caches[i].cacheobjs = (cache_block *)malloc(cache_cnt[i] * sizeof(cache_block));
        cache_block *j = caches[i].cacheobjs;
        int k;
        for (k = 0; k < cache_cnt[i]; j++, k++) {
            j->time = 0;
            j->datasize = 0;
            j->url = malloc(sizeof(char) * MAXLINE);
            strcpy(j->url, "");
            j->data = malloc(sizeof(char) * cache_block_size[i]);
            memset(j->data, 0, cache_block_size[i]);
            pthread_rwlock_init(&j->rwlock, NULL);
        }
    }
}

void free_cache()
{
    int i;
    for (i = 0; i < TYPES; i++) {
        cache_block *j = caches[i].cacheobjs;
        int k;
        for (k = 0; k < cache_cnt[i]; j++, k++) {
            free(j->url);
            free(j->data);
            pthread_rwlock_destroy(&j->rwlock);
        }
        free(caches[i].cacheobjs);
    }
}

int read_cache(char *url,int fd)
{
    int tar = 0, i = 0;
    cache_type cur;
    cache_block *p;
    printf("read cache %s \n", url);
    for (; tar < TYPES; tar++) {
        cur = caches[tar];
        p = cur.cacheobjs;
        for(i = 0; i < cur.size; i++, p++){
            if(p->time != 0 && strcmp(url, p->url) == 0) break;
        }
        if (i < cur.size) break;     
    }

    if(i == cur.size){
        printf("read cache fail\n");
        return 0;
    }
    pthread_rwlock_rdlock(&p->rwlock);
    if(strcmp(url, p->url) != 0){
        pthread_rwlock_unlock(&p->rwlock);
        return 0;
    }
    pthread_rwlock_unlock(&p->rwlock);
    if (!pthread_rwlock_trywrlock(&p->rwlock)) {
        p->time = currentTimeMillis();
        pthread_rwlock_unlock(&p->rwlock); 
    }
    pthread_rwlock_rdlock(&p->rwlock);
    Rio_writen(fd, p->data, p->datasize);
    pthread_rwlock_unlock(&p->rwlock);
    printf("read cache successful\n");
    return 1;
}

void write_cache(char *url, char *data, int len){
    int tar;
    for (tar = 0; tar < TYPES && len > cache_block_size[tar]; tar++) ;
    printf("write cache %s %d\n", url, tar);
    /* find empty block */
    cache_type cur = caches[tar];
    cache_block *p = cur.cacheobjs, *pt;
    int i;
    for(i = 0; i < cur.size; i++, p++){
        if(p->time == 0){
            break;
        }
    }
    
    /* find last visited */
    int64_t min = currentTimeMillis();
    if(i == cur.size) {
        for(i = 0,pt = cur.cacheobjs; i < cur.size; i++, pt++) {
            pthread_rwlock_rdlock(&pt->rwlock);
            if(pt->time <= min){
                min = pt->time;
                p = pt;
            }
            pthread_rwlock_unlock(&pt->rwlock);
        }
    }
    pthread_rwlock_wrlock(&p->rwlock);
    p->time = currentTimeMillis();
    p->datasize = len;
    memcpy(p->url, url,MAXLINE);
    memcpy(p->data, data, len);
    pthread_rwlock_unlock(&p->rwlock);
    printf("write Cache\n");
}

int64_t currentTimeMillis() 
{
  struct timeval time;
  gettimeofday(&time, NULL);
  int64_t s1 = (int64_t)(time.tv_sec) * 1000;
  int64_t s2 = (time.tv_usec / 1000);
  return s1 + s2;
}

```

修改makefile

```makefile
all: proxy

csapp.o: csapp.c csapp.h
	$(CC) $(CFLAGS) -c csapp.c

proxy.o: proxy.c csapp.h
	$(CC) $(CFLAGS) -c proxy.c

proxy: proxy.o csapp.o
	$(CC) $(CFLAGS) proxy.o csapp.o -o proxy $(LDFLAGS)
```

改为

```makefile
all: proxy

csapp.o: csapp.c csapp.h
	$(CC) $(CFLAGS) -c csapp.c

cache.o: cache.c cache.h
	$(CC) $(CFLAGS) -c cache.c

proxy.o: proxy.c csapp.h
	$(CC) $(CFLAGS) -c proxy.c

proxy: proxy.o csapp.o cache.o
	$(CC) $(CFLAGS) proxy.o cache.o csapp.o -o proxy $(LDFLAGS)
```

在proxy.c中

```c
int main(int argc, char **argv)
{
    int listenfd, *connfd;
    pthread_t tid;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    /* Check command-line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    signal(SIGPIPE, SIG_IGN);
    init_cache();
    listenfd = Open_listenfd(argv[1]);

    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Malloc(sizeof(int));
        *connfd = Accept(listenfd, (SA *) &clientaddr, &clientlen);

        /* Print accepted message */
        Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);

        /* Sequential handle the client transaction */
        // doit(connfd);
        // Close(connfd);

        /* Multithreading */
        Pthread_create(&tid, NULL, thread, (void *) connfd);
    }

    Close(listenfd);
    free_cache();
    return 0;
}
```

doit()

```c
    ...
    if (strcasecmp(method, "GET")) {
        clienterror(client_fd, method, "501", "Not Implemented", "Proxy Server does not implement this method");
        return;
    }

    /* read cache */
    int ret = read_cache(uri, client_fd);
    if (ret == 1) {
        return;
    }

    ...
    Rio_writen(endserver_fd, newreq, strlen(newreq));
    int n;
    int size = 0;
    char data[MAX_OBJECT_SIZE];

    while ((n = Rio_readlineb(&to_endserver, buf, MAXLINE)) > 0) {
        if (size <= MAX_OBJECT_SIZE) {
            memcpy(data + size, buf, n);
            size += n;
        }
        printf("proxy received %d bytes, then send\n", n);
        Rio_writen(client_fd, buf, n);
    }
    printf("size: %d\n", size);
    if (size <= MAX_OBJECT_SIZE) {
        write_cache(uri, data, size);
    }

    Close(endserver_fd);
```

