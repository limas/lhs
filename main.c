#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <sqlite3.h>

#include "global.h"
#include "list.h"
#include "server.h"

#define DB_NAME "db.sql"
#define DB_DEF_TABLE "db_def_table.txt"

/* function prototype */
static response server_home(server server, int fd, const request *req);
static response server_login(server server, int fd, const request *req);

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

static server serv;

static response server_home(server server, int fd, const request *req)
{
    return (response){0};
}

static response server_login(server server, int fd, const request *req)
{
    return (response){0};
}

static bool _pre_handle(server server, int clientfd, const request *req)
{
    /* varify session */

    return true;
}

static void create_default_table(sqlite3 *db)
{
    int ret;
    char *sql = NULL;;
    char *errmsg = NULL;
    int fd = -1;
    struct stat info;

    fprintf(stdout, "construct default tables for new database.\n");

    do
    {
        fd = open(DB_DEF_TABLE, O_RDONLY);
        if(-1 == fd)
        {
            break;
        }

        ret = fstat(fd, &info);
        if(-1 == ret)
        {
            fprintf(stdout, "warning, fail to stat table constructor [%d].\n", errno);
            break;
        }

        sql = (char *)malloc(info.st_size+1);
        if(!sql)
        {
            fprintf(stdout, "warning, fail to allocate memory for default table construction.\n");
            break;
        }

        read(fd, (void *)sql, info.st_size);
        sql[info.st_size]='\0';
        ret = sqlite3_exec(db, sql, NULL, 0, &errmsg);

        if(SQLITE_OK != ret)
        {
            fprintf(stdout, "warning, fail to create table, SQL error: %s\n", errmsg);
            sqlite3_free(errmsg);
            sqlite3_close(db);
        }
    }while(0);

    if(-1 != fd)
        close(fd);

    free(sql);
}

static sqlite3 *db_init(void)
{
    int ret;
    bool new_db = true;
    char *errmsg = NULL;
    sqlite3 *db = NULL;

    if(0 == access(DB_NAME, R_OK|W_OK))
    {
        new_db = false;
    }

    ret = sqlite3_open(DB_NAME, &db);

    if(ret)
    {
        fprintf(stderr, "fail to open database: %s\n", sqlite3_errmsg(db));
        return NULL;
    }

    if(new_db)
    {
        create_default_table(db);
    }

    return db;
}

int main(int argc, char **argv)
{
    sqlite3 *db;
    struct server_info info;

    db = db_init();

    fprintf(stdout, "start initialing server...\n");

    //info.ip = "localhost";
    info.ip = NULL;
    info.port = "80";

    if(NULL == (serv = server_create(&info, db)))
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

    server_destroy(serv);
    sqlite3_close(db);

    return 0;
}

