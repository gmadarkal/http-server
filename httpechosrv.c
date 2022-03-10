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
#include <sys/time.h>
#include <dirent.h>
#include <time.h>

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
    char host[100];
    char connection_state[50];
};
struct HttpResponse {
    char http_version[10];
    char status_code[4];
    char content_type[20];
    char content_length[10];
    char *content;
    long length;
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
        connfdp = malloc(4 * sizeof(int));
        *connfdp = accept(listenfd, (struct sockaddr*)&clientaddr, &clientlen);
        if (*connfdp < 1) {
            continue;
        }
        printf("----------------connection made---------------- \n");
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
    printf("-----------closing connection----------------- \n");
    close(connfd);
    return NULL;
}

struct HttpRequest getHttpAttributes(char *buf) {
    int i = 0, j = 0;
    char httpMethod[7], httpVersion[10], resource[100], host[100], connection_state[50];
    // memset(connection_state, 0, 50);
    // memset(httpMethod, 0, 7);
    // memset(httpVersion, 0, 10);
    // memset(resource, 0, 100);
    // memset(host, 0, 100);
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
    j = 0; i += 2;
    if (buf[i] == 'H') {
        // "Host: "
        i += 6;
        while(!(buf[i] == '\r')) {
            host[j] = buf[i];
            j += 1; i += 1;
        }
        host[j] = '\0';
        strcpy(request.host, host);
    }
    j = 0;
    int len = strlen(buf);
    while (i < len && (buf[i] != 'C' || buf[i+1] != 'o' || buf[i+2] != 'n')) {
        i++;
    }
    if (i < len) {
        // "Connection: "
        i += 12;
        while(!(buf[i] == '\r')) {
            connection_state[j] = buf[i];
            j += 1; i += 1;
        }
        connection_state[j] = '\0';
    }
    strcpy(request.connection_state, connection_state);
    strcpy(request.http_version, httpVersion);
    strcpy(request.request_method, httpMethod);
    strcpy(request.resource, resource);
    return request;
}

struct HttpResponse getResponseContents(struct HttpRequest request) {
    char *base_path = "./www";
    char *path;
    path = malloc(1000 * sizeof(char));
    FILE *fp;
    int file_not_found = 0;
    struct HttpResponse response;
    bzero(response.http_version, 8);
    strcpy(response.http_version, request.http_version);
    if (strcmp(request.resource, "/") == 0 || strcmp(request.resource, "/index.html") == 0 ) {
        memset(path, 0, 1000);
        strcat(path, base_path);
        strcat(path, "/index.html");
        fp = fopen(path, "r");
        if (fp == NULL) {
            file_not_found = 1;
        }
    } else {
        memset(path, 0, 1000);
        strcat(path, base_path);
        strcat(path, request.resource);
        printf("requested file %s \n", path);
        fp = fopen(path, "rb");
        if (fp == NULL) {
            file_not_found = 1;
        }
    }
    if (file_not_found) {
        printf("%s, file not found \n", request.resource);
        strcpy(response.status_code, "404");
        response.status_code[4] = '\0';
        char *contents = "<html><head><title>404 File Not Found</title></head><body><h2>404 File Not Found</h2></body></html>";
        response.content = malloc(strlen(contents) * sizeof(char));
        strcpy(response.content, contents);
        strcpy(response.content_type, "text/html");
        sprintf(response.content_length, "%ld", strlen(contents));
        response.length = strlen(contents);
    } else {
        char *file_type = malloc(10 * sizeof(char));
        char *resource = malloc(1000);
        memset(resource, 0, 1000);
        strcpy(resource, request.resource);
        int i = strlen(resource) - 1;
        if (strlen(resource) == 1 && resource[0] == '/') {
            file_type = "html";
        } else {
            while(resource[i] != '.') {
                i--;
            }
            i += 1; int j = 0;
            while(resource[i] != '\0') {
                file_type[j] = resource[i];
                i++;j++;
            }
            file_type[j] = '\0';
        }
        printf("requested file type %s \n", file_type);
        memset(response.content_type, 0, 20);
        if (strcmp(file_type, "html") == 0) {
            char *content_type = "text/html";
            strcpy(response.content_type, content_type);
        } else if (strcmp(file_type, "pdf") == 0) {
            char *content_type = "text/pdf";
            strcpy(response.content_type, content_type);
        } else if (strcmp(file_type, "txt") == 0) {
            char *content_type = "text/plain";
            strcpy(response.content_type, content_type);
        } else if (strcmp(file_type, "gif") == 0) {
            char *content_type = "image/gif";
            strcpy(response.content_type, content_type);
        } else if (strcmp(file_type, "jpg") == 0) {
            char *content_type = "image/jpg";
            strcpy(response.content_type, content_type);
        } else if (strcmp(file_type, "png") == 0) {
            char *content_type = "image/png";
            strcpy(response.content_type, content_type);
        } else if (strcmp(file_type, "css") == 0) {
            char *content_type = "text/css";
            strcpy(response.content_type, content_type);
        } else if (strcmp(file_type, "js") == 0) {
            char *content_type = "text/javascript";
            strcpy(response.content_type, content_type);
        } else if (strcmp(file_type, "ico") == 0) {
            strcpy(response.content_type, "image/x-icon");
        } 
        else {
            strcpy(response.content_type, "");
        }
        fseek(fp, 0, SEEK_END);
        long fsize = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        char *file = malloc(fsize + 1);
        if (file != NULL) {
            fread(file, fsize, 1, fp);
            fclose(fp);
            file[fsize] = 0;
            response.length = fsize;
            response.content = malloc(fsize + 1);
            char *status = "200";
            // memset(response.status_code, 0, 4);
            strcpy(response.status_code, status);
            memcpy(response.content, file, fsize);
            // memset(response.content_length, 0, 10);
            sprintf(response.content_length, "%ld", fsize);
        } else {
            strcpy(response.status_code, "404");
            char *contents = "<html><head><title>404 File Not Found</title></head><body><h2>404 File Not Found</h2></body></html>";
            response.content = malloc(strlen(contents) * sizeof(char));
            strcpy(response.content, contents);
            strcpy(response.content_type, "text/html");
            sprintf(response.content_length, "%ld", strlen(contents));
            response.length = strlen(contents);
        }
    }
    return response;
}

