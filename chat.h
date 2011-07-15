#if !defined(CHAT_H)
#define CHAT_H

#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <assert.h>
#include <unistd.h>
#include "chat_io.h"
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>

#include "chat_io.h"
#include "chat_timeq.h"
#include "chat_table.h"

#define TRUE                            1
#define FALSE                           0
#define CHAT_CONNECTION_HEADER_LENGTH   3
#define CHAT_REREGISTER_TIME            (CHAT_SOFT_STATE/2)
/*
#define CHAT_DEFAULT_TIMEOUT            60000
#define CHAT_SOFT_STATE                 300000
#define CHAT_IDLE_CHANNEL_TIMEOUT       120000
*/
#define CHAT_DEFAULT_TIMEOUT            -1
#define CHAT_SOFT_STATE                 360000
#define CHAT_IDLE_CHANNEL_TIMEOUT       240000
#define CHAT_FILE_CONNECT_TIMEOUT       60000


typedef enum
{
    CHAT_SUCCESS = 0,
    CHAT_ERROR_PROTOCOL = -1,
    CHAT_ERROR_USERNAME_EXISTS = -2,
    CHAT_ERROR_USERNAME_NOT_FOUND = -3,
    CHAT_ERROR_SOCKET_IN_USE = -4,
    CHAT_ERROR_SOCKET = -5,
    CHAT_ERROR_CANCELED = -6,
    CHAT_ERROR_EOF = -7,
    CHAT_ERROR_INCOMPLETE = -8,
    CHAT_ERROR_CLOSED = -9
} chat_error_t;

void
chat_debug_set_level(
    int                                 level);

int
chat_debug_print(
    int                                 level,
    const char *                        fmt,
    ...);


#endif
