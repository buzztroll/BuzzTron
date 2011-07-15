#if !defined(CHAT_HASHTABLE_H)
#define CHAT_HASHTABLE_H 1

typedef struct chat_hastable_s *        chat_hastable_t;

int
chat_hashtable_init(
    chat_hastable_t *                   out_table);

int
chat_hashtable_destroy(
    chat_hastable_t                     out_table);

int
chat_hashtable_add(
    chat_hastable_t                     out_table,
    char *                              key,
    void *                              value);

void *
chat_hashtable_lookup(
    chat_hastable_t                     out_table,
    char *                              key);

void *
chat_hashtable_delete(
    chat_hastable_t                     out_table,
    char *                              key);

void *
chat_hashtable_delete_any(
    chat_hastable_t                     table);

#endif
