#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>

#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netdb.h>

#include "global.h"
#include "list.h"
#include "server.h"
#include "misc.h"

typedef struct
{
    sqlite3 *db;

    char *ip;
    char *port;
    int serverfd;
    int epollfd;
    struct epoll_event *events;

    pre_handler pre_handler;
    list handler;
}serv_inst;

static int setnonblocking(int fd)
{
    if(-1 == fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0)|O_NONBLOCK))
    {
        return -1;
    }

    return 0;
}

static bool _tcp_setup(serv_inst *serv)
{
    bool ret = false;
    int status;
    struct addrinfo hints = {};
    struct addrinfo *info = NULL;

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if(0 != (status = getaddrinfo(serv->ip, serv->port, &hints, &info)))
    {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return false;
    }

    do
    {
        if(0 > (serv->serverfd = socket(info->ai_family, info->ai_socktype, info->ai_protocol)))
        {
            fprintf(stderr, "fail to create socket.\n");
            break;
        }

        int opt = 1;
        if(0 > (status = setsockopt(serv->serverfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int))))
        {
            fprintf(stderr, "fail to set socket option [%d].\n", status);
            break;
        }

        if(0 > (status = bind(serv->serverfd, info->ai_addr, info->ai_addrlen)))
        {
            fprintf(stderr, "fail to bind [%d].\n", status);
            break;
        }

        ret = true;
    }while(0);

    freeaddrinfo(info);

    if(false == ret)
    {
        close(serv->serverfd);
        serv->serverfd = -1;
    }

    return ret;
}

static bool _dummy_prehandle()
{
    return true;
}

typedef enum
{
    HTTP_STATUS_100_CONT = 0,
    HTTP_STATUS_101_SWITCH_PROTO,
    HTTP_STATUS_102_PROC,
    HTTP_STATUS_103_EARLY_HINTS,

    HTTP_STATUS_200_OK,

    HTTP_STATUS_400_BAD_REQ,
    HTTP_STATUS_401_UNAURTH,
    HTTP_STATUS_402_PAY_REQ,
    HTTP_STATUS_403_FORBID,
    HTTP_STATUS_404_NOT_FOUND,
}HTTP_STATUS_CODE;

char _http_err_msg_404[]={
    "HTTP/1.1 404 Not Found\r\n"
    "Content-Length: 0\r\n"
    "Server: lhs/0.1 (Unix) (Red-Hat/Linux)\r\n"
    "Connection: close\r\n"
};

typedef struct
{
    const char *msg;
    uint32_t len;
}http_err_msg;

response _http_err_msg[]={
    {NULL, 0},
    {_http_err_msg_404, sizeof(_http_err_msg_404)},
};

static response _get_error_reponse(HTTP_STATUS_CODE code)
{
    switch(code)
    {
        case HTTP_STATUS_404_NOT_FOUND: return _http_err_msg[1];
        default:return _http_err_msg[0];
    }
}

static bool _request_parsing(serv_inst *server, int clientfd, request *req)
{
    static char *buf = NULL;
    char *req_line;
    char *header;
    char *value;
    char *body;
    ssize_t len;

    if(!buf)
    {
        buf = (char *)malloc(REQ_BUF_SIZE);
        if(!buf)
        {
            fprintf(stderr, "fail to allocate memory.\n");
            exit(-1);
        }
    }

    req->header = list_new(NULL);
    if(!req->header)
    {
        fprintf(stderr, "fail to new header list.\n");
        exit(-1);
    }

    len = recv(clientfd, buf, REQ_BUF_SIZE, 0);
    if(len < 0)
    {
        fprintf(stderr, "error return from recv [%d].\n", errno);
        return false;
    }

    if(len == 0)
    {
        fprintf(stdout, "client [%d] shotdown connection.\n", clientfd);
        return false;
    }

    if(REQ_BUF_SIZE > len)
    {
        buf[len] = '\0';
    }
    else
    {
        /* request message larger than buffer size, need some other special handle */
        return false;
    }

    /* get request line */
    req_line = strtok_l(buf, "\r\n");
    if(NULL == req_line)
    {
        return false;
    }

    /* deal with header */
    while(header = strtok_l(NULL, "\r\n"))
    {
        if('\0' == *header)
            break;

        value = strchr(header, ':');
        *value = '\0';
        value++;
        list_add(req->header, header, trim(value));
    }

    /* deal with body */
    body = strtok_l(NULL, "\r\n");

    req->method   = strtok_l(req_line, " ");
    req->resource = strtok_l(NULL, " ");
    req->protocol = strtok_l(NULL, " ");

    return true;
}

bool server_destroy(server server)
{
    serv_inst *serv = (serv_inst *)server;

    if(0 < serv->serverfd)
        close(serv->serverfd);

    if(NULL != serv->handler)
        list_del(serv->handler);

    if(0 < serv->epollfd)
        close(serv->epollfd);

    if(NULL != serv->events)
        free(serv->events);

    free(serv);

    return true;
}

