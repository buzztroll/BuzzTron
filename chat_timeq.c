#include <chat_timeq.h>
#include <chat_io.h>
#include <stdlib.h>

typedef struct chat_l_timeq_entry_s
{
    struct timeval                      tm;
    void *                              value;
    struct chat_l_timeq_entry_s *       next;
} chat_l_timeq_entry_t;

typedef struct chat_l_timeq_s 
{
    chat_l_timeq_entry_t *              head;
    int                                 size;
} chat_l_timeq_t;


int
chat_l_tm_cmp(
    const struct timeval *              t1,
    const struct timeval *              t2)
{
    if(t1->tv_sec > t2->tv_sec)
    {
        return 1;
    }
    else if(t1->tv_sec < t2->tv_sec)
    {
        return -1;
    }
    else
    {
        if(t1->tv_usec > t2->tv_usec)
        {
            return 1;
        }
        else if(t1->tv_usec < t2->tv_usec)
        {
            return -1;
        }
        else
        {
            return 0;
        }
    }
}


int
chat_timeq_init(
    chat_timeq_t *                      out_timeq)
{
    chat_l_timeq_t *                    l_tq;

    l_tq = (chat_l_timeq_t *) calloc(1, sizeof(chat_l_timeq_t));
    *out_timeq = l_tq;

    return 0;
}

int
chat_timeq_destroy(
    chat_timeq_t                        timeq)
{
    free(timeq);
    return 0;
}

int
chat_timeq_enqueue(
    chat_timeq_t                        timeq,
    struct timeval *                    tm,
    void *                              value)
{
    int                                 done;
    chat_l_timeq_entry_t *              i;
    chat_l_timeq_entry_t *              j;
    chat_l_timeq_entry_t *              new;
    struct timeval                      now;

    gettimeofday(&now, NULL);

    i = NULL;
    j = timeq->head;

    new = (chat_l_timeq_entry_t *) calloc(1, sizeof(chat_l_timeq_entry_t));
    new->tm.tv_sec = tm->tv_sec + now.tv_sec;
    new->tm.tv_usec = tm->tv_usec + now.tv_usec;
    new->value = value;

    done = FALSE;
    while(j != NULL && !done)
    {
        if(chat_l_tm_cmp(&new->tm, &j->tm) < 0)
        {
            done = TRUE;
        }
        else
        {
            i = j;
            j = j->next;
        }
    }

    new->next = j;
    if(i == NULL)
    {
        timeq->head = new;
    }
    else
    {
        i->next = new;
    }

    return 0;
}

void *
chat_timeq_dequeue(
    chat_timeq_t                        timeq)
{
    chat_l_timeq_entry_t *              head;
    struct timeval                      now;
    void *                              val;

    gettimeofday(&now, NULL);

    head = timeq->head;

    if(head == NULL)
    {
        return NULL;
    }

    if(chat_l_tm_cmp(&now, &head->tm) >= 0)
    {
        timeq->size--;
        timeq->head = timeq->head->next;
        val = head->value;
        free(head);
        return val;
    }

    return NULL;
}

void *
chat_timeq_peek(
    chat_timeq_t                        timeq)
{
    chat_l_timeq_entry_t *              head;
    struct timeval                      now;

    gettimeofday(&now, NULL);

    head = timeq->head;

    if(head == NULL)
    {
        return NULL;
    }

    if(chat_l_tm_cmp(&now, &head->tm) >= 0)
    {
        return head->value;
    }

    return NULL;
}

int
chat_timeq_remove(
    chat_timeq_t                        timeq,
    void *                              value)
{
    chat_l_timeq_entry_t *              i;
    chat_l_timeq_entry_t *              j;

    i = NULL;
    j = timeq->head;

    while(j != NULL)
    {
        if(j->value == value)
        {
            if(i == NULL)
            {
                timeq->head = j->next;
            }
            else
            {
                i->next = j->next;
            }
            free(j);
            return 0;
        }
        i = j;
        j = j->next;
    }

    return -1;
}

int
chat_timeq_next(
    chat_timeq_t                        timeq,
    struct timeval *                    out_tm)
{
    chat_l_timeq_entry_t *              head;
    struct timeval                      now;

    head = timeq->head;
    if(head == NULL)
    {
        out_tm->tv_sec = -1;
        out_tm->tv_usec = -1;
    }
    else
    {
        gettimeofday(&now, NULL);
        out_tm->tv_sec = head->tm.tv_sec - now.tv_sec;
        out_tm->tv_usec = head->tm.tv_usec - now.tv_usec;
        if(out_tm->tv_sec < 0) out_tm->tv_sec = 0;
        if(out_tm->tv_usec < 0) out_tm->tv_usec = 0;
    }
    
    return 0;
}

