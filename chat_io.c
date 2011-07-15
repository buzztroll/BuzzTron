#define CHAT_MAX_FD                     256

#include "chat.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


typedef struct chat_l_io_event_queue_s
{
    int                                 max_fd;
    int                                 high_fd;
    chat_io_event_t *                   write_events[CHAT_MAX_FD];
    chat_io_event_t *                   read_events[CHAT_MAX_FD];
    fd_set                              write_fd_set;
    fd_set                              read_fd_set;
    chat_timeq_t                        time_q;
} chat_l_io_event_queue_t;

static
chat_io_event_t *
chat_l_create_event(
    chat_l_io_event_queue_t *           q,
    int                                 fd,
    chat_io_event_callback_t            cb,
    void *                              user_arg,
    int                                 mtimeout)
{
    chat_io_event_t *                   event;

    event = (chat_io_event_t *) calloc(1, sizeof(chat_io_event_t));
    event->q = q;
    event->fd = fd;
    event->cb = cb;
    event->user_arg = user_arg;
    event->error = CHAT_SUCCESS;
    if(mtimeout >= 0)
    {
        event->timeout.tv_sec = mtimeout / 1000;
        event->timeout.tv_usec = (mtimeout % 1000) * 1000;
    }
    else
    {
        event->timeout.tv_sec = CHAT_MAX_TIMEOUT;
        event->timeout.tv_usec = 0;
    }
 
    chat_timeq_enqueue(q->time_q, &event->timeout, event);

    if(fd > q->high_fd)
    {
        q->high_fd = fd;
    }

    return event;
}

static
void
chat_free_event(
    chat_io_event_t *                   event)
{
    free(event);
}

static
chat_io_event_t *
chat_l_io_read_ready(
    chat_l_io_event_queue_t *           q,
    int                                 fd)
{
    int                                 rc;
    int                                 flags;
    ssize_t                             nbytes;
    chat_io_event_t *                   event;
    chat_io_event_t *                   return_event = NULL;
    struct linger                       linger;
    socklen_t                           addr_len;

    event = (chat_io_event_t *) q->read_events[fd];

    switch(event->type)
    {
        case CHAT_IO_EVENT_TYPE_ACCEPT:
            do
            {
                event->fd = accept(fd, NULL, NULL);
            } while(event->fd < 0 && errno == EINTR);
            if(event->fd < 0)
            {
                event->error = errno;
            }
            else
            {
                flags = fcntl(event->fd, F_GETFL);
                flags = flags | O_NONBLOCK;
                fcntl(event->fd, F_SETFL, flags);

                linger.l_onoff = 1;
                linger.l_linger = 100;
                setsockopt(event->fd, SOL_SOCKET, SO_LINGER, &linger,
                    sizeof(linger));
            }
            return_event = event;
            break;

        case CHAT_IO_EVENT_TYPE_TCP_READ:
            nbytes = read(
                fd,
                &event->buffer[event->offset],
                event->length - event->offset);
            if(nbytes == 0)
            {
                event->error = CHAT_ERROR_EOF;
                return_event = event;
            }
            else if(nbytes == -1)
            {
                event->error = errno;
                return_event = event;
            }
            else
            {
                event->offset += nbytes;
                if(event->offset == event->length)
                {
                    return_event = event;
                }
            }
            break;

        case CHAT_IO_EVENT_TYPE_UDP_READ:
            addr_len = sizeof(event->myaddr);
            nbytes = recvfrom(
                fd,
                event->buffer,
                event->length,
                0,
                (struct sockaddr *)&event->myaddr,
                &addr_len);
            if(nbytes < 0)
            {
                event->error = errno;
            }
            else
            {
                event->offset = nbytes;
                event->udp_from_ip = inet_ntoa(event->myaddr.sin_addr);
                event->udp_from_port = (int)ntohs(event->myaddr.sin_port);
            }

            event->offset = nbytes;
            return_event = event;
            break;

        default:
            assert(0);
            break;
    }
    if(return_event != NULL)
    {
        rc = chat_timeq_remove(q->time_q, event);
        assert(rc == 0);
        q->read_events[fd] = q->read_events[fd]->next;
        if(q->read_events[fd] == NULL)
        {
            FD_CLR(fd, &q->read_fd_set);
        }
    }

    return return_event;
}

/*
 *  when select returns a ready write
 */
