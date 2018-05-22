#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>

#include "global.h"
#include "list.h"
#include "server.h"

/* function prototype */
static response *server_home(server *server, int fd, const request *req);
static response *server_login(server *server, int fd, const request *req);

struct route_map
{
    char *path;
    res_handler handler;
};

static const struct route_map route_map[]=
{
    {"/", server_home},
    {"/login", server_login},
};

static response *server_home(server *server, int fd, const request *req)
{
    return NULL;
}

static response *server_login(server *server, int fd, const request *req)
{
    return NULL;
}

static bool _pre_handle(server *server, int clientfd, const request *req)
{
    /* varify session */

    return true;
}

int main(int argc, char **argv)
{
    server *serv;
    struct server_info info;

    fprintf(stdout, "start initialing server...\n");

    //info.ip = "localhost";
    info.ip = NULL;
    info.port = "80";

    if(NULL == (serv = server_create(&info)))
    {
        fprintf(stderr, "fail to create server instance.\n");
        exit(-1);
    }

    server_addprehandle(serv, _pre_handle);

    for(int i=0; i<ARR_SIZE(route_map); i++)
    {
        server_addhandle(
                serv,
                route_map[i].path,
                route_map[i].handler);
    }

    if(false == server_start(serv))
    {
        fprintf(stderr, "fail to start server.\n");
        exit(-1);
    }

    return 0;
}

