#include "chat.h"

#define HASHTABLE_SIZE                  256

typedef struct chat_hastable_entry_s
{
    char *                              key;
    void *                              value;
    struct chat_hastable_entry_s *      next;
} chat_l_hastable_entry_t;

typedef struct chat_hastable_s
{
    chat_l_hastable_entry_t *           entries[HASHTABLE_SIZE];
} chat_l_hastable_t;

/*
 *  lame string hash function
 */
int
chat_l_hashtable_hashfunc(
    char *                              key)
{
    unsigned long                       h = 0;

    while(*key)
    {
        h += *key;
        key++;
    }

    return h % HASHTABLE_SIZE;
}

int
chat_hashtable_init(
    chat_hastable_t *                   out_table)
{
    chat_l_hastable_t *                 table;

    table = (chat_l_hastable_t *) calloc(1, sizeof(chat_l_hastable_t));
    if(table == NULL)
    {
        return 1;
    }

    *out_table = table;

    return 0; 
}

int
chat_hashtable_destroy(
    chat_hastable_t                     table)
{
    chat_l_hastable_entry_t *           ent;
    int                                 i;

    for(i = 0; i < HASHTABLE_SIZE; i++)
    {
        ent = table->entries[i];

        while(ent != NULL)
        {
            free(ent->key);
            free(ent);
            ent = ent->next;
        }
    }

    return 0;
}

int
chat_hashtable_add(
    chat_hastable_t                     table,
    char *                              key,
    void *                              value)
{
    int                                 ndx;
    chat_l_hastable_entry_t *           new;
    chat_l_hastable_entry_t *           i;
    chat_l_hastable_entry_t *           j;

/******** XXX TODO do lookup first and jsut update current value */
    ndx = chat_l_hashtable_hashfunc(key);

    i = NULL;
    j = table->entries[ndx];

    while(j != NULL)
    {
        /* if the key is already in the table return failure */
        if(strcmp(key, j->key) == 0)
        {
            return 1;
        }
        i = j;
        j = j->next;
    }

    new = (chat_l_hastable_entry_t *)
        calloc(1, sizeof(chat_l_hastable_entry_t));
    new->key = strdup(key);
    new->value = value;
    new->next = NULL;

    if(i == NULL)
    {
        table->entries[ndx] = new;
    }
    else
    {
        i->next = new;
    }

    return 0;
}

void *
chat_hashtable_lookup(
    chat_hastable_t                     table,
    char *                              key)
{
    int                                 ndx;
    chat_l_hastable_entry_t *           i;

    ndx = chat_l_hashtable_hashfunc(key);

    i = table->entries[ndx];

    while(i != NULL)
    {
        if(strcmp(i->key, key) == 0)
        {
            return i->value;
        }
        i = i->next;
    }

    return NULL;
}

void *
chat_hashtable_delete(
    chat_hastable_t                     table,
    char *                              key)
{
    void *                              rc;
    int                                 ndx;
    chat_l_hastable_entry_t *           i;
    chat_l_hastable_entry_t *           j;

    ndx = chat_l_hashtable_hashfunc(key);

    i = NULL;
    j = table->entries[ndx];

    while(j != NULL)
    {
        if(strcmp(key, j->key) == 0)
        {
            if(i == NULL)
            {
                table->entries[ndx] = j->next;
            }
            else
            {
                i->next = j->next;
            }
            rc = j->value;
            free(j->key);
            free(j);
            return rc;
        }
        i = j;
        j = j->next;
    }
    return NULL;
}

void *
chat_hashtable_delete_any(
    chat_hastable_t                     table)
{
    void *                              rc;
    int                                 ctr;

    for(ctr = 0; ctr < HASHTABLE_SIZE; ctr++)
    {
        if(table->entries[ctr] != NULL)
        {
            rc = table->entries[ctr]->value;
            table->entries[ctr] = table->entries[ctr]->next;
            return rc;
        }
    }
    return NULL;
}
