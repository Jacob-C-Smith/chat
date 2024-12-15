/** !
 * Chat server
 *
 * @file server.c
 * 
 * @author Jacob Smith
 */

// Standard library
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// socket
#include <socket/socket.h>

// queue
#include <queue/queue.h>

// parallel
#include <parallel/parallel.h>
#include <parallel/thread_pool.h>

// Preprocessor definitions
#define MAX_BUFFER_LEN 1024

// Structure definitions
struct client_s
{
    char        _name[63 + 1];
    char        _channel[63 + 1];
    char        _in_buf[MAX_BUFFER_LEN];
    bool        connected;
    queue      *p_queue;
    socket_tcp  _tcp_socket;
};

// Type definitions
typedef struct client_s client;

// Static data
static struct
{
    socket_tcp _tcp_socket;
    thread_pool *p_thread_pool;
    mutex        _lock;
    
    struct
    {
        unsigned short port;
        int max_users;
        char _name[128];
    } data;

    struct 
    {
        int connections;
    } state;

    client _users [8];
} _chat_server;

/** !
 * Initialize the chat server
 * 
 * @param void
 * 
 * @return void
 */
void chat_server_init ( void ) __attribute__((constructor));

/** !
 * Parse command line arguments
 * 
 * @param argc the argc parameter of the entry point
 * @param argv the argv parameter of the entry point
 * 
 * @return void on success, program abort on failure
 */
void parse_command_line_arguments ( int argc, const char *argv[] );

/** !
 * Client thread
 * 
 * @param p_client the client
 * 
 * @return 1 on success, 0 on error
 */
int server_client_thread ( client *p_client );

/** !
 * 
 * 
 * 
 */
int someone_connected ( socket_tcp _tcp_socket, unsigned long ip_address, unsigned short port, void *p_chat_server )
{

    // Uninitialized data
    char buffer[MAX_BUFFER_LEN];

    // Initialized data
    unsigned char a = (ip_address & 0xff000000) >> 24,
                  b = (ip_address & 0x00ff0000) >> 16,
                  c = (ip_address & 0x0000ff00) >> 8,
                  d = (ip_address & 0x000000ff) >> 0;
    client *p_client = (void *) 0;
    
    // Store a pointer to the next user
    p_client = &_chat_server._users[_chat_server.state.connections];

    // Populate the user 
    *p_client = (client)
    {
        ._channel = "general",
        ._in_buf = { 0 },
        .connected = true,
        ._tcp_socket = _tcp_socket
    };

    // Name the client
    snprintf(p_client->_name, sizeof(p_client->_name), "anon%d", rand() % 10000);

    if ( _chat_server.state.connections == _chat_server.data.max_users )
    {

        // Prompt
        socket_tcp_send(p_client->_tcp_socket, "\033[41m\033[[[[[SERVER FULL]]]\033[0m\n", 31);
        
        // Shutdown
        socket_tcp_destroy(&_tcp_socket);

        // Success
        return 1;
    };

    // Increment the quantity of connections
    _chat_server.state.connections++;

    // Log the IP
    printf("\r\033[44m\033[[[[[%d.%d.%d.%d:%d CONNECTED as %s]]]\033[0m\n", a, b, c, d, port, p_client->_name);

    // Start a new thread
    thread_pool_execute(_chat_server.p_thread_pool, server_client_thread, p_client);

    // Success
    return 1;
}

// Entry point
int main ( int argc, const char *argv[] )
{

    // Initialized data
    bool running = true;

    // Parse command line arguments
    parse_command_line_arguments(argc, argv);

    // Construct a thread pool
    if ( thread_pool_construct(&_chat_server.p_thread_pool, _chat_server.data.max_users ) == 0 ) goto failed_to_create_thread_pool;

    // Construct a socket
    if  ( socket_tcp_create(&_chat_server._tcp_socket, socket_address_family_ipv4, _chat_server.data.port) == 0 ) goto failed_to_create_socket;
    // 
    while ( running )
        socket_tcp_listen(_chat_server._tcp_socket, someone_connected, &_chat_server);

    // NOTE: No longer accepting connections

    // Wait for the thread pool to idle (everyone disconnects)
    thread_pool_wait_idle(_chat_server.p_thread_pool);

    // Success
    return EXIT_SUCCESS;

    // Error handling
    {

        // Parallel errors
        {
            failed_to_create_thread_pool:
                #ifndef NDEBUG
                    log_error("Error: Failed to create thread pool!\n");
                #endif

                // Error
                return 0;
        }

        // Socket errors
        {
            failed_to_create_socket:
                #ifndef NDEBUG
                    log_error("Error: Failed to create socket!\n");
                #endif

                // Error
                return 0;
        }
    }
}

