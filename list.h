
#define INVALID_LIST (NULL)

typedef void * list;
typedef struct
{
    uint32_t dim;
    bool key_case_sensitive;
}list_cfg;

list list_new(list_cfg *cfg);
bool list_add(list list, char *key, void *obj);
bool list_remove(list list, char *key);
void *list_find(list list, char *key);
bool list_del(list list);

