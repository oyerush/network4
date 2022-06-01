#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define PORT 80
#define NUM_OF_SERVERS 3

int servers_fds[3] = {-1, -1, -1};

int servers_connection()
{
    const char *address[] =
        {
            "192.168.0.101",
            "192.168.0.102",
            "192.168.0.103"};
    for (int i = 0; i < NUM_OF_SERVERS; i++)
    {
        int sock = 0, valread, client_fd;
        struct sockaddr_in serv_addr;
        char *hello = "Hello from client";
        char buffer[1024] = {0};
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            printf("\n Socket creation error \n");
            return -1;
        }

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(PORT);

        // Convert IPv4 and IPv6 addresses from text to binary
        // form
        if (inet_pton(AF_INET, address[i], &serv_addr.sin_addr) <= 0)
        {
            printf(
                "\nInvalid address/ Address not supported \n");
            return -1;
        }

        if ((connect(sock, (struct sockaddr *)&serv_addr,
                                      sizeof(serv_addr))) < 0)
        {
            printf("\nConnection Failed \n");
            return -1;
        }
        servers_fds[i] = sock;
    }
    return 0;
}

int clients_connection(int *fd, struct sockaddr_in *fd_address)
{
    int server_fd, new_socket, valread;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        printf("socket failed");
        return -1;
    }

    // Forcefully attaching socket to the port 8080
    if (setsockopt(server_fd, SOL_SOCKET,
                   SO_REUSEADDR | SO_REUSEPORT, &opt,
                   sizeof(opt)))
    {
        printf("setsockopt");
        return -1;
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Forcefully attaching socket to the port 8080
    if (bind(server_fd, (struct sockaddr *)&address,
             sizeof(address)) < 0)
    {
        printf("bind failed");
        return -1;
    }
    if (listen(server_fd, 5) < 0)
    {
        printf("listen");
        return -1;
    }
    *fd = server_fd;
    *fd_address = address;
    return 0;
}

int server_to_client[3] = {-1, -1, -1};

int scheduler(char *buffer)
{

    int cur_time = time(NULL);
    int server1_ttr = MAX(server_to_client[0] - cur_time, 1);
    int server2_ttr = MAX(server_to_client[1] - cur_time, 1);
    int server3_ttr = MAX(server_to_client[2] - cur_time, 1);
    int server12_ttr = MIN(server1_ttr, server2_ttr);
    int server12 = (MIN(server1_ttr, server2_ttr) == server2_ttr);
    switch (buffer[0])
    {
    case 'M':
    {
        // server3 is available
        if (server_to_client[2] == -1)
        {
            return 2;
        }
        // check if server3 time is less then the time to finish job
        if (server3_ttr < buffer[1] - '0')
        {
            // wait until server3 finish
            return 2;
        }
        if (server_to_client[0] == -1)
        {
            return 0;
        }
        if (server_to_client[1] == -1)
        {
            return 1;
        }
        if (server3_ttr < (buffer[1] - '0') + server12_ttr)
        {
            // wait until server1/2 finish
            return 2;
        }
        return -MIN(server12_ttr, server3_ttr);
    }
    case 'V':
    {
        // server1 is available
        if (server_to_client[0] == -1)
        {
            return 0;
        }
        // server2 is available
        if (server_to_client[1] == -1)
        {
            return 1;
        }
        // check if server1/2 time is less then the time to finish job
        if (server12_ttr < 2 * (buffer[1] - '0'))
        {
            // wait until server1/2 finish
            printf("server12 %d\n", server12);
            return server12;
        }
        if (server_to_client[2] == -1)
        {
            return 2;
        }
        if (server12_ttr < (buffer[1] - '0') + server3_ttr)
        {
            // wait until server1/2 finish
            return server12;
        }
        return -MIN(server12_ttr, server3_ttr);
    }
    case 'P':
    {
        // server1 is available
        if (server_to_client[0] == -1)
        {
            return 0;
        }
        // server2 is available
        if (server_to_client[1] == -1)
        {
            return 1;
        }
        // check if server1/2 time is less then the time to finish job
        if (server12_ttr < (buffer[1] - '0'))
        {
            // wait until server1/2 finish
            return server12;
        }
        if (server_to_client[2] == -1)
        {
            return 2;
        }
        if (server12_ttr < (buffer[1] - '0') + server3_ttr)
        {
            // wait until server1/2 finish
            return server12;
        }
        return -MIN(server12_ttr, server3_ttr);
    }
        //        default:
        //            printf("req error");
        //            return -1;
    }
    printf("req error");
    return -1;
}


pthread_mutex_t lock;

void *client_handler(void *fd)
{
    char buffer[1024] = {0};
    int bytes_read = read(*((int *)fd), buffer, 1024);
    pthread_mutex_lock(&lock);
    int server_num;
    while ((server_num = scheduler(buffer)) < 0)
    {
        pthread_mutex_unlock(&lock);
        sleep(-server_num);
        pthread_mutex_lock(&lock);
    }
    if (server_to_client[server_num] == -1)
    {
        server_to_client[server_num] = time(NULL) + buffer[1] - '0';
    }
    else
    {
        server_to_client[server_num] += time(NULL) + buffer[1] - '0';
    }
    
    pthread_mutex_unlock(&lock);
    write(servers_fds[server_num], buffer, bytes_read);
    bytes_read = read(servers_fds[server_num], buffer, 1024);
    write(*((int *)fd), buffer, bytes_read);    
    pthread_mutex_lock(&lock);
    if (time(NULL) >= server_to_client[server_num])
        server_to_client[server_num] = -1;
    pthread_mutex_unlock(&lock);
    close(*(int *)fd);
    return fd;
}

int lb(int lb_fd, struct sockaddr_in fd_address)
{

    int addrlen = sizeof(fd_address);
    pthread_t threads[100];
    int i = 0;
    int *client_new_soc = (int *)malloc(100 * sizeof(int));
    int **ptr_client =  (int **)malloc(sizeof(int *));
    *ptr_client = client_new_soc;
    while (1)
    {
        struct sockaddr_in tmp_address = fd_address;
        int tmp_addrlen = addrlen;
        if ((client_new_soc[i] = accept(lb_fd, (struct sockaddr *)&tmp_address, (socklen_t *)&tmp_addrlen)) < 0)
        // if ((client_new_soc[i] = accept(lb_fd, (struct sockaddr *)ptr_fd_addr,
        //                             (socklen_t *)addrlen)) < 0)
        {
            // if there is no client try to connect
            printf("con err\n");
            continue;
        }
        pthread_create(threads + i, NULL, client_handler, (void *)(&client_new_soc[i]));
        i = (i + 1) % 100;
    }
}

int main()
{

    int *lb_fd = (int *)malloc(sizeof(int));
    struct sockaddr_in *fd_address = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
    // set up the lb
    // set up connection with servers
    if (servers_connection() < 0)
    {
        return -1;
    }
    // set up connection with clients
    if (clients_connection(lb_fd, fd_address) < 0)
    {
        return -1;
    }
    lb(*lb_fd, *fd_address);
}