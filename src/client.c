
#include <dc_application/command_line.h>
#include <dc_application/config.h>
#include <dc_application/defaults.h>
#include <dc_application/environment.h>
#include <dc_application/options.h>
#include <dc_posix/dc_stdlib.h>
#include <dc_posix/dc_string.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>


struct application_settings
{
    struct dc_opt_settings opts;
    struct dc_setting_string *start;
    struct dc_setting_string *packets;
    struct dc_setting_string *serverip;
    struct dc_setting_string *serverport;
    struct dc_setting_string *size;
    struct dc_setting_string *delay;
};



static struct dc_application_settings *create_settings(const struct dc_posix_env *env, struct dc_error *err);
static int destroy_settings(const struct dc_posix_env *env,
                            struct dc_error *err,
                            struct dc_application_settings **psettings);
static int run(const struct dc_posix_env *env, struct dc_error *err, struct dc_application_settings *settings);
static void error_reporter(const struct dc_error *err);
static void trace_reporter(const struct dc_posix_env *env,
                           const char *file_name,
                           const char *function_name,
                           size_t line_number);


int run_udp(const struct dc_posix_env *env, struct dc_error *error, void *arg);


int main(int argc, char *argv[])
{
    dc_posix_tracer tracer;
    dc_error_reporter reporter;
    struct dc_posix_env env;
    struct dc_error err;
    struct dc_application_info *info;
    int ret_val;

    reporter = error_reporter;
    tracer = trace_reporter;
    tracer = NULL;
    dc_error_init(&err, reporter);
    dc_posix_env_init(&env, tracer);
    info = dc_application_info_create(&env, &err, "Settings Application");
    ret_val = dc_application_run(&env, &err, info, create_settings, destroy_settings, run, dc_default_create_lifecycle, dc_default_destroy_lifecycle, NULL, argc, argv);
    dc_application_info_destroy(&env, &info);
    dc_error_reset(&err);

    return ret_val;
}

static struct dc_application_settings *create_settings(const struct dc_posix_env *env, struct dc_error *err)
{
    struct application_settings *settings;

    DC_TRACE(env);
    settings = dc_malloc(env, err, sizeof(struct application_settings));

    if(settings == NULL)
    {
        return NULL;
    }

    settings->opts.parent.config_path = dc_setting_path_create(env, err);
    settings->start = dc_setting_string_create(env, err);
    settings->packets = dc_setting_string_create(env, err);
    settings->serverip = dc_setting_string_create(env, err);
    settings->serverport = dc_setting_string_create(env, err);
    settings->size = dc_setting_string_create(env, err);
    settings->delay = dc_setting_string_create(env, err);

    struct options opts[] = {
            {(struct dc_setting *)settings->opts.parent.config_path,
                    dc_options_set_path,
                    "config",
                    required_argument,
                    'c',
                    "CONFIG",
                    dc_string_from_string,
                    NULL,
                    dc_string_from_config,
                    NULL},
            {(struct dc_setting *)settings->start,
                    dc_options_set_string,
                    "start",
                    required_argument,
                    'm',
                    "MESSAGE",
                    dc_string_from_string,
                    "message",
                    dc_string_from_config,
                    NULL},
            {(struct dc_setting *)settings->packets,
                    dc_options_set_string,
                    "packets",
                    required_argument,
                    'm',
                    "MESSAGE",
                    dc_string_from_string,
                    "message",
                    dc_string_from_config,
                    "100"},
            {(struct dc_setting *)settings->serverip,
                    dc_options_set_string,
                    "serverip",
                    required_argument,
                    'm',
                    "MESSAGE",
                    dc_string_from_string,
                    "message",
                    dc_string_from_config,
                    NULL},
            {(struct dc_setting *)settings->serverport,
                    dc_options_set_string,
                    "serverport",
                    required_argument,
                    'm',
                    "MESSAGE",
                    dc_string_from_string,
                    "message",
                    dc_string_from_config,
                    "4981"},
            {(struct dc_setting *)settings->size,
                    dc_options_set_string,
                    "size",
                    required_argument,
                    'm',
                    "MESSAGE",
                    dc_string_from_string,
                    "message",
                    dc_string_from_config,
                    "100"},
            {(struct dc_setting *)settings->delay,
                    dc_options_set_string,
                    "delay",
                    required_argument,
                    'm',
                    "MESSAGE",
                    dc_string_from_string,
                    "message",
                    dc_string_from_config,
                    "50"},
    };

