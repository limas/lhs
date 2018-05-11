#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>

#include "global.h"
#include "hash.h"

hash *hash_create(hash_cfg *cfg)
{
    return NULL;
}

bool hash_add(hash *hash, char *key, void *obj)
{
    return true;
}

bool hash_del(hash *hash, char *key)
{
    return true;
}

void *hash_find(hash *hash, char *key)
{
    return NULL;
}

bool hash_destroy(hash *hash)
{
    return true;
}

