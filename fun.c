#include <ncurses.h>
#include <stdlib.h>
#include "chat.h"

#define     GAME_TIC  200

static char *                           g_exit_msg = NULL;
static char **                          g_map;
static int                              g_map_length;

static chat_byte_t                      g_msg_buffer[16];
WINDOW *                                g_main_scr;
static int                              g_max_x;
static int                              g_max_y;
static int                              g_border_color;
static int                              g_blank_color;
static int                              g_done = 0;
static int                              g_sock;
    chat_io_event_q_t                   g_q;

enum
{
    MSG_TYPE_MAP = '1',
    MSG_TYPE_WIN = '2',
    MSG_TYPE_LOSE = '3'
};

enum
{
    MAP_TYPE_EMPTY = ' ',
    MAP_TYPE_BORDER = '#',
    MAP_TYPE_P1 = '@',
    MAP_TYPE_P1_TRAIL = '*',
    MAP_TYPE_P2 = '%',
    MAP_TYPE_P2_TRAIL = '+',

    MAP_TYPE_P1_UP = 'U',
    MAP_TYPE_P1_DOWN = 'D',
    MAP_TYPE_P1_LEFT = 'L',
    MAP_TYPE_P1_RIGHT = 'R',

    MAP_TYPE_P2_UP = 'u',
    MAP_TYPE_P2_DOWN = 'd',
    MAP_TYPE_P2_LEFT = 'l',
    MAP_TYPE_P2_RIGHT = 'r'
};

typedef struct player_s
{
    int                                 x;
    int                                 y;
    char                                ch;
    int                                 dir;
    int                                 cp;
    int                                 bk_cp;
    char                                map_char;
    char                                map_trail_char;
} player_t;

int
move_player(
    player_t *                          p);

void
send_map();

static
void
client_header_read_cb(
    chat_io_event_t *                   event);

static player_t                         g_p1;
static player_t                         g_p2;
static int                              g_connected = 0;
static int                              g_is_server = 1;
static player_t *                       g_me;
static player_t *                       g_enemy;
/*******************************************************************
*
*******************************************************************/

void
fun_exit()
{
    clear();
    endwin();
    if(g_exit_msg != NULL)
    {
        printf("%s\n", g_exit_msg);
    }
}

void draw_map()
{
    int                                 x;
    int                                 y;
    char                                ch;

    for(y = 0; y < g_max_y; y++)
    {
        for(x = 0; x < g_max_x; x++)
        {
            move(y, x);
            switch(g_map[y][x])
            {
                case MAP_TYPE_EMPTY:
                    color_set(g_blank_color, NULL);
                    ch = g_map[y][x];
                    break;

                case MAP_TYPE_BORDER:
                    color_set(g_border_color, NULL);
                    ch = g_map[y][x];
                    break;

                case MAP_TYPE_P1:
                    color_set(g_p1.cp, NULL);
                    ch = g_map[y][x];
                    break;

                case MAP_TYPE_P1_TRAIL:
                    color_set(g_p1.bk_cp, NULL);
                    ch = g_map[y][x];
                    break;

                case MAP_TYPE_P2:
                    color_set(g_p2.cp, NULL);
                    ch = g_map[y][x];
                    break;

                case MAP_TYPE_P2_TRAIL:
                    color_set(g_p2.bk_cp, NULL);
                    ch = g_map[y][x];
                    break;

                case MAP_TYPE_P1_UP:
                    color_set(g_p1.cp, NULL);
                    ch = '^';
                    break;

                case MAP_TYPE_P1_DOWN:
                    color_set(g_p1.cp, NULL);
                    ch = 'v';
                    break;

                case MAP_TYPE_P1_LEFT:
                    ch = '<';
                    color_set(g_p1.cp, NULL);
                    break;

                case MAP_TYPE_P1_RIGHT:
                    color_set(g_p1.cp, NULL);
                    ch = '>';
                    break;

                case MAP_TYPE_P2_UP:
                    color_set(g_p2.cp, NULL);
                    ch = '^';
                    break;

                case MAP_TYPE_P2_DOWN:
                    color_set(g_p2.cp, NULL);
                    ch = 'v';
                    break;

                case MAP_TYPE_P2_LEFT:
                    color_set(g_p2.cp, NULL);
                    ch = '<';
                    break;

                case MAP_TYPE_P2_RIGHT:
                    color_set(g_p2.cp, NULL);
                    ch = '>';
                    break;

            }
            addch(ch);
        }
    }
    move(0, 0);
    refresh();
}