    // note the trick here - we use calloc and add 1 to ensure the last line is all 0/NULL
    settings->opts.opts_count = (sizeof(opts) / sizeof(struct options)) + 1;
    settings->opts.opts_size = sizeof(struct options);
    settings->opts.opts = dc_calloc(env, err, settings->opts.opts_count, settings->opts.opts_size);
    dc_memcpy(env, settings->opts.opts, opts, sizeof(opts));
    settings->opts.flags = "m:";
    settings->opts.env_prefix = "DC_EXAMPLE_";

    return (struct dc_application_settings *)settings;
}

static int destroy_settings(const struct dc_posix_env *env,
                            __attribute__((unused)) struct dc_error *err,
                            struct dc_application_settings **psettings)
{
    struct application_settings *app_settings;

    DC_TRACE(env);
    app_settings = (struct application_settings *)*psettings;
    dc_setting_string_destroy(env, &app_settings->start);
    dc_setting_string_destroy(env, &app_settings->packets);
    dc_setting_string_destroy(env, &app_settings->serverip);
    dc_setting_string_destroy(env, &app_settings->serverport);
    dc_setting_string_destroy(env, &app_settings->size);
    dc_setting_string_destroy(env, &app_settings->delay);

    dc_free(env, app_settings->opts.opts, app_settings->opts.opts_count);
    dc_free(env, *psettings, sizeof(struct application_settings));

    if(env->null_free)
    {
        *psettings = NULL;
    }

    return 0;
}

struct info
{
    char *start;
    int packets;
    char *serverip;
    int serverport;
    int size;
    int delay;
};

static int run(const struct dc_posix_env *env, struct dc_error *err, struct dc_application_settings *settings)
{
    struct application_settings *app_settings;
    const char *start_str;
    const char *packets_str;
    const char *serverip_str;
    const char *serverport_str;
    const char *size_str;
    const char *delay_str;

    int ret_val;

    DC_TRACE(env);

    app_settings = (struct application_settings *)settings;
    start_str = dc_setting_string_get(env, app_settings->start);
    packets_str = dc_setting_string_get(env, app_settings->packets);
    serverip_str = dc_setting_string_get(env, app_settings->serverip);
    serverport_str = dc_setting_string_get(env, app_settings->serverport);
    size_str = dc_setting_string_get(env, app_settings->size);
    delay_str = dc_setting_string_get(env, app_settings->delay);


    printf("start %s\n", start_str);
    printf("packets %s\n", packets_str);
    printf("serverip %s\n", serverip_str);
    printf("serverport %s\n", serverport_str);
    printf("size %s\n", size_str);
    printf("delay %s\n", delay_str);

    struct info info;
    info.start = strdup(start_str);
    info.packets = atoi(packets_str);
    info.serverip = strdup(serverip_str);
    info.serverport = atoi(serverport_str);
    info.size = atoi(size_str);
    info.delay = atoi(delay_str);

    ret_val = run_udp(env, err, &info);

    return ret_val;
}

static void error_reporter(const struct dc_error *err)
{
    fprintf(stderr, "ERROR: %s : %s : @ %zu : %d\n", err->file_name, err->function_name, err->line_number, 0);
    fprintf(stderr, "ERROR: %s\n", err->message);
}

static void trace_reporter(__attribute__((unused)) const struct dc_posix_env *env,
                           const char *file_name,
                           const char *function_name,
                           size_t line_number)
{
    fprintf(stdout, "TRACE: %s : %s : @ %zu\n", file_name, function_name, line_number);
}

#include <dc_fsm/fsm.h>
#include <dc_posix/dc_posix_env.h>
#include <stdio.h>

/*! \enum state
    \brief The possible FSM states.

    The state that the FSM can be in.
*/

enum shell_states
{
    MAKE_CONNECTION = DC_FSM_USER_START, /**< the initial state */             //  2
    SEND_START_DATA,                  /**< accept user input */             //  3
    SEND_DATA,              /**< separate the commands */         //  4
    SEND_END_DATA,                 /**< parse the commands */            //  5
    CLOSE_CONNECTION,               /**< execute the commands */          //  6
    EXIT,                           /**< exit the shell */                //  7
    ERROR,                          /**< handle errors */                 //  9
};