static
chat_io_event_t *
chat_l_io_write_ready(
    chat_l_io_event_queue_t *           q,
    int                                 fd)
{
    int                                 rc;
    ssize_t                             nbytes;
    int                                 err;
    int                                 errlen;
    chat_io_event_t *                   event;
    chat_io_event_t *                   return_event = NULL;

    event = (chat_io_event_t *) q->write_events[fd];

    switch(event->type)
    {
        case CHAT_IO_EVENT_TYPE_CONNECT:
            /* see if it connected successfully */
            errlen = sizeof(int);
            rc = getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &errlen);
            if(rc < 0)
            {
                event->error = errno;
            }
            else
            {
                event->error = err;
            }
            return_event = event;
            break;

        case CHAT_IO_EVENT_TYPE_TCP_WRITE:
            nbytes = write(
                fd,
                &event->buffer[event->offset],
                event->length - event->offset);
            if(nbytes == -1)
            {
                event->error = errno;
                return_event = event;
            }
            else
            {
                event->offset += nbytes;
                if(event->offset == event->length)
                {
                    return_event = event;
                }
            }
            break;

        case CHAT_IO_EVENT_TYPE_UDP_WRITE:
            nbytes = sendto(
                fd,
                event->buffer,
                event->length,
                0,
                (struct sockaddr *)&event->myaddr,
                sizeof(event->myaddr));
            if(nbytes < 0)
            {
                event->error = errno;
            }
            else if(nbytes != event->length)
            {
                event->error = CHAT_ERROR_INCOMPLETE;
            }
            event->offset = nbytes;
            return_event = event;
            break;

        default:
            assert(0);
            break;
    }

    if(return_event != NULL)
    {
        rc = chat_timeq_remove(q->time_q, event);
        assert(rc == 0);
        q->write_events[fd] = q->write_events[fd]->next;
        if(q->write_events[fd] == NULL)
        {
            FD_CLR(fd, &q->write_fd_set);
        }
    }

    return return_event;
}

/*
 *  start function visible outside of this file
 */
int
chat_io_eventq_init(
    chat_io_event_q_t *                 out_eventq)
{
    chat_l_io_event_queue_t *           eventq;

    eventq = (chat_l_io_event_queue_t *) calloc(
        1, sizeof(chat_l_io_event_queue_t));
    chat_timeq_init(&eventq->time_q);

    *out_eventq = eventq;

    return CHAT_SUCCESS;
}

int
chat_io_eventq_destroy(
    chat_io_event_q_t                   eventq)
{
    chat_timeq_destroy(eventq->time_q);
    free(eventq);

    return CHAT_SUCCESS;
}

int
chat_io_eventq_wait(
    chat_io_event_q_t                   q)
{
    int                                 i;
    int                                 nready;
    chat_io_event_t *                   event;
    struct timeval                      tm;
    fd_set                              write_fd_set;
    fd_set                              read_fd_set;

    /* first check timeout events */
    event = (chat_io_event_t *) chat_timeq_peek(q->time_q);
    while(event != NULL && event->type == CHAT_IO_EVENT_TYPE_TIMER)
    {
        chat_timeq_dequeue(q->time_q);
        if(event->cb) event->cb(event);
        chat_free_event(event);

        event = (chat_io_event_t *) chat_timeq_peek(q->time_q);
    }

    /* run the select first, then check for timeouts */
    memcpy(&read_fd_set, &q->read_fd_set, sizeof(fd_set));
    memcpy(&write_fd_set, &q->write_fd_set, sizeof(fd_set));
    chat_timeq_next(q->time_q, &tm);
    nready = select(
        q->high_fd+1,
        &read_fd_set,
        &write_fd_set,
        NULL,
        &tm);
    /* if an error occurs bubble it up to the user */
    if(nready < 0)
    {
        /* just return the error to let them know. they will poll again */
        return errno;
    }
    else if(nready > 0)
    {
        /* TODO: add code to make sure higher level fds dont starve */
        for(i = 0; i < CHAT_MAX_FD; i++)
        {
            if(q->write_events[i] != NULL && FD_ISSET(i, &write_fd_set))
            {
                event = chat_l_io_write_ready(q, i);
                if(event != NULL)
                {
                    if(event->cb) event->cb(event);
                    chat_free_event(event);
                }
            }
            if(q->read_events[i] != NULL && FD_ISSET(i, &read_fd_set))
            {
                event = chat_l_io_read_ready(q, i);
                if(event != NULL)
                {
                    if(event->cb) event->cb(event);
                    chat_free_event(event);
                }
            }
        }
    }
    /* scan for timeouts */

    event = chat_timeq_dequeue(q->time_q);
    while(event != NULL)
    {
        switch(event->type)
        {
            case CHAT_IO_EVENT_TYPE_ACCEPT:
            case CHAT_IO_EVENT_TYPE_TCP_READ:
                q->read_events[event->fd] = NULL;
                FD_CLR(event->fd, &q->read_fd_set);
                event->timedout = TRUE;
                break;

            case CHAT_IO_EVENT_TYPE_CONNECT:
            case CHAT_IO_EVENT_TYPE_TCP_WRITE:
                q->write_events[event->fd] = NULL;
                FD_CLR(event->fd, &q->write_fd_set);
                event->timedout = TRUE;
                break;

            case CHAT_IO_EVENT_TYPE_TIMER:
                break;

            default:
                assert(0);
                break;
        }
        if(event->cb) event->cb(event);
        chat_free_event(event);
        event = chat_timeq_dequeue(q->time_q);
    }
    return 0;
}


