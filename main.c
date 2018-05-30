#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>

#include <sqlite3.h>

#include "global.h"
#include "list.h"
#include "server.h"

#define DB_NAME "test.sql"

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

static sqlite3 *db_init(void)
{
    int ret;
    bool new_db = true;
    sqlite3 *db = NULL;
    char *sql;
    char *errmsg = NULL;

    if(0 == access(DB_NAME, R_OK|W_OK))
    {
        new_db = false;
    }

    ret = sqlite3_open(DB_NAME, &db);

    if(ret)
    {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        return NULL;
    }

    if(new_db)
    {
        sql = "CREATE TABLE SESSION(" \
              "ID CHAR(40)  PRIMARY KEY  NOT NULL," \
              "USER_ID INT               NOT NULL," \
              "FOREIGN KEY(USER_ID) REFERENCES USER(ID));" \
              "" \
              "CREATE TABLE USER(" \
              "ID      INT  PRIMARY KEY  NOT NULL," \
              "F_NAME TEXT  NOT NULL," \
              "L_NAME TEXT  NOT NULL);" \
              "" \
              "";

        ret = sqlite3_exec(db, sql, NULL, 0, &errmsg);
        if(SQLITE_OK != ret)
        {
            fprintf(stderr, "fail to create table, SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }
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