int make_tcp_connection(const struct dc_posix_env *env, struct dc_error *err, void *arg);
static int send_start(const struct dc_posix_env *env, struct dc_error *err, int socket_fd, void *arg);
static int send_udp(const struct dc_posix_env *env, struct dc_error *err, int socket_fd, void *arg);
static int send_end(const struct dc_posix_env *env, struct dc_error *err, int socket_fd, void *arg);
static int close_tcp_connection(const struct dc_posix_env *env, struct dc_error *err, int socket_fd, void *arg);



int run_udp(const struct dc_posix_env *env, struct dc_error *error, void *arg)
{
    struct info *info;
    info = arg;

    int ret_val;
    struct dc_fsm_info *fsm_info;
    static struct dc_fsm_transition transitions[] = {
            {DC_FSM_INIT, MAKE_CONNECTION,     make_tcp_connection},
            {MAKE_CONNECTION,     SEND_START_DATA, send_start},
//            {MAKE_CONNECTION,     ERROR, handle_error},
            {SEND_START_DATA,     SEND_DATA, send_udp},
//            {SEND_START_DATA,       ERROR, separate_commands},
            {SEND_DATA,     SEND_END_DATA, send_end},
//            {SEND_DATA,     ERROR, handle_error},
            {SEND_END_DATA,     CLOSE_CONNECTION, close_tcp_connection},
//            {SEND_END_DATA,     ERROR, execute_commands},
//            {ERROR,     EXIT, handle_error},
//            {CLOSE_CONNECTION,     EXIT, reset_state},
//            {EXIT,     DC_FSM_EXIT, NULL},
    };

    dc_error_init(error, error->reporter);
    dc_posix_env_init(env, NULL /*trace_reporter*/);
    ret_val = EXIT_SUCCESS;
    fsm_info = dc_fsm_info_create(env, error, "traffic");

    if(dc_error_has_no_error(error))
    {
    //    struct info info;
        int from_state;
        int to_state;

//        state.stdin = in;
//        state.stdout = out;
//        state.stderr = err;

        ret_val = dc_fsm_run(env, error, fsm_info, &from_state, &to_state, info, transitions);
        dc_fsm_info_destroy(env, &fsm_info);
    }

    return ret_val;
}





#include <dc_posix/dc_netdb.h>
#include <dc_posix/dc_posix_env.h>
#include <dc_posix/dc_unistd.h>
#include <dc_posix/dc_signal.h>
#include <dc_posix/dc_string.h>
#include <dc_posix/sys/dc_socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


//static void error_reporter(const struct dc_error *err);
//static void trace_reporter(const struct dc_posix_env *env, const char *file_name,
//                           const char *function_name, size_t line_number);
static void quit_handler(int sig_num);



static volatile sig_atomic_t exit_flag;