struct HttpResponse postResponseContents(struct HttpRequest request) {
    struct HttpResponse response = getResponseContents(request);
    if (strcmp(response.status_code, "500") == 0) {
        return response;
    }
    char *content = malloc(response.length + 200);
    strcpy(content, response.content);
    memset(response.content, 0, response.length);
    char *header = "<html><body><pre><h1>POSTDATA </h1></pre>";
    response.length += strlen(header);
    sprintf(response.content_length, "%ld", response.length);
    strcat(response.content, header);
    strcat(response.content, content);
    return response;
}

void clear(struct HttpRequest req) {
    int len = strlen(req.resource);
    memset(req.connection_state, 0, strlen(req.connection_state));
    memset(req.host, 0, strlen(req.host));
    memset(req.http_version, 0, strlen(req.http_version));
    memset(req.resource, 0, strlen(req.resource));
    memset(req.request_method, 0, strlen(req.request_method));
}
/*
 * echo - read and echo text lines until client closes connection
 */
void echo(int connfd)
{
    size_t n;
    char buf[MAXLINE]; 
    char *response_str;
    response_str = malloc(200000 * sizeof(char));
    struct HttpRequest request;
    struct HttpResponse response;
    int keepAlive = 0; int is_first = 1;
    n = read(connfd, buf, MAXLINE);
    if (n > 0 && strlen(buf) > 1) {
        while((keepAlive == 1 || is_first == 1) && strlen(buf) > 1) {
            printf("Request made on socket: %d \n", connfd);
            // clear(request);
            // memset(response_str, 0, 200000 * sizeof(char));
            request = getHttpAttributes(buf);
            if (strcmp(request.connection_state, "Keep-alive") == 0 || strcmp(request.connection_state, "keep-alive") == 0) {
                keepAlive = 1;
            } else {
                keepAlive = 0;
            }
            if (strcmp(request.request_method, "GET") == 0 || strcmp(request.request_method, "Get") == 0) {
                response = getResponseContents(request);
            } else if (strcmp(request.request_method, "POST") == 0 || strcmp(request.request_method, "Post") == 0) {
                response = postResponseContents(request);
            } else if (strcmp(request.request_method, "HEAD") == 0 || strcmp(request.request_method, "Head") == 0) {
                response = getResponseContents(request);
            } else {

            }
            strcat(response_str, request.http_version);
            strcat(response_str, " ");
            strcat(response_str, response.status_code);
            strcat(response_str, " ");
            strcat(response_str, "Document Follows");
            strcat(response_str, "\r\n");
            strcat(response_str, "Content-Type:");
            strcat(response_str, response.content_type);
            strcat(response_str, "\r\n");
            strcat(response_str, "Content-Length:");
            strcat(response_str, response.content_length);
            strcat(response_str, "\r\n");
            if (strcmp(response.status_code, "500") == 0 || strcmp(response.status_code, "404") == 0 ) {
                printf("server error \n");
                strcat(response_str, "\r\n");
                write(connfd, response_str, strlen(response_str));
                write(connfd, response.content, response.length);
                keepAlive = 0;
            } else {
                if (keepAlive == 1) {
                    strcat(response_str, "Connection: keep-alive");
               } else {
                    strcat(response_str, "Connection: Close");
               }
                if (strcmp(request.request_method, "POST") == 0 || strcmp(request.request_method, "GET") == 0) {
                    strcat(response_str, "\r\n\r\n");
                    write(connfd, response_str, strlen(response_str));
                    write(connfd, response.content, response.length);
                    // memcpy(response_str + strlen(response_str), response.content, response.length);
                } else {            
                    write(connfd, response_str, strlen(response_str));
                }
            }
            memset(response_str, 0, strlen(response_str));
            memset(buf, 0, MAXLINE);
            is_first = 0;
            if (keepAlive == 1) {
                printf("waiting to read \n");
                // bzero(buf, strlen(buf));
                n = read(connfd, buf, MAXLINE);
                printf("reading request \n");
                // printf("%s \n", buf);
                if (n < 0) {
                    printf("empty message \n");
                }
            }
        }
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
    struct timeval tv;
    tv.tv_sec = 10;
	tv.tv_usec = 0;
  
    /* Create a socket descriptor */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;
    /* Eliminates "Address already in use" error from bind. */
    if (setsockopt(listenfd, SOL_SOCKET, SO_RCVTIMEO,
            (const char*)&tv, sizeof tv) < 0)
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