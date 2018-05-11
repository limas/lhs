
typedef struct
{
    uint32_t dim;
}hash;

typedef struct
{
    uint32_t dim;
}hash_cfg;

hash *hash_create(hash_cfg *cfg);
bool hash_add(hash *hash, char *key, void *obj);
bool hash_del(hash *hash, char *key);
void *hash_find(hash *hash, char *key);
bool hash_destroy(hash *hash);

