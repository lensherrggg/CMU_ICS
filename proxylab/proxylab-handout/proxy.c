#include <stdio.h>
#include "csapp.h"
#include "cache.h"

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
    // Rio_readlineb(&from_client, buf, MAXLINE);
    if (!Rio_readlineb(&from_client, buf, MAXLINE)) {
        return;
    }
    sscanf(buf, "%s %s %s", method, uri, version);

    if (strcasecmp(method, "GET")) {
        clienterror(client_fd, method, "501", "Not Implemented", "Proxy Server does not implement this method");
        return;
    }

    /* read cache */
    int ret = read_cache(uri, client_fd);
    if (ret == 1) {
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
}

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