int make_tcp_connection(const struct dc_posix_env *env, struct dc_error *err, void *arg)
{
    struct info *info;
    info = arg;

    printf("in tcp connection port = %d\n", info->serverport);

    dc_error_reporter reporter;
    dc_posix_tracer tracer;
//    struct dc_error err;
//    struct dc_posix_env env;
    const char *host_name;
    struct addrinfo hints;
    struct addrinfo *result;

    reporter = error_reporter;
    tracer = trace_reporter;
    tracer = NULL;
    dc_error_init(err, reporter);
    dc_posix_env_init(env, tracer);

    host_name = "localhost"; // TODO: change to info.serverip
    dc_memset(env, &hints, 0, sizeof(hints));
    hints.ai_family =  PF_INET; // PF_INET6;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_CANONNAME;
    dc_getaddrinfo(env, err, host_name, NULL, &hints, &result);

    if(dc_error_has_no_error(err))
    {
        int socket_fd;

        socket_fd = dc_socket(env, err, result->ai_family, result->ai_socktype, result->ai_protocol);

        if(dc_error_has_no_error(err))
        {
            struct sockaddr *sockaddr;
            in_port_t port;
            in_port_t converted_port;
            socklen_t sockaddr_size;

            sockaddr = result->ai_addr;
            port = (in_port_t) info->serverport; // TODO: change it
            converted_port = htons(port);

            if(sockaddr->sa_family == AF_INET)
            {
                struct sockaddr_in *addr_in;

                addr_in = (struct sockaddr_in *)sockaddr;
                addr_in->sin_port = converted_port;
                sockaddr_size = sizeof(struct sockaddr_in);
            }
            else
            {
                if(sockaddr->sa_family == AF_INET6)
                {
                    struct sockaddr_in6 *addr_in;

                    addr_in = (struct sockaddr_in6 *)sockaddr;
                    addr_in->sin6_port = converted_port;
                    sockaddr_size = sizeof(struct sockaddr_in6);
                }
                else
                {
                    DC_ERROR_RAISE_USER(&err, "sockaddr->sa_family is invalid", -1);
                    sockaddr_size = 0;
                }
            }

            if(dc_error_has_no_error(err))
            {
                dc_connect(env, err, socket_fd, sockaddr, sockaddr_size);

                printf("connection made\n");

            //    send_start(env, err, socket_fd);

//                if(dc_error_has_no_error(err))
//                {
//                    struct sigaction old_action;
//
//                    dc_sigaction(env, err, SIGINT, NULL, &old_action);
//
//                    if(old_action.sa_handler != SIG_IGN)
//                    {
//                        struct sigaction new_action;
//                        char data[1024];
//
//                        exit_flag = 0;
//                        new_action.sa_handler = quit_handler;
//                        sigemptyset(&new_action.sa_mask);
//                        new_action.sa_flags = 0;
//                        dc_sigaction(env, err, SIGINT, &new_action, NULL);
//                        strcpy(data, "GET / HTTP/1.0\r\n\r\n");
//
//                        if(dc_error_has_no_error(err))
//                        {
//                            printf("about to write\n");
//                            dc_write(env, err, socket_fd, data, strlen(data));
//
//                        //    client_udp();
//                            printf("ended client udp in while loop\n");
//
//                            printf("sending end packet\n");
//                            dc_write(env, err, socket_fd, data, strlen(data));
//
////                            while(dc_read(&env, &err, socket_fd, data, 1024) > 0 && dc_error_has_no_error(&err))
////                            {
////                                printf("READ %s\n", data);
////                            }
//
//                            if(err->type == DC_ERROR_ERRNO && err->errno_code == EINTR)
//                            {
//                                dc_error_reset(err);
//                            }
//                        }
//                    }
//                }
            }
        }

        send_start(env, err, socket_fd, info);
      //  return SEND_START_DATA;

        //send_udp(env, err, socket_fd, info);
        printf("ended client udp\n");

        //send_end(env, err, socket_fd, info);

        //close_tcp_connection(env, err, socket_fd, info);

//        if(dc_error_has_no_error(err))
//        {
//            printf("closing socket\n");
//            dc_close(env, err, socket_fd);
//        }
    }

    //return SEND_START_DATA;

   return EXIT_SUCCESS;
}

static void quit_handler(int sig_num)
{
    exit_flag = 1;
}

//static void error_reporter(const struct dc_error *err)
//{
//    fprintf(stderr, "Error: \"%s\" - %s : %s : %d @ %zu\n", err->message, err->file_name, err->function_name, err->errno_code, err->line_number);
//}
//
//static void trace_reporter(const struct dc_posix_env *env, const char *file_name,
//                           const char *function_name, size_t line_number)
//{
//    fprintf(stderr, "Entering: %s : %s @ %zu\n", file_name, function_name, line_number);
//}


static int send_start(const struct dc_posix_env *env, struct dc_error *err, int socket_fd, void *arg)
{
    struct info *info;
    info = arg;

    if(dc_error_has_no_error(err))
    {
        struct sigaction old_action;

        dc_sigaction(env, err, SIGINT, NULL, &old_action);

        if(old_action.sa_handler != SIG_IGN)
        {
            struct sigaction new_action;
            char data[1024];

            exit_flag = 0;
            new_action.sa_handler = quit_handler;
            sigemptyset(&new_action.sa_mask);
            new_action.sa_flags = 0;
            dc_sigaction(env, err, SIGINT, &new_action, NULL);
            strcpy(data, "GET / HTTP/1.0\r\n\r\n");

            if(dc_error_has_no_error(err))
            {
                printf("about to write\n");
                dc_write(env, err, socket_fd, data, strlen(data));

                //    client_udp();
                printf("ended client udp in while loop\n");

//                printf("sending end packet\n");
//                dc_write(env, err, socket_fd, data, strlen(data));

//                            while(dc_read(&env, &err, socket_fd, data, 1024) > 0 && dc_error_has_no_error(&err))
//                            {
//                                printf("READ %s\n", data);
//                            }

                if(err->type == DC_ERROR_ERRNO && err->errno_code == EINTR)
                {
                    dc_error_reset(err);
                }
            }
        }
    }

    send_udp(env, err, socket_fd, info);

    return SEND_DATA;
    //return EXIT_SUCCESS;
}