void init_screen()
{
    int                                 i;
    int                                 color = 1;

    g_main_scr = initscr();
    atexit(fun_exit);
    start_color();
    cbreak();
    noecho();
    halfdelay(1);
    keypad(g_main_scr, 1);

    getmaxyx(g_main_scr, g_max_y, g_max_x);

    g_map = (char **) calloc(g_max_y, sizeof(char*));
    for(i = 0; i < g_max_y; i++)
    {
        g_map[i] = (char *) calloc(g_max_x, sizeof(char));
        memset(g_map[i], MAP_TYPE_EMPTY, g_max_x * sizeof(char));
    }
    g_map_length = g_max_y * g_max_x;

    for(i = 0; i < g_max_x; i++)
    {
        g_map[0][i] = MAP_TYPE_BORDER;
        g_map[g_max_y-1][i] = MAP_TYPE_BORDER;
    }
    for(i = 0; i < g_max_y; i++)
    {
        g_map[i][0] = MAP_TYPE_BORDER;
        g_map[i][g_max_x-1] = MAP_TYPE_BORDER;
    }
    clear();

    g_border_color = color++;
    init_pair(g_border_color, COLOR_RED, COLOR_BLUE);
    g_blank_color = color++;
    init_pair(g_blank_color, COLOR_WHITE, COLOR_BLACK);

    g_p1.x = 5;
    g_p1.y = 5;
    g_p1.ch = '*';
    g_p1.dir = KEY_DOWN;
    g_p1.cp = color++;
    g_p1.bk_cp = color++;
    g_p1.map_char = MAP_TYPE_P1;
    g_p1.map_trail_char = MAP_TYPE_P1_TRAIL;
    init_pair(g_p1.cp, COLOR_RED, COLOR_GREEN);
    init_pair(g_p1.bk_cp, COLOR_RED, COLOR_BLACK);

    g_p2.x = g_max_x - 5;
    g_p2.y = g_max_y - 5;;
    g_p2.ch = '*';
    g_p2.dir = KEY_UP;
    g_p2.cp = color++;
    g_p2.bk_cp = color++;
    g_p2.map_char = MAP_TYPE_P2;
    g_p2.map_trail_char = MAP_TYPE_P2_TRAIL;
    init_pair(g_p2.cp, COLOR_GREEN, COLOR_RED);
    init_pair(g_p2.bk_cp, COLOR_GREEN, COLOR_BLACK);

    if(g_is_server)
    {
        g_me = &g_p1;
        g_enemy = &g_p2;
    }
    else
    {
        g_me = &g_p2;
        g_enemy = &g_p1;
    }

}

int
move_player(
    player_t *                          p)
{
    char                                ch;

    color_set(p->bk_cp, NULL);
    move(p->y, p->x);
    g_map[p->y][p->x] = p->map_trail_char;
    switch(p->dir)
    {
        case KEY_UP:
            if(p == &g_p1)
            {
                ch = MAP_TYPE_P1_UP;
            }
            else
            {
                ch = MAP_TYPE_P2_UP;
            }
            p->y--;
            break;

        case KEY_DOWN:
            if(p == &g_p1)
            {
                ch = MAP_TYPE_P1_DOWN;
            }
            else
            {
                ch = MAP_TYPE_P2_DOWN;
            }
            p->y++;
            break;

        case KEY_LEFT:
            if(p == &g_p1)
            {
                ch = MAP_TYPE_P1_LEFT;
            }
            else
            {
                ch = MAP_TYPE_P2_LEFT;
            }
            p->x--;
            break;

        case KEY_RIGHT:
            if(p == &g_p1)
            {
                ch = MAP_TYPE_P1_RIGHT;
            }
            else
            {
                ch = MAP_TYPE_P2_RIGHT;
            }
            p->x++;
            break;
    }
    color_set(p->cp, NULL);
    move(p->y, p->x);
    if(g_map[p->y][p->x] != MAP_TYPE_EMPTY)
    {
        return 1;
    }
    g_map[p->y][p->x] = ch;
    move(0, 0);

    return 0;
}

