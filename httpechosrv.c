#include <stdio.h>
#include <stdlib.h>
#include <string.h>      /* for fgets */
#include <strings.h>     /* for bzero, bcopy */
#include <unistd.h>      /* for read, write */
#include <sys/socket.h>  /* for socket use */
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define MAXLINE  8192  /* max text line length */
#define MAXBUF   8192  /* max I/O buffer size */
#define LISTENQ  1024  /* second argument to listen() */

int open_listenfd(int port);
void echo(int connfd);
void *thread(void *vargp);

struct HttpRequest {
    char request_method[7];
    char http_version[10];
    char resource[100];
};
struct HttpResponse {
    char http_version[10];
    char status_code[4];
    char content_type[10];
    char content_length[10];
    char *content;
};

int main(int argc, char **argv) 
{
    int listenfd, *connfdp, port, clientlen=sizeof(struct sockaddr_in);
    struct sockaddr_in clientaddr;
    pthread_t tid; 

    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(0);
    }
    port = atoi(argv[1]);

    listenfd = open_listenfd(port);
    while (1) {
        connfdp = malloc(sizeof(int));
        *connfdp = accept(listenfd, (struct sockaddr*)&clientaddr, &clientlen);
        if (*connfdp < 1) {
            continue;
        }
        printf("connection made");
        pthread_create(&tid, NULL, thread, connfdp);
    }
}

/* thread routine */
void * thread(void * vargp) 
{
    int connfd = *((int *)vargp);
    pthread_detach(pthread_self()); 
    free(vargp);
    echo(connfd);
    close(connfd);
    return NULL;
}

struct HttpRequest getHttpAttributes(char *buf) {
    int i = 0, j = 0;
    char httpMethod[7], httpVersion[10], resource[100];
    char space = ' ';
    struct HttpRequest request;
    while (!(buf[i] == ' ')) {
        httpMethod[j] = buf[i];
        i++;j++;
    }
    httpMethod[j] = '\0';
    j = 0; i += 1;
    while(!(buf[i] == ' ')) {
        resource[j] = buf[i];
        i++;j++;
    }
    resource[j] = '\0';
    j = 0; i += 1;
    while(j < 8) {
        httpVersion[j] = buf[i];
        i++;j++;
    }
    httpVersion[j] = '\0';
    strcpy(request.http_version, httpVersion);
    strcpy(request.request_method, httpMethod);
    strcpy(request.resource, resource);
    return request;
}

struct HttpResponse getResponseContents(struct HttpRequest request) {
    char *base_path = "./www";
    char *path;
    path = malloc(100 * sizeof(char));
    FILE *fp;
    int file_not_found = 0;
    struct HttpResponse response;
    bzero(response.http_version, 8);
    strcpy(response.http_version, request.http_version);
    if (strcmp(request.resource, "/") == 0 || strcmp(request.resource, "/index.html") == 0 ) {
        strcat(path, base_path);
        strcat(path, "/index.html");
        fp = fopen(path, "r");
        if (fp == NULL) {
            file_not_found = 1;
        }
    } else {
        strcat(path, base_path);
        strcat(path, request.resource);
        fp = fopen(path, "r");
        if (fp == NULL) {
            file_not_found = 1;
        }
    }
    if (file_not_found) {
        strcpy(response.status_code, "404");
        char *contents = "<html><h1>File not found!</h1></html>";
        response.content = malloc(strlen(contents) * sizeof(char));
        strcpy(response.content, contents);
        sprintf(response.content_length, "%ld", strlen(contents));
    } else {
        fseek(fp, 0, SEEK_END);
        long fsize = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        char *file = malloc(fsize + 1);
        fread(file, fsize, 1, fp);
        fclose(fp);
        file[fsize] = 0;
        response.content = malloc(fsize * sizeof(char));
        strcpy(response.status_code, "200");
        strcpy(response.content, file);
        sprintf(response.content_length, "%ld", fsize);
    }
    return response;
}

/*
 * echo - read and echo text lines until client closes connection
 */
void echo(int connfd) 
{
    size_t n;
    char buf[MAXLINE]; 
    char httpmsg[] = "HTTP/1.1 200 Document Follows\r\nContent-Type:text/html\r\nContent-Length:32\r\n\r\n<html><h1>Hello CSCI4273 Course!</h1>";
    char *response_str;
    response_str = malloc(20000 * sizeof(char));
    struct HttpRequest request;
    struct HttpResponse response;
    n = read(connfd, buf, MAXLINE);
    if (n > 0) {
        request = getHttpAttributes(buf);
        response = getResponseContents(request);
        strcat(response_str, request.http_version);
        strcat(response_str, " ");
        strcat(response_str, response.status_code);
        strcat(response_str, " ");
        strcat(response_str, "Document Follows");
        strcat(response_str, "\r\n");
        strcat(response_str, "Content-Type:text/html\r\n");
        strcat(response_str, "Content-Length:");
        strcat(response_str, response.content_length);
        strcat(response_str, "\r\n\r\n");
        strcat(response_str, response.content);
        write(connfd, response_str, strlen(response_str));
    }
    
}

/* 
 * open_listenfd - open and return a listening socket on port
 * Returns -1 in case of failure 
 */
int open_listenfd(int port)
{
    int listenfd, optval=1;
    struct sockaddr_in serveraddr;
  
    /* Create a socket descriptor */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;

    /* Eliminates "Address already in use" error from bind. */
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, 
                   (const void *)&optval , sizeof(int)) < 0)
        return -1;

    /* listenfd will be an endpoint for all requests to port
       on any IP address for this host */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET; 
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    serveraddr.sin_port = htons((unsigned short)port); 
    if (bind(listenfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) < 0)
        return -1;

    /* Make it a listening socket ready to accept connection requests */
    if (listen(listenfd, LISTENQ) < 0)
        return -1;
    return listenfd;
} /* end open_listenfd */