int
chat_io_connect(
    chat_io_event_q_t                   q,
    char *                              hostname,
    int                                 port,
    chat_io_event_callback_t            cb,
    void *                              user_arg,
    int                                 mtimeout)
{
    struct hostent *                    hp;
    int                                 sock;
    struct sockaddr_in                  addr;
    int                                 done;
    int                                 rc;
    int                                 flags;
    chat_io_event_t *                   event;
    struct linger                       linger;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0)
    {
        goto error;
    }
    hp = gethostbyname(hostname);
    if(hp == NULL)
    {
        goto sock_close_error;
    }
    memset(&addr, '\0', sizeof(addr));
    addr.sin_family = hp->h_addrtype;
    memcpy(&addr.sin_addr,
           hp->h_addr,
           hp->h_length);
    addr.sin_port = htons(port);

    if(q->read_events[sock] != NULL)
    {
        goto error;
    }
    flags = fcntl(sock, F_GETFL);
    flags = flags | O_NONBLOCK;
    fcntl(sock, F_SETFL, flags);

    /* turn on linger */
    linger.l_onoff = 1;
    linger.l_linger = 100;
    if(setsockopt(sock, SOL_SOCKET, SO_LINGER, &linger,
        sizeof(linger)) < 0)
    {
        goto error;
    }

    do
    {
        done = TRUE;
        rc = connect(sock,(struct sockaddr *)&addr,
            sizeof(struct sockaddr_in));
        if(rc < 0)
        {
            switch(errno)
            {
                /* try the connect again */
                case EINTR:
                    done = FALSE;
                    break;
                /* connection cannot be completed imediately */
                case EINPROGRESS:
                    done = TRUE;
                    break;
                /* all other cases are errors */
                default:
                    goto sock_close_error;
            }
        }
    } while(!done);

    event = chat_l_create_event(q, sock, cb, user_arg, mtimeout);
    event->type = CHAT_IO_EVENT_TYPE_CONNECT;
    FD_SET(sock, &q->write_fd_set);
    q->write_events[sock] = event;

    return sock;

sock_close_error:
    close(sock);
error:
    return CHAT_ERROR_SOCKET;
}


/*
 *  function to register a write with the system
 */
int
chat_io_write(
    chat_io_event_q_t                   q,
    int                                 sock,
    chat_byte_t *                       buffer,
    int                                 buffer_length,
    chat_io_event_callback_t            cb,
    void *                              user_arg,
    int                                 mtimeout)
{
    chat_io_event_t *                   event;
    chat_io_event_t *                   i;

    if(q->write_events[sock] != NULL)
    {
        return CHAT_ERROR_SOCKET_IN_USE;
    }
    event = chat_l_create_event(q, sock, cb, user_arg, mtimeout);
    event->type = CHAT_IO_EVENT_TYPE_TCP_WRITE;
    event->buffer = buffer;
    event->length = buffer_length;

    if(q->write_events[sock] == NULL)
    {
        FD_SET(sock, &q->write_fd_set);
        q->write_events[sock] = event;
    }
    else
    {
        for(i = q->write_events[sock]; i->next != NULL; i = i->next)
        {
        }
        i->next = event;
    }
    return CHAT_SUCCESS;
}

