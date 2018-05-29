#include <sqlite3.h>

typedef void * server;
typedef struct _request request;
typedef struct _response response;

typedef bool (*pre_handler)(
        server server,
        int clientfd,
        const request *req);

typedef response (*res_handler)(
        server server,
        int fd,
        const request *req);

struct _request
{
    char *method;
    char *resource;
    char *protocol;
    list header;
    char *body;
};

struct _response
{
    char *msg;
    uint32_t msglen;
};

struct server_info
{
    char *ip;
    char *port;
};

server server_create(struct server_info *, sqlite3 *);
bool server_destroy(server);
bool server_addprehandle(server, pre_handler);
bool server_addhandle(server, char *, res_handler);
bool server_start(server);

