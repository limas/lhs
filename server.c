#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netdb.h>

#include "global.h"
#include "server.h"

static char *strtok_l(char *str1, char *str2)
{
    static char *str;
    char *pos;
    char *ret = NULL;

    if(NULL != str1)
        str = str1;

    if(NULL == str)
        return NULL;

    pos = strstr(str, str2);
    ret = str;
    if(NULL != pos)
    {
        *pos = '\0';
        str = (pos+strlen(str2));
    }
    else
    {
        str = NULL;
    }

    return ret;
}

static int setnonblocking(int fd)
{
    if(-1 == fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0)|O_NONBLOCK))
    {
        return -1;
    }

    return 0;
}

static bool _tcp_setup(server *self)
{
    bool ret = false;
    int status;
    struct addrinfo hints = {};
    struct addrinfo *info = NULL;

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if(0 != (status = getaddrinfo(self->ip, self->port, &hints, &info)))
    {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return false;
    }

    do
    {
        if(0 > (self->serverfd = socket(info->ai_family, info->ai_socktype, info->ai_protocol)))
        {
            fprintf(stderr, "fail to create socket.\n");
            break;
        }

        int opt = 1;
        if(0 > (status = setsockopt(self->serverfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int))))
        {
            fprintf(stderr, "fail to set socket option [%d].\n", status);
            break;
        }

        if(0 > (status = bind(self->serverfd, info->ai_addr, info->ai_addrlen)))
        {
            fprintf(stderr, "fail to bind [%d].\n", status);
            break;
        }

        ret = true;
    }while(0);

    freeaddrinfo(info);

    if(false == ret)
        close(self->serverfd);

    return ret;
}

static bool _dummy_prehandle()
{
    return true;
}

static bool _request_parsing(server *server, int clientfd, request *req)
{
    static char *buf = NULL;
    char *req_line;
    char *header;
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
    }

    /* deal with body */
    body = strtok_l(NULL, "\r\n");

    req->method   = strtok_l(req_line, " ");
    req->resource = strtok_l(NULL, " ");
    req->protocol = strtok_l(NULL, " ");

    return true;
}

server *server_create(struct server_info *info)
{
    server *serv_inst;

    do
    {
        serv_inst = (server *)calloc(1, sizeof(server));
        if(!serv_inst)
        {
            fprintf(stderr, "fail to allocate memory for server instance.\n");
            break;
        }

        serv_inst->ip = info->ip;
        serv_inst->port = info->port;

        if(false == _tcp_setup(serv_inst))
        {
            fprintf(stderr, "fail to setup TCP binding.\n");
            break;
        }

        serv_inst->epollfd = epoll_create1(0);
        if (-1 == serv_inst->epollfd)
        {
            fprintf(stderr, "fail to create poll fd.\n");
            break;
        }

        serv_inst->events = (struct epoll_event *)malloc(MAX_POLL_FD*sizeof(struct epoll_event));
        if(!serv_inst->events)
        {
            fprintf(stderr, "fail to allocate event array.\n");
            break;
        }

        fprintf(stdout, "server created.\n");
        return serv_inst;
    }while(0);

    if(0 != serv_inst->serverfd)
        close(serv_inst->serverfd);

    free(serv_inst);

    return NULL;
}

bool server_destroy(server *server)
{
    free(server);

    return true;
}

bool server_addprehandle(server *server, pre_handler handler)
{
    server->pre_handler = handler;
    return true;
}

bool server_addhandle(server *server, char *path, res_handler handler)
{
    return true;
}

bool server_start(server *server)
{
    res_handler handler=NULL;
    struct epoll_event event;

    if(-1 == listen(server->serverfd, 10))
    {
        fprintf(stderr, "error on listen [%d].\n", errno);
        return false;
    }

    if(-1 == setnonblocking(server->serverfd))
    {
        fprintf(stderr, "fail to set socket as non-blocking [%d].\n", errno);
        return false;
    }

    event.events = EPOLLIN;
    event.data.fd = server->serverfd;
    if(-1 == epoll_ctl(server->epollfd, EPOLL_CTL_ADD, server->serverfd, &event))
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

        nfds = epoll_wait(server->epollfd, server->events, MAX_POLL_FD, -1);
        if(-1 == nfds)
        {
            fprintf(stderr, "fail to wait for event [%d].\n", errno);
            return false;
        }

        for(idx=0; idx<nfds; idx++)
        {
            clientfd = server->events[idx].data.fd;

            if(clientfd == server->serverfd)
            {
                clientfd = accept(server->serverfd, (struct sockaddr *)&client, &size);
                if(0 > clientfd)
                {
                    fprintf(stderr, "error on accept [%d].", errno);
                    continue;
                }

                setnonblocking(clientfd);
                event.events = EPOLLIN | EPOLLET;
                event.data.fd = clientfd;
                if(-1 == epoll_ctl(server->epollfd, EPOLL_CTL_ADD, clientfd, &event))
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

                /* analysis request */
                if(false == _request_parsing(server, clientfd, &req))
                {
                    goto disconnect;
                }

                /* pre-handler, in charge such as session validation, unauthorized access rejection, etc... */
                if(false == server->pre_handler(server, clientfd, &req))
                {
                    goto disconnect;
                }

                /* find corresponding handler */
                if(handler)
                {
                    handler(server, clientfd, NULL);
                }
                else
                {
                    char msg[]={"HTTP/1.1 404 Not Found\r\n"};
                    write(clientfd, msg, sizeof(msg));
                }

                /* check persistent connection request, if not, close the connection */
                if(0 /* placeholder for persistent connection checking */)
                {
                    continue;
                }

disconnect:
                if(-1 == epoll_ctl(server->epollfd, EPOLL_CTL_DEL, clientfd, NULL))
                {
                    fprintf(stderr, "fail to remove epoll event with client socket.\n");
                }

                fprintf(stdout, "client [%d] disconnected.\n", clientfd);
                close(clientfd);
            }
        }
    }
}