static int send_end(const struct dc_posix_env *env, struct dc_error *err, int socket_fd, void *arg)
{
    struct info *info;
    info = arg;

    if(dc_error_has_no_error(err))
    {
        struct sigaction old_action;

        dc_sigaction(env, err, SIGINT, NULL, &old_action);

        if(old_action.sa_handler != SIG_IGN)
        {
            struct sigaction new_action;
            char data[1024];

            exit_flag = 0;
            new_action.sa_handler = quit_handler;
            sigemptyset(&new_action.sa_mask);
            new_action.sa_flags = 0;
            dc_sigaction(env, err, SIGINT, &new_action, NULL);
            strcpy(data, "GET / HTTP/1.0\r\n\r\n");

            if(dc_error_has_no_error(err))
            {
                //    client_udp();
                printf("ended client udp in while loop\n");

                printf("sending end packet\n");
                dc_write(env, err, socket_fd, data, strlen(data));

                if(err->type == DC_ERROR_ERRNO && err->errno_code == EINTR)
                {
                    dc_error_reset(err);
                }
            }
        }
    }

    close_tcp_connection(env, err, socket_fd, info);

    return CLOSE_CONNECTION;
    //return EXIT_SUCCESS;
}


static int close_tcp_connection(const struct dc_posix_env *env, struct dc_error *err, int socket_fd, void *arg)
{
    if(dc_error_has_no_error(err))
    {
        printf("closing socket\n");
        dc_close(env, err, socket_fd);
    }
    return EXIT_SUCCESS;
}



// =====================

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <unistd.h>

struct packet
{
    uint16_t id; // id of the packet (eg. 0-99 above)
    uint16_t size; // size of the data in bytes
    uint8_t *data; // the data
};

static int send_udp(const struct dc_posix_env *env, struct dc_error *err, int socket_fd, void *arg)
{
    struct info *info;
    info = arg;

    int socket_desc;
    struct sockaddr_in server_addr;
    char server_message[2000], client_message[2000];
    int server_struct_length = sizeof(server_addr);




    // Clean buffers:
    memset(server_message, '\0', sizeof(server_message));
    memset(client_message, '\0', sizeof(client_message));

    // Create socket:
    socket_desc = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if(socket_desc < 0){
        printf("Error while creating socket\n");
        return -1;
    }
    printf("Socket created successfully\n");

    // Set port and IP:
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((uint16_t) info->serverport);  // TODO: change port
    //server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_addr.s_addr = inet_addr("10.0.0.108");  //10.0.0.108

    char *test_msg = malloc(sizeof(int));

    // TODO: change 100 to info.packets
    for (int i = 1; i <= info->packets; i++)
    {
        struct packet *my_packet;
        my_packet = malloc(sizeof (struct packet));
        my_packet->size = (uint16_t) info->size; // TODO: change it to info.size
        my_packet->id = (uint16_t) i;

        // Get input from the user:
        printf("Enter message %d: ", i);
       //  gets(client_message);
      //  fgets(client_message,5,stdin);

        //char *seq_id = ;
        sprintf(test_msg, "%d", i);

        // Send the message to server:
        if(sendto(socket_desc, my_packet, sizeof (my_packet), 0,
                  (struct sockaddr*)&server_addr, server_struct_length) < 0){
            printf("Unable to send message\n");
            return -1;
        }

        sleep((unsigned int) (info->delay / 1000)); // TODO: change it to info.delay, divide by 1000 as delay is in ms
        //dc_usleep(1000);

        // Receive the server's response:
        if(recvfrom(socket_desc, server_message, sizeof(server_message), 0,
                    (struct sockaddr*)&server_addr, &server_struct_length) < 0){
            printf("Error while receiving server's msg\n");
            return -1;
        }

        printf("Server's response: %s\n", server_message);

    }


    printf("close sending packets\n");
    // Close the socket:
    close(socket_desc);

    free(test_msg);


    send_end(env, err, socket_fd, info);


    return SEND_END_DATA;
}