int
chat_io_read(
    chat_io_event_q_t                   q,
    int                                 sock,
    chat_byte_t *                       buffer,
    int                                 buffer_length,
    chat_io_event_callback_t            cb,
    void *                              user_arg,
    int                                 mtimeout)
{
    chat_io_event_t *                   event;
    chat_io_event_t *                   i;

    if(q->read_events[sock] != NULL)
    {
        return CHAT_ERROR_SOCKET_IN_USE;
    }
    event = chat_l_create_event(q, sock, cb, user_arg, mtimeout);
    event->type = CHAT_IO_EVENT_TYPE_TCP_READ;
    event->buffer = buffer;
    event->length = buffer_length;
    if(q->read_events[sock] == NULL)
    {
        FD_SET(sock, &q->read_fd_set);
        q->read_events[sock] = event;
    }
    else
    {
        for(i = q->read_events[sock]; i->next != NULL; i = i->next)
        {
        }
        i->next = event;
    }

    return CHAT_SUCCESS;
}

int
chat_io_accept(
    chat_io_event_q_t                   q,
    int                                 listener_sock,
    chat_io_event_callback_t            cb,
    void *                              user_arg,
    int                                 mtimeout)
{
    chat_io_event_t *                   event;

    if(q->read_events[listener_sock] != NULL)
    {
        return CHAT_ERROR_SOCKET_IN_USE;
    }
    event = chat_l_create_event(q, listener_sock, cb, user_arg, mtimeout);
    event->type = CHAT_IO_EVENT_TYPE_ACCEPT;
    event->listener_fd = listener_sock;
    FD_SET(listener_sock, &q->read_fd_set);
    q->read_events[listener_sock] = event;

    return CHAT_SUCCESS;
}

int
chat_io_create_listener(
    int *                               port)
{
    struct sockaddr_in                  addr;
    int                                 sock;
    int                                 rc;
    int                                 flags;
    int                                 len;

    sock = socket(PF_INET, SOCK_STREAM, 0);

    flags = fcntl(sock, F_GETFL);
    flags = flags | O_NONBLOCK;
    fcntl(sock, F_SETFL, flags);

    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(*port);

    rc = bind(sock, (struct sockaddr *)&addr, sizeof(addr));
    if(rc < 0)
    {
        return CHAT_ERROR_SOCKET;
    }

    rc = listen(sock, -1);
    if(rc < 0)
    {
        return CHAT_ERROR_SOCKET;
    }

    len = sizeof(addr);
    getsockname(sock, (struct sockaddr *)&addr, &len);
    *port = ntohs(addr.sin_port);

    return sock;
}

int
chat_io_close(
    chat_io_event_q_t                   q,
    int                                 sock)
{
    int                                 flags;
    chat_io_event_t *                   event;

    /* set back to blocking */
    flags = fcntl(sock, F_GETFL);
    flags = flags & (~O_NONBLOCK);
    fcntl(sock, F_SETFL, flags);

    /* walk through all pending operations can close give back errors */
    if(q != NULL)
    {
        event = q->write_events[sock];
        q->write_events[sock] = NULL;
        while(event != NULL)
        {
            event->error = CHAT_ERROR_CANCELED;
            if(event->cb) event->cb(event);
            event = event->next;
            chat_free_event(event);
        }
        event = q->read_events[sock];
        q->read_events[sock] = NULL;
        while(event != NULL)
        {
            event->error = CHAT_ERROR_CANCELED;
            if(event->cb) event->cb(event);
            event = event->next;
            chat_free_event(event);
        }
    }

    close(sock);

    return CHAT_SUCCESS;
}


int
chat_io_udp_bind(
    int *                               port)
{
    int                                 len;
    int                                 rc;
    int                                 sock;
    struct sockaddr_in                  my_addr;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock < 0)
    {
        rc = sock;
        goto error;
    }

    memset(&my_addr, '\0', sizeof(struct sockaddr_in));

    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(*port);
    rc = bind(sock, (struct sockaddr *)&my_addr, sizeof(my_addr));
    if(rc != 0)
    {
        goto error_bind;
    }
    len = sizeof(my_addr);
    rc = getsockname(sock, (struct sockaddr *)&my_addr, &len);
    if(rc < 0)
    {
        goto error_bind;
    }
    *port = (int) ntohs(my_addr.sin_port);

    return sock;
error_bind:
    close(sock);
error:
    return rc;

}