server server_create(struct server_info *info, sqlite3 *db)
{
    serv_inst *serv;

    do
    {
        serv = (serv_inst *)calloc(1, sizeof(serv_inst));
        if(!serv)
        {
            fprintf(stderr, "fail to allocate memory for server instance.\n");
            break;
        }

        serv->ip = info->ip;
        serv->port = info->port;

        if(false == _tcp_setup(serv))
        {
            fprintf(stderr, "fail to setup TCP binding.\n");
            break;
        }

        serv->epollfd = epoll_create1(0);
        if (-1 == serv->epollfd)
        {
            fprintf(stderr, "fail to create poll fd.\n");
            break;
        }

        serv->events = (struct epoll_event *)malloc(MAX_POLL_FD*sizeof(struct epoll_event));
        if(!serv->events)
        {
            fprintf(stderr, "fail to allocate event array.\n");
            break;
        }

        serv->handler = list_new(NULL);
        if(INVALID_LIST == serv->handler)
        {
            fprintf(stderr, "fail to create list for handler.\n");
            break;
        }

        fprintf(stdout, "server created.\n");
        return (server)serv;
    }while(0);

    server_destroy(serv);

    return NULL;
}

bool server_addprehandle(server server, pre_handler handler)
{
    ((serv_inst *)server)->pre_handler = handler;
    return true;
}

bool server_addhandle(server server, char *path, res_handler handler)
{
    return list_add(((serv_inst *)server)->handler, path, (void *)handler);
}

bool server_start(server server)
{
    serv_inst *serv=(serv_inst *)server;
    res_handler handler=NULL;
    struct epoll_event event;

    if(-1 == listen(serv->serverfd, 10))
    {
        fprintf(stderr, "error on listen [%d].\n", errno);
        return false;
    }

    if(-1 == setnonblocking(serv->serverfd))
    {
        fprintf(stderr, "fail to set socket as non-blocking [%d].\n", errno);
        return false;
    }

    event.events = EPOLLIN;
    event.data.fd = serv->serverfd;
    if(-1 == epoll_ctl(serv->epollfd, EPOLL_CTL_ADD, serv->serverfd, &event))
    {
        fprintf(stderr, "fail to add epoll event with listen socket.\n");
        return false;
    }

    fprintf(stdout, "server start running.\n");
    /* here should use select to accomplish concurrent connection handle requirement */
    while(1)
    {
        struct sockaddr_storage client;
        socklen_t size=sizeof(client);
        int clientfd;
        int nfds;
        int idx;

        nfds = epoll_wait(serv->epollfd, serv->events, MAX_POLL_FD, -1);
        if(-1 == nfds)
        {
            fprintf(stderr, "fail to wait for event [%d].\n", errno);
            return false;
        }

        for(idx=0; idx<nfds; idx++)
        {
            clientfd = serv->events[idx].data.fd;

            if(clientfd == serv->serverfd)
            {
                clientfd = accept(serv->serverfd, (struct sockaddr *)&client, &size);
                if(0 > clientfd)
                {
                    fprintf(stderr, "error on accept [%d].", errno);
                    continue;
                }

                setnonblocking(clientfd);
                event.events = EPOLLIN;
                event.data.fd = clientfd;
                if(-1 == epoll_ctl(serv->epollfd, EPOLL_CTL_ADD, clientfd, &event))
                {
                    fprintf(stderr, "error on add client socket to epoll [%d].\n", errno);
                    close(clientfd);
                    continue;
                }

                fprintf(stdout, "client [%d] connected.\n", clientfd);
            }
            else
            {
                request req;
                response resp;

                /* analysis request */
                if(false == _request_parsing(serv, clientfd, &req))
                {
                    goto disconnect;
                }

                /* pre-handler, in charge such as session validation, unauthorized access rejection, etc... */
                if(false == serv->pre_handler(serv, clientfd, &req))
                {
                    goto disconnect;
                }

                /* find corresponding handler */
                handler = (res_handler)list_find(serv->handler, req.resource);
                if(handler)
                {
                    resp = handler(serv, clientfd, &req);
                }
                else
                {
                    resp = _get_error_reponse(HTTP_STATUS_404_NOT_FOUND);
                    write(clientfd, resp.msg, resp.msglen);

                    goto disconnect;
                }

                /* check persistent connection request, if not, close the connection */
                if(0 == stricmp("keep-alive", list_find(req.header, "connection")))
                {
                    goto done;
                }

disconnect:
                if(-1 == epoll_ctl(serv->epollfd, EPOLL_CTL_DEL, clientfd, NULL))
                {
                    fprintf(stderr, "fail to remove epoll event with client socket [%d].\n", clientfd);
                }

                fprintf(stdout, "client [%d] disconnected.\n", clientfd);
                close(clientfd);
done:
                list_del(req.header);
            }
        }
    }
}

