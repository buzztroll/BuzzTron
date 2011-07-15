/* 
 *  asynchorous io.
 *
 *  This allows write to post io events for async completion.  Once the
 *  requests is completed an event is delivered to the user via a 
 *  callback.  Timer events can be registered as wall.
 */
#if !defined(CHAT_IO_H)
#define CHAT_IO_H 1

#include <netinet/tcp.h>
#include <netdb.h>
#include <sys/time.h>

#define TRUE                            1
#define FALSE                           0

#define CHAT_MAX_TIMEOUT                3600

typedef enum
{
    CHAT_IO_EVENT_TYPE_ACCEPT = 1,
    CHAT_IO_EVENT_TYPE_CONNECT,
    CHAT_IO_EVENT_TYPE_TCP_READ,
    CHAT_IO_EVENT_TYPE_TCP_WRITE,
    CHAT_IO_EVENT_TYPE_UDP_READ,
    CHAT_IO_EVENT_TYPE_UDP_WRITE,
    CHAT_IO_EVENT_TYPE_TIMER
} chat_io_event_type_t;

typedef unsigned char                   chat_byte_t;
typedef struct chat_l_io_event_queue_s* chat_io_event_q_t;
struct chat_io_event_s;

typedef void
(*chat_io_event_callback_t)(
    struct chat_io_event_s *            event);

typedef struct chat_io_event_s
{
    chat_io_event_type_t                type;
    chat_io_event_q_t                   q;
    int                                 fd;
    chat_io_event_callback_t            cb;
    void *                              user_arg;
    chat_byte_t *                       buffer;
    int                                 length;
    int                                 offset;
    int                                 timedout;
    int                                 listener_fd;
    int                                 error;
    struct timeval                      timeout;
    struct sockaddr_in                  myaddr;
    char *                              udp_from_ip;
    int                                 udp_from_port;
    struct chat_io_event_s *            next;
} chat_io_event_t;

int
chat_io_eventq_init(
    chat_io_event_q_t *                 out_eventq);

int
chat_io_eventq_destroy(
    chat_io_event_q_t                   eventq);

int
chat_io_eventq_wait(
    chat_io_event_q_t                   eventq);

int
chat_io_connect(
    chat_io_event_q_t                   q,
    char *                              hostname,
    int                                 port,
    chat_io_event_callback_t            cb,
    void *                              user_arg,
    int                                 mtimeout);

int
chat_io_write(
    chat_io_event_q_t                   eventq,
    int                                 sock,
    chat_byte_t *                       buffer,
    int                                 buffer_length,
    chat_io_event_callback_t            cb,
    void *                              user_arg,
    int                                 msectimeout);

int
chat_io_read(
    chat_io_event_q_t                   eventq,
    int                                 sock,
    chat_byte_t *                       buffer,
    int                                 buffer_length,
    chat_io_event_callback_t            cb,
    void *                              user_arg,
    int                                 msectimeout);

int
chat_io_accept(
    chat_io_event_q_t                   eventq,
    int                                 listener_sock,
    chat_io_event_callback_t            cb,
    void *                              user_arg,
    int                                 msectimeout);

int
chat_io_timer(
    chat_io_event_q_t                   eventq,
    chat_io_event_callback_t            cb,
    void *                              user_arg,
    void **                             cancel_handle,
    int                                 msectimeout);

int
chat_io_timer_cancel(
    chat_io_event_q_t                   eventq,
    void *                              cancel_handle);

int
chat_io_timer_remove(
    chat_io_event_q_t                   eventq,
    void *                              user_arg);

int
chat_io_create_listener(
    int *                               port);

int
chat_io_close(
    chat_io_event_q_t                   q,
    int                                 sock);

int
chat_io_udp_bind(
    int *                               port);

int
chat_io_recvfrom(
    chat_io_event_q_t                   eventq,
    int                                 sock,
    chat_byte_t *                       buffer,
    int                                 buffer_length,
    chat_io_event_callback_t            cb,
    void *                              user_arg,
    int                                 msectimeout);

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
    int                                 mtimeout);

char *
chat_io_getpeername(
    int                                 sock);

char *
chat_io_getsockname(
    int                                 sock);

#endif
