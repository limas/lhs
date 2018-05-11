
typedef struct _server server;
typedef struct _request request;
typedef struct _response response;

typedef bool (*pre_handler)(server *server, int clientfd, const request *req);
typedef response *(*res_handler)(server *self, int fd, const request *req);

struct _request
{
    char *method;
    char *resource;
    char *protocol;
    char *header;
    char *body;
};

struct _response
{
    char *msg;
};

struct _server
{
    char *ip;
    char *port;
    int serverfd;
    int epollfd;
    struct epoll_event *events;

    pre_handler pre_handler;
};

struct server_info
{
    char *ip;
    char *port;
};

server *server_create(struct server_info *info);
bool server_destroy(server *server);
bool server_addprehandle(server *server, pre_handler handler);
bool server_addhandle(server *server, char *path, res_handler handler);
bool server_start(server *server);