static
void
client_body_read_cb(
    chat_io_event_t *                   event)
{
    int                                 x;
    int                                 y;
    int                                 i = 0;

    if(event->timedout || event->error != 0)
    {
        fprintf(stderr, "Connection failed\n");
        g_exit_msg = "Net error";
        exit(1);
    }
    for(y = 0; y < g_max_y; y++)
    {
        for(x = 0; x < g_max_x; x++)
        {
            switch(event->buffer[i])
            {
                case MAP_TYPE_EMPTY:
                    color_set(g_blank_color, NULL);
                    break;

                case MAP_TYPE_BORDER:
                    color_set(g_border_color, NULL);
                    break;

                case MAP_TYPE_P1:
                    color_set(g_p1.cp, NULL);
                    break;

                case MAP_TYPE_P1_TRAIL:
                    color_set(g_p1.bk_cp, NULL);
                    break;

                case MAP_TYPE_P2:
                    color_set(g_p2.cp, NULL);
                    break;

                case MAP_TYPE_P2_TRAIL:
                    color_set(g_p2.bk_cp, NULL);
                    break;

                case MAP_TYPE_P1_UP:
                    color_set(g_p1.cp, NULL);
                    event->buffer[i] = '^';
                    break;

                case MAP_TYPE_P1_DOWN:
                    color_set(g_p1.cp, NULL);
                    event->buffer[i] = 'v';
                    break;

                case MAP_TYPE_P1_LEFT:
                    event->buffer[i] = '<';
                    color_set(g_p1.cp, NULL);
                    break;

                case MAP_TYPE_P1_RIGHT:
                    color_set(g_p1.cp, NULL);
                    event->buffer[i] = '>';
                    break;

                case MAP_TYPE_P2_UP:
                    color_set(g_p2.cp, NULL);
                    event->buffer[i] = '^';
                    break;

                case MAP_TYPE_P2_DOWN:
                    color_set(g_p2.cp, NULL);
                    event->buffer[i] = 'v';
                    break;

                case MAP_TYPE_P2_LEFT:
                    color_set(g_p2.cp, NULL);
                    event->buffer[i] = '<';
                    break;

                case MAP_TYPE_P2_RIGHT:
                    color_set(g_p2.cp, NULL);
                    event->buffer[i] = '>';
                    break;
            }
            move(y, x);
            addch(event->buffer[i]);
            i++;
        }
    }
    refresh();
    chat_io_read(
        event->q,
        event->fd,
        g_msg_buffer,
        3,
        client_header_read_cb,
        NULL,
        -1);
}

static
void
client_header_read_cb(
    chat_io_event_t *                   event)
{
    char                                type;
    int                                 max_x;
    int                                 max_y;
    chat_byte_t *                       buffer;

    if(event->timedout || event->error != 0)
    {
        fprintf(stderr, "Connection failed\n");
        g_exit_msg = "Net error";
        exit(1);
    }
    type = event->buffer[0];
    switch(type)
    {
        case MSG_TYPE_WIN:
            move(g_max_y / 2, g_max_x / 2);
            addstr("You Win!");
            g_exit_msg = "You won.";
            refresh();
            sleep(5);
            exit(1);
            break;

        case MSG_TYPE_LOSE:
            move(g_max_y / 2, g_max_x / 2);
            addstr("You Lose!");
            g_exit_msg = "You lost.";
            refresh();
            sleep(5);
            exit(2);
            break;

        case MSG_TYPE_MAP:
            max_y = (int) event->buffer[1];
            max_x = (int) event->buffer[2];

            if(max_x > g_max_x || max_y > g_max_y)
            {
                move(g_max_y / 2, g_max_x / 2);
                addstr("Screen dimensions are too small");
                g_exit_msg = "Your screen dimensions are too small.";
                refresh();
                sleep(5);
                exit(2);
            }
            buffer = (chat_byte_t *)malloc(max_y*max_x);
            g_max_x = max_x;
            g_max_y = max_y;
            chat_io_read(
                event->q,
                event->fd,
                buffer,
                max_y*max_x,
                client_body_read_cb,
                NULL,
                -1);
            break;
    }
}

static
void
connect_cb(
    chat_io_event_t *                   event)
{
    chat_byte_t *                       buffer;

    if(event->timedout || event->error != 0)
    {
        fprintf(stderr, "Connection failed\n");
        g_exit_msg = "Net error";
        exit(1);
    }
    init_screen();
    buffer = malloc(3);
    chat_io_read(
        event->q,
        event->fd,
        buffer,
        3,
        client_header_read_cb,
        NULL,
        -1);
    g_connected = 1;
}

static
void
server_read_cb(
    chat_io_event_t *                   event)
{
    int                                 ch;

    if(event->timedout || event->error != 0)
    {
        fprintf(stderr, "Connection failed\n");
        g_exit_msg = "Net error";
        exit(1);
    }

    ch = g_enemy->dir;
    switch(g_msg_buffer[1])
    { 
        case 'U':
            ch = KEY_UP;
            break;

        case 'L':
            ch = KEY_LEFT;
            break;

        case 'D':
            ch = KEY_DOWN;
            break;

        case 'R':
            ch = KEY_RIGHT;
            break;

        default:
            break;
    }
    g_enemy->dir = ch;

    chat_io_read(
        g_q,
        g_sock,
        g_msg_buffer,
        3,
        server_read_cb,
        NULL,
        -1);
}

static
void
accept_cb(
    chat_io_event_t *                   event)
{
    if(event->timedout || event->error != 0)
    {
        fprintf(stderr, "Connection failed\n");
        g_exit_msg = "Net error";
        exit(1);
    }
    g_sock = event->fd;
    init_screen();
    send_map();

    chat_io_read(
        g_q,
        g_sock,
        g_msg_buffer,
        3,
        server_read_cb,
        NULL,
        -1);
}

