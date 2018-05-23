#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>

#include "global.h"
#include "list.h"
#include "misc.h"

#define LENTITY(x) ((lentity *)x)

typedef int (*cmpfunc)(const char *str1, const char *str2);

typedef struct _list_entry list_entry;
struct _list_entry
{
    char *key;
    void *obj;
    list_entry *next;
};

typedef struct
{
    list_entry *table;
    list_entry **tail;
    cmpfunc cmp;
}lentity;

static list_cfg _def_cfg =
{
    .dim=1024,
    .key_case_sensitive=false,
};

list list_new(list_cfg *cfg)
{
    lentity *inst;

    if(!cfg)
        cfg = &_def_cfg;

    inst = (lentity *)malloc(sizeof(lentity));
    if(inst)
    {
        inst->table = NULL;
        inst->tail = &(inst->table);
        inst->cmp = (cmpfunc)((cfg->key_case_sensitive)?(strcmp):(stricmp));
    }

    return (list)inst;
}

bool list_add(list list, char *key, void *obj)
{
    list_entry *entry;

    entry = (list_entry *)malloc(sizeof(list_entry));
    if(!entry)
        return false;

    entry->key = strdup(key);
    entry->obj = obj;
    entry->next = NULL;

    *(LENTITY(list)->tail) = entry;
    LENTITY(list)->tail = &(entry->next);

    return true;
}

bool list_remove(list list, char *key)
{
    list_entry **entry = &(LENTITY(list)->table);
    list_entry **prev = entry;
    list_entry *next;

    while(*entry)
    {
        if(0 == LENTITY(list)->cmp((*entry)->key, key))
        {
            next = (*entry)->next;

            free((*entry)->key);
            free(*entry);
            *entry = next;

            if(NULL == *entry)
            {
                LENTITY(list)->tail = prev;
            }

            return true;
        }

        prev = entry;
        entry = &((*entry)->next);
    }

    return false;
}

void *list_find(list list, char *key)
{
    list_entry *entry = LENTITY(list)->table;

    while(entry)
    {
        if(0 == LENTITY(list)->cmp(entry->key, key))
            return entry->obj;

        entry = entry->next;
    }

    return NULL;
}

bool list_del(list list)
{
    list_entry *entry;
    list_entry *next;

    if(!list)
        return true;

    entry = LENTITY(list)->table;

    while(entry)
    {
        next = entry->next;

        free(entry->key);
        free(entry);

        entry = next;
    }

    free(list);

    return true;
}