int
chat_io_recvfrom(
    chat_io_event_q_t                   q,
    int                                 sock,
    chat_byte_t *                       buffer,
    int                                 buffer_length,
    chat_io_event_callback_t            cb,
    void *                              user_arg,
    int                                 mtimeout)
{
    chat_io_event_t *                   event;
    chat_io_event_t *                   i;

    event = chat_l_create_event(q, sock, cb, user_arg, mtimeout);
    event->type = CHAT_IO_EVENT_TYPE_UDP_READ;
    event->buffer = buffer;
    event->length = buffer_length;
    if(q->read_events[sock] == NULL)
    {
        FD_SET(sock, &q->read_fd_set);
        q->read_events[sock] = event;
    }
    else
    {
        for(i = q->read_events[sock]; i->next != NULL; i = i->next)
        {
        }
        i->next = event;
    }

    return CHAT_SUCCESS;
}

int
chat_io_sendto(
    chat_io_event_q_t                   q,
    int                                 sock,
    chat_byte_t *                       buffer,
    int                                 buffer_length,
    const char *                        to_ip,
    int                                 port,
    chat_io_event_callback_t            cb,
    void *                              user_arg,
    int                                 mtimeout)
{
    chat_io_event_t *                   event;
    chat_io_event_t *                   i;

    event = chat_l_create_event(q, sock, cb, user_arg, mtimeout);
    event->type = CHAT_IO_EVENT_TYPE_UDP_WRITE;
    event->buffer = buffer;
    event->length = buffer_length;

    event->myaddr.sin_family = AF_INET;
    event->myaddr.sin_port = htons(port);
    event->myaddr.sin_addr.s_addr = inet_addr(to_ip);

    if(q->write_events[sock] == NULL)
    {
        FD_SET(sock, &q->write_fd_set);
        q->write_events[sock] = event;
    }
    else
    {
        for(i = q->write_events[sock]; i->next != NULL; i = i->next)
        {
        }
        i->next = event;
    }

    return CHAT_SUCCESS;
}


char *
chat_io_getpeername(
    int                                 sock)
{
    int                                 rc;
    struct sockaddr_in                  my_addr;
    socklen_t                           len = sizeof(struct sockaddr_in);
    char *                              name;

    rc = getpeername(sock, (struct sockaddr *) & my_addr, &len);
    if(rc != 0)
    {
        return NULL;
    }

    name = malloc(4*4+1);

    sprintf(name, "%d.%d.%d.%d",
        (int) (((unsigned char * ) &my_addr.sin_addr.s_addr)[0]),
        (int) (((unsigned char * ) &my_addr.sin_addr.s_addr)[1]),
        (int) (((unsigned char * ) &my_addr.sin_addr.s_addr)[2]),
        (int) (((unsigned char * ) &my_addr.sin_addr.s_addr)[3]));

    return name;
}

char *
chat_io_getsockname(
    int                                 sock)
{
    int                                 rc;
    struct sockaddr_in                  my_addr;
    socklen_t                           len = sizeof(struct sockaddr_in);
    char *                              name;

    rc = getpeername(sock, (struct sockaddr *) & my_addr, &len);
    if(rc != 0)
    {
        return NULL;
    }

    name = malloc(4*4+1);

    sprintf(name, "%d.%d.%d.%d",
        (int) (((unsigned char * ) &my_addr.sin_addr.s_addr)[0]),
        (int) (((unsigned char * ) &my_addr.sin_addr.s_addr)[1]),
        (int) (((unsigned char * ) &my_addr.sin_addr.s_addr)[2]),
        (int) (((unsigned char * ) &my_addr.sin_addr.s_addr)[3]));

    return name;
}


int
chat_io_timer(
    chat_io_event_q_t                   q,
    chat_io_event_callback_t            cb,
    void *                              user_arg,
    void **                             cancel_handle,
    int                                 mtimeout)
{
    chat_io_event_t *                   event;

    event = chat_l_create_event(q, -1, cb, user_arg, mtimeout);
    event->type = CHAT_IO_EVENT_TYPE_TIMER;

    if(cancel_handle != NULL)
    {
        *cancel_handle = event;
    }

    return CHAT_SUCCESS;
}

int
chat_io_timer_cancel(
    chat_io_event_q_t                   q,
    void *                              cancel_handle)
{
    int                                 rc;

    rc = chat_timeq_remove(q->time_q, cancel_handle);
    chat_free_event(cancel_handle);
    assert(rc == 0);

    return CHAT_SUCCESS;
}