static
void
done_write_cb(
    chat_io_event_t *                   event)
{
    sleep(5);
    exit(1); 
}

static
void
game_tic(
    chat_io_event_t *                   event)
{
    chat_byte_t *                       buf = NULL;
    int                                 rc;

    draw_map();
    rc = move_player(g_me);
    if(rc == 1)
    {
        move(g_max_y / 2, g_max_x / 2);
        addstr("You Lose!");
        g_exit_msg = "You lost.";
        refresh();
        buf = malloc(3);
        buf[0] = MSG_TYPE_WIN;
    }
    rc = move_player(g_enemy);
    if(rc == 1)
    {
        move(g_max_y / 2, g_max_x / 2);
        addstr("You Win!");
        g_exit_msg = "You won.";
        refresh();
        buf = malloc(3);
        buf[0] = MSG_TYPE_LOSE;
    }

    if(buf != NULL)
    {
        chat_io_write(
            g_q,
            g_sock,
            buf,
            3,
            done_write_cb,
            NULL,
            -1);
    }
    else
    {
        send_map();
        chat_io_timer(event->q, game_tic, NULL, NULL, GAME_TIC);
    }
}

static
void
server_poll_keyboard(
    chat_io_event_t *                   event)
{
    int                                 ch;

    ch = wgetch(g_main_scr);
    if(ch != ERR)
    {
        switch(ch)
        {
            case KEY_RIGHT:
            case KEY_LEFT:
            case KEY_DOWN:
            case KEY_UP:
                g_me->dir = ch;
                break;

            default:
                break;
        }
        do
        {
            ch = wgetch(g_main_scr);
        } while(ch != ERR);
    }
    chat_io_timer(event->q, server_poll_keyboard, NULL, NULL, 5);
}

static
void
write_cb(
    chat_io_event_t *                   event)
{
    g_connected = 1;
}

static
void
client_poll_keyboard(
    chat_io_event_t *                   event)
{
    int                                 ch;
    char                                send_ch = '\0';

    ch = wgetch(g_main_scr);
    if(ch != ERR)
    {
        switch(ch)
        {
            case KEY_RIGHT:
                send_ch = 'R';
                break;

            case KEY_LEFT:
                send_ch = 'L';
                break;

            case KEY_DOWN:
                send_ch = 'D';
                break;

            case KEY_UP:
                send_ch = 'U';
                break;

            default:
                break;
        }
        do
        {
            ch = wgetch(g_main_scr);
        } while(ch != ERR);
    }
    if(send_ch != '\0')
    {
        chat_byte_t * buffer = malloc(3);

        buffer[1] = send_ch;
        chat_io_write(
            g_q,
            g_sock,
            buffer,
            3,
            write_cb,
            NULL,
            -1);
    }

    chat_io_timer(event->q, client_poll_keyboard, NULL, NULL, 5);
}


void
send_map()
{
    int                                 i;
    int                                 x;
    int                                 y;
    chat_byte_t *                       buffer;

    i = 0;
    buffer = malloc(g_map_length + 3);
    buffer[i++] = (char)MSG_TYPE_MAP;
    buffer[i++] = (char)g_max_y;
    buffer[i++] = (char)g_max_x;

    for(y = 0; y < g_max_y; y++)
    {
        for(x = 0; x < g_max_x; x++)
        {
            buffer[i++] = g_map[y][x];
        }
    }

    chat_io_write(
        g_q,
        g_sock,
        buffer,
        i,
        write_cb,
        NULL,
        -1);
}

int
main(
    int                                 argc,
    char **                             argv)
{
    int                                 server_fd;
    int                                 port = 0;
    char *                              host;


    if(argc > 1)
    {
        g_is_server = 0;
        host = argv[1];
        port = atoi(argv[2]);
    }
    else
    {
        g_is_server = 1;
    }

    chat_io_eventq_init(&g_q);

    if(g_is_server)
    {
        server_fd = chat_io_create_listener(&port);
        printf("waiting for a client connection on port %d\n", port);
        chat_io_accept(g_q, server_fd, accept_cb, NULL, -1);
    }
    else
    {
        g_sock = chat_io_connect(g_q, host, port, connect_cb, NULL, -1);
    }
    printf("Connected to other player, initializing...\n");
    while(!g_connected)
    {
        chat_io_eventq_wait(g_q);
    }


    if(g_is_server)
    {
        chat_io_timer(g_q, server_poll_keyboard, NULL, NULL, 5);
        chat_io_timer(g_q, game_tic, NULL, NULL, GAME_TIC);
    }
    else
    {
        chat_io_timer(g_q, client_poll_keyboard, NULL, NULL, 5);
    }

    while(!g_done)
    {
        chat_io_eventq_wait(g_q);
    }
    return 0;
}

