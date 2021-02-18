#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "http.h"

#define BUF_SIZE 1024



/* makes a connection to a given host with a port*/
int create_connection(char *host, int port) {
    struct hostent *server;
    struct sockaddr_in serv_addr;
    int sockfd;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) assert(0 && "ERROR opening socket");

    /* lookup the ip address */
    server = gethostbyname(host);
    if (server == NULL){
        assert(0 && "ERROR, no such host");
    } 

    /* fill in the structure */
    memset(&serv_addr,0,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    memcpy(&serv_addr.sin_addr.s_addr,server->h_addr,server->h_length);

    /* connect the socket */
    if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0)
        assert(0 && "ERROR connecting");
    //return socket
    return sockfd;
}

//
// Writes out to Http connection
//
int http_write(int sockfd, char *message){
    int total = strlen(message);
    int sent = 0;
    int bytes = 0;
    //loop through and sends message out to connection
    do {
        bytes = write(sockfd,message+sent,total-sent);
        if (bytes < 0)
            assert(0 && "ERROR writing message to socket");
        if (bytes == 0)
            break;
        sent+=bytes;
    } while (sent < total);

    return 0;
}
//
//Reads from to Http connection to buffer
//
int http_read(int sockfd, Buffer* output){
    int received = 0;
    int bytes = 0;

    //working buffers
    char data[BUF_SIZE];
    char *response = (char*)malloc(sizeof(char)*BUF_SIZE);
    //loop through and reads message to response buffer
    do {
        bytes = read(sockfd, data, BUF_SIZE);
        if (bytes < 0)
            assert(0 && "ERROR reading response from socket");
        if (bytes == 0)
            break;
        received+=bytes;
        char *new_data = (char*)realloc(response, received);

        int temp = received - bytes;
        memcpy(new_data+temp, data, bytes);

        response = new_data;


    } while (1);
    //append response buffer to output buffer
    output->data = response;
    output->length = received;

    return 0;
}

/**
 * Perform an HTTP 1.0 query to a given host and page and port number.
 * host is a hostname and page is a path on the remote server. The query
 * will attempt to retrievev content in the given byte range.
 * User is responsible for freeing the memory.
 * 
 * @param host - The host name e.g. www.canterbury.ac.nz
 * @param page - e.g. /index.html
 * @param range - Byte range e.g. 0-500. NOTE: A server may not respect this
 * @param port - e.g. 80
 * @return Buffer - Pointer to a buffer holding response data from query
 *                  NULL is returned on failure.
 */
Buffer* http_query(char *host, char *page, const char *range, int port) {
    char *message_fmt = "GET /%s HTTP/1.0\r\nHost: %s\r\nRange: bytes=%s\r\nUser-Agent: getter\r\n\r\n";
    int sockfd;//, bytes, received;
    char message[1024];
    Buffer* output = (Buffer*)malloc(sizeof(Buffer));
    sprintf(message,message_fmt,page,host,range);

    /* create the socket */
    sockfd = create_connection(host, port);

    /* send the request */
    http_write(sockfd, message);

    /*reas responce*/
    http_read(sockfd,output);

    /* close the socket */
    close(sockfd);

    return output;
}

/**
 * Separate the content from the header of an http request.
 * NOTE: returned string is an offset into the response, so
 * should not be freed by the user. Do not copy the data.
 * @param response - Buffer containing the HTTP response to separate 
 *                   content from
 * @return string response or NULL on failure (buffer is not HTTP response)
 */
char* http_get_content(Buffer *response) {

    char* header_end = strstr(response->data, "\r\n\r\n");

    if (header_end) {
        return header_end + 4;
    }
    else {
        return response->data;
    }
}


/**
 * Splits an HTTP url into host, page. On success, calls http_query
 * to execute the query against the url. 
 * @param url - Webpage url e.g. learn.canterbury.ac.nz/profile
 * @param range - The desired byte range of data to retrieve from the page
 * @return Buffer pointer holding raw string data or NULL on failure
 */
Buffer *http_url(const char *url, const char *range) {
    
    char host[BUF_SIZE];
    strncpy(host, url, BUF_SIZE);
    char *page = strstr(host, "/");
    if (page) {
        page[0] = '\0';
        ++page;
        return http_query(host, page, range, 80);
    }
    else {
        fprintf(stderr, "could not split url into host/page %s\n", url);
        return NULL;
    }
    
}


/**
 * Makes a HEAD request to a given URL and gets the content length
 * Then determines max_chunk_size and number of split downloads needed
 * @param url   The URL of the resource to download
 * @param threads   The number of threads to be used for the download
 * @return int  The number of downloads needed satisfying max_chunk_size
 *              to download the resource
 */
int get_num_tasks(char *url, int threads) {
    // Splits URL
    char host[BUF_SIZE];
    strncpy(host, url, BUF_SIZE);
    char *page = strstr(host, "/");
    if (page) {
        page[0] = '\0';
        page++;
    }
    else {
        fprintf(stderr, "could not split url into host/page %s\n", url);
    }

    char *message_fmt = "HEAD /%s HTTP/1.0\r\nHost: %s\r\n\r\n";

    int sockfd;
    char message[1024];

    Buffer* output = (Buffer*)malloc(sizeof(Buffer));
    sprintf(message,message_fmt,page,host);

    /* create the socket */
    sockfd = create_connection(host, 80);

    /* send the request */
    http_write(sockfd, message);

    /*read responce*/
    http_read(sockfd,output);

    /* close the socket */
    close(sockfd);

    char *Start = strstr(output->data, "Content-Length: ");
    Start += strlen("Content-Length: ");
    
    char *End = strstr(Start, "\r\n");

    char Worker[25];
    strncpy(Worker, Start, ((size_t)End - (size_t)Start));

    int length = atoi(Worker);

    //check if URl wont accept bytes offset
    if (strstr(output->data, "Accept-Ranges: bytes") == NULL) {
        max_chunk_size = length;
        free(output->data);
        free(output);
        return 1;
    }

    max_chunk_size = (length / threads) + 1;
    free(output->data);
    free(output);
    return threads;
}


int get_max_chunk_size() {

    return max_chunk_size;
}
