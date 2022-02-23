
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


struct application_settings
{
    struct dc_opt_settings opts;
    struct dc_setting_string *port;
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

int init(const struct dc_posix_env *env, struct dc_error *err);
int run_server(void);

struct server_info
{
    int port;
};

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
    settings->port = dc_setting_string_create(env, err);

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
            {(struct dc_setting *)settings->port,
                    dc_options_set_string,
                    "port",
                    required_argument,
                    'p',
                    "PORT",
                    dc_string_from_string,
                    "port",
                    dc_string_from_config,
                    "4981"},
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
    dc_setting_string_destroy(env, &app_settings->port);
    dc_free(env, app_settings->opts.opts, app_settings->opts.opts_count);
    dc_free(env, *psettings, sizeof(struct application_settings));

    if(env->null_free)
    {
        *psettings = NULL;
    }

    return 0;
}

static int run(const struct dc_posix_env *env, struct dc_error *err, struct dc_application_settings *settings)
{
    struct application_settings *app_settings;
    const char *message;
    int ret_val;

    struct server_info info;

    DC_TRACE(env);

    app_settings = (struct application_settings *)settings;
    message = dc_setting_string_get(env, app_settings->port);
    printf("server port %s\n", message);

    int port = atoi(message);
    printf("port = %d\n", port);
    info.port = port;

    init(env, err);

    return EXIT_SUCCESS;
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


#include <dc_posix/dc_netdb.h>
#include <dc_posix/dc_posix_env.h>
#include <dc_posix/dc_unistd.h>
#include <dc_posix/dc_signal.h>
#include <dc_posix/dc_string.h>
#include <dc_posix/dc_stdlib.h>
#include <dc_posix/sys/dc_socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


static void error_reporter(const struct dc_error *err);
static void trace_reporter(const struct dc_posix_env *env, const char *file_name,
                           const char *function_name, size_t line_number);
static void quit_handler(int sig_num);
void receive_data(struct dc_posix_env *env, struct dc_error *err, int fd, size_t size);

static int send_udp(struct dc_posix_env *env, struct dc_error *err);


static volatile sig_atomic_t exit_flag;


#include <dc_posix/dc_pthread.h>
int init(const struct dc_posix_env *env, struct dc_error *err)
{
    pthread_t tcp_thread;
    pthread_t udp_thread;

    dc_pthread_create(env, err, &tcp_thread, NULL, (void *(*)(void *)) run_server, NULL);

  //  dc_pthread_create(env, err, &udp_thread, NULL, (void *(*)(void *)) send_udp, (void *)&tcp_thread);

 //   send_udp(&env, &err);
    if(dc_error_has_no_error(err))
    {
        dc_pthread_join(env, err, tcp_thread, NULL);
    //    dc_pthread_join(env, err, udp_thread, NULL);

    }

    return EXIT_SUCCESS;
}

int run_server(void)
{
    dc_error_reporter reporter;
    dc_posix_tracer tracer;
    struct dc_error err;
    struct dc_posix_env env;
    const char *host_name;
    struct addrinfo hints;
    struct addrinfo *result;

    reporter = error_reporter;
    tracer = trace_reporter;
    tracer = NULL;
    dc_error_init(&err, reporter);
    dc_posix_env_init(&env, tracer);

    host_name = "localhost";
    dc_memset(&env, &hints, 0, sizeof(hints));
    hints.ai_family =  PF_INET; // PF_INET6;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_CANONNAME;
    dc_getaddrinfo(&env, &err, host_name, NULL, &hints, &result);

    if(dc_error_has_no_error(&err))
    {
        int server_socket_fd;

        server_socket_fd = dc_socket(&env, &err, result->ai_family, result->ai_socktype, result->ai_protocol);

        if(dc_error_has_no_error(&err))
        {
            struct sockaddr *sockaddr;
            in_port_t port;
            in_port_t converted_port;
            socklen_t sockaddr_size;

            sockaddr = result->ai_addr;
            port = 2002; // TODO: change port to passed parameter
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

            if(dc_error_has_no_error(&err))
            {
                dc_bind(&env, &err, server_socket_fd, sockaddr, sockaddr_size);

                if(dc_error_has_no_error(&err))
                {
                    int backlog;

                    backlog = 5;
                    dc_listen(&env, &err, server_socket_fd, backlog);

                    printf("server listening\n");
                    if(dc_error_has_no_error(&err))
                    {
                        struct sigaction old_action;

                        dc_sigaction(&env, &err, SIGINT, NULL, &old_action);

                        if(old_action.sa_handler != SIG_IGN)
                        {
                            struct sigaction new_action;

                            exit_flag = 0;
                            new_action.sa_handler = quit_handler;
                            sigemptyset(&new_action.sa_mask);
                            new_action.sa_flags = 0;
                            dc_sigaction(&env, &err, SIGINT, &new_action, NULL);

                            while(!(exit_flag) && dc_error_has_no_error(&err))
                            {
                                int client_socket_fd;

                                client_socket_fd = dc_accept(&env, &err, server_socket_fd, NULL, NULL);

                                if(dc_error_has_no_error(&err))
                                {
                                    receive_data(&env, &err, client_socket_fd, 1024);
                                    printf("out of read data\n");
                                    send_udp(&env, &err); // working here

                                    dc_close(&env, &err, client_socket_fd);
                                }
                                else
                                {
                                    if(err.type == DC_ERROR_ERRNO && err.errno_code == EINTR)
                                    {
                                        dc_error_reset(&err);
                                    }
                                }

                                //send_udp();
                            }
                        }
                    }
                }
                //send_udp();
            }
        }

        //send_udp();

        if(dc_error_has_no_error(&err))
        {
            dc_close(&env, &err, server_socket_fd);
        }
    }

    return EXIT_SUCCESS;
}

// Look at the code in the client, you could do the same thing
void receive_data(struct dc_posix_env *env, struct dc_error *err, int fd, size_t size)
{
    // more efficient would be to allocate the buffer in the caller (main) so we don't have to keep
    // mallocing and freeing the same data over and over again.
    char *data;
    ssize_t count;

    printf("in receive data\n");
    data = dc_malloc(env, err, size);

//    while(!(exit_flag) && (count = dc_read(env, err, fd, data, size)) > 0 && dc_error_has_no_error(err))
//    {
//        dc_write(env, err, STDOUT_FILENO, data, (size_t)count);
//    }

    count = dc_read(env, err, fd, data, size);
    dc_write(env, err, STDOUT_FILENO, data, (size_t)count);

    dc_free(env, data, size);
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




// =======

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>

static void calculate_statistics(int seq_id_arr[], int sent);

struct packet
{
    uint16_t id; // id of the packet (eg. 0-99 above)
    uint16_t size; // size of the data in bytes
    uint8_t *data; // the data
};

static int send_udp(struct dc_posix_env *env, struct dc_error *err)
{
    int socket_desc;
    struct sockaddr_in server_addr, client_addr;
    char server_message[2000], client_message[2000];
    int client_struct_length = sizeof(client_addr);

    int packets = 20; // TODO: get it from initial client message

    printf("in server udp\n");

    // Clean buffers:
    memset(server_message, '\0', sizeof(server_message));
    memset(client_message, '\0', sizeof(client_message));

    // Create UDP socket:
    socket_desc = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if(socket_desc < 0){
        printf("Error while creating socket\n");
        return -1;
    }
    printf("Socket created successfully\n");

    // Set port and IP:
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(2002);  //TODO: change port
  //  server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");  //10.0.0.108
    server_addr.sin_addr.s_addr = inet_addr("10.0.0.108");  //10.0.0.108

    // Bind to the set port and IP:
    if(bind(socket_desc, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        printf("Couldn't bind to the port\n");
        return -1;
    }
    printf("Done with binding\n");

    printf("Listening for incoming messages...\n\n");

    struct packet *my_pac_arr[packets];
    int arr_id[packets];

    int cnt = 0;
    while(!(exit_flag) && dc_error_has_no_error(err))
    {
        struct packet *my_packet;
        my_packet = malloc(sizeof (struct packet));

        // Receive client's message:
        if (recvfrom(socket_desc, my_packet, sizeof(my_packet), 0,
                     (struct sockaddr*)&client_addr, &client_struct_length) < 0){
            printf("Couldn't receive\n");
            return -1;
        }
        printf("Received message from IP: %s and port: %i\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        printf("Packet id: %hu\n", my_packet->id);
        my_pac_arr[cnt] = my_packet;
        arr_id[cnt] = my_packet->id;

        // Respond to client:
        strcpy(server_message, client_message);

        if (sendto(socket_desc, server_message, strlen(server_message), 0,
                   (struct sockaddr*)&client_addr, client_struct_length) < 0){
            printf("Can't send\n");
            return -1;
        }

        cnt++;
        if(cnt == packets)
        {
            break;
        }

        free(my_packet);
    }

    printf("done receiving udp\n");

    for (int i = 0; i < packets; i++)
    {
        printf("arr_id[%d]: %d\n", i, arr_id[i]);
        printf("my_pac_arr[%d]: %d\n", i, my_pac_arr[i]->id);
    }


    // Close the socket:
    close(socket_desc);

    calculate_statistics(arr_id, packets);


    return 0;
}

static void calculate_statistics(int seq_id_arr[], int sent)
{
    printf("in func: %d\n", seq_id_arr[0]);
    printf("in func: %d\n", seq_id_arr[90]);

    int received = sent;
    int lost;
    int out_of_order = 0;

    for (int i = 0; i < sent; i++)
    {
        if (seq_id_arr[i] == 0)
        {
            received = i;
        }

        if (i > 0)
        {
            if (seq_id_arr[i] < seq_id_arr[i-1])
            {
                out_of_order++;
            }
        }

    }

    lost = sent - received;

    FILE *fptr;

    fptr = fopen("output.txt", "a");

    fprintf(fptr, "# of packets RECEIVED = %d\n", received);
    fprintf(fptr, "# of packets LOST = %d\n", lost);
    fprintf(fptr, "# of packets received OUT OF ORDER = %d\n", out_of_order);

    printf("# of packets RECEIVED = %d\n", received);
    printf("# of packets LOST = %d\n", lost);
    printf("# of packets received OUT OF ORDER = %d\n", out_of_order);

    fclose(fptr);


}


