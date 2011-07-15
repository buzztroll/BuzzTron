#if !defined(CHAT_TIMEQ_H)
#define CHAT_TIMEQ_H 1

#include <sys/time.h>

typedef struct chat_l_timeq_s *         chat_timeq_t;

int
chat_timeq_init(
    chat_timeq_t *                      out_timeq);

int
chat_timeq_destroy(
    chat_timeq_t                        timeq);

int
chat_timeq_enqueue(
    chat_timeq_t                        timeq,
    struct timeval *                    tm,
    void *                              value);

void *
chat_timeq_dequeue(
    chat_timeq_t                        timeq);

void *
chat_timeq_peek(
    chat_timeq_t                        timeq);

int
chat_timeq_remove(
    chat_timeq_t                        timeq,
    void *                              value);

int
chat_timeq_next(
    chat_timeq_t                        timeq,
    struct timeval *                    out_tm);

#endif