void chat_server_init ( void )
{

    // Costruct a lock
    mutex_create(&_chat_server._lock);

    // Set the name of the chat server
    strncpy(_chat_server.data._name, "chat", 4);

    // Store the port
    _chat_server.data.port = 3000;

    // Store the default user quantity
    _chat_server.data.max_users = 8;

    // Seed the random number generator
    srand(time(NULL));

    // Done
    return;
}

void parse_command_line_arguments ( int argc, const char *argv[] )
{

    // Argument check

    for (size_t i = 1; i < argc; i++)
    {

        size_t len = strlen(argv[i]);

        if ( argv[i][0] != '-' ) continue;

        for (size_t j = 1; j < len; j++)
        {
            switch(argv[i][j])
            {
                case 'p':
                {

                    size_t len = strlen(argv[++i]);
                    
                    // Parse the port
                    _chat_server.data.port = strtol(argv[i], 0, 10);

                    // Print the port number
                    log_info("[chat room] Using port %d\n", _chat_server.data.port);

                    // Done
                    break;
                }

                case 'c':
                {
                    
                    size_t len = strlen(argv[++i]);

                    // Parse the port
                    _chat_server.data.max_users = strtol(argv[i], 0, 10);

                    // Print the port number
                    log_info("[chat room] Maximum users %d\n", _chat_server.data.max_users);

                    // Done
                    break;
                }

                case 'n':
                {
                    
                    size_t len = strlen(argv[++i]);

                    // Copy the name
                    strncpy(_chat_server.data._name, argv[i], len);
                    _chat_server.data._name[len] = '\0';

                    // Print the port number
                    log_info("[chat room] Name %s\n", _chat_server.data._name);

                    // Done
                    break;
                }
            }
        }
    }
}

int server_client_thread ( client *p_client )
{

    // Initialized data
    char _prompt[128] = { 0 };
    char _message_queue[4096] = { 0 };
    int message_queue_len = 0;
    int status = 1;

    queue_construct(&p_client->p_queue);

    while ( p_client->connected )
    {
        if ( status < 0 )
        {

            // Print the disconnect
            printf("\r\033[44m\033[[[[[%s DISCONNECTED]]]\033[0m\n", p_client->_name);

            // Done
            break;
        }

        // Clear the input buffer
        memset(&p_client->_in_buf, 0, sizeof(p_client->_in_buf));

        // Clear the prompt
        memset(&_prompt, 0, sizeof(_prompt));

        message_queue_len = 0;

        while ( queue_empty(p_client->p_queue) == false )
        {
            char *p_sender = (void *) 0;

            queue_dequeue(p_client->p_queue, &p_sender);
            message_queue_len += snprintf(&_message_queue[message_queue_len], sizeof(_message_queue) - message_queue_len, "%s\n", p_sender);
        }

        // Build the prompt
        message_queue_len += snprintf(&_prompt[message_queue_len], sizeof(_prompt), "%s\033[01;32m%s@%s \033[01;34m%s\033[0m > \033[0m", _message_queue, p_client->_name, _chat_server.data._name, p_client->_channel);

        // Prompt
        socket_tcp_send(p_client->_tcp_socket, _prompt, message_queue_len);
        
        // Receive data from the socket
        status = socket_tcp_receive(p_client->_tcp_socket, p_client->_in_buf, sizeof(p_client->_in_buf));
        
        // Evaluate input
        if ( *p_client->_in_buf == '/' )
        {
            if ( strncmp(p_client->_in_buf, "/disconnect", 11) == 0 ) status = -1;
            if ( strncmp(p_client->_in_buf, "/join ", 6) == 0 ) {
                memset(p_client->_channel, 0, sizeof(p_client->_channel));

                memcpy(p_client->_channel, &p_client->_in_buf[6], strlen(&p_client->_in_buf[6]) - 1);

                // Log 
                printf("\r\033[44m\033[[[[[%s JOINED %s]]]\033[0m\n", p_client->_name, p_client->_channel);
            }
            if ( strncmp(p_client->_in_buf, "/nick ", 6) == 0 ) {
                
                p_client->_in_buf[strlen(p_client->_in_buf) - 1] = '\0';
                
                printf("\r\033[44m\033[[[[[%s RENAMED %*s]]]\033[0m\n", p_client->_name, (strlen(&p_client->_in_buf[6])), &p_client->_in_buf[6]);
                
                memset(p_client->_name, 0, sizeof(p_client->_name));
                
                memcpy(p_client->_name, &p_client->_in_buf[6], strlen(&p_client->_in_buf[6]));
            }
        }

        else
        {
            for (size_t i = 0; i < _chat_server.state.connections; i++)
                queue_enqueue(_chat_server._users[i].p_queue, p_client->_name);            
        }
    }
    
    if ( status == -1 )
    {

        socket_tcp_send(p_client->_tcp_socket, "BYE\n", strlen("BYE\n"));
        
        socket_tcp_destroy(&p_client->_tcp_socket);
    }
    
    _chat_server.state.connections--;

    // Success
    return 1;
}
