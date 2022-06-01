#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>
#include <list>
#include <thread>
#include <mutex>
#include <algorithm>


#define PORT 80
#define NUM_OF_SERVERS 3


int servers_connection(int *fds)
{
    const char * address[] = 
    { 
        "192.168.0.101",
        "192.168.0.102",
        "192.168.0.103"
    };
    for (int i = 0; i < NUM_OF_SERVERS; i++)
    {
        int sock = 0, valread, client_fd;
        struct sockaddr_in serv_addr1;
        struct sockaddr_in *serv_addr = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
        *serv_addr = serv_addr1;
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            printf("\n Socket creation error \n");
            return -1;
        }
        serv_addr->sin_family = AF_INET;
        serv_addr->sin_port = htons(PORT);
        struct in_addr *sin_addr = (struct in_addr *)malloc(sizeof(struct in_addr));
        *sin_addr = serv_addr->sin_addr;
        if (inet_pton(AF_INET, address[i], sin_addr) <= 0)
        {
            printf("\nInvalid address/ Address not supported \n");
            return -1;
        }
        serv_addr->sin_addr = *sin_addr;
        if ((fds[i] = connect(sock, (struct sockaddr *)serv_addr, sizeof(*serv_addr))) < 0)
        {
            printf("\nConnection Failed \n");
            return -1;
        }
    }
    return 0;
}

int clients_connection(int &fd, struct sockaddr_in &fd_address)
{
    int server_fd, new_socket, valread;
    struct sockaddr_in address1;
    struct sockaddr_in *address = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
    *address = address1;
    int *opt = (int *)malloc(sizeof(int));
    *opt = 1;
    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        printf("socket failed");
        return -1;
    }
    if (setsockopt(server_fd, SOL_SOCKET,
                   SO_REUSEADDR | SO_REUSEPORT, opt,
                   sizeof(*opt)))
    {
        printf("setsockopt");
        return -1;
    }
    address->sin_family = AF_INET;
    address->sin_addr.s_addr = INADDR_ANY;
    address->sin_port = htons(PORT);
    if (bind(server_fd, (struct sockaddr *)address,
             sizeof(*address)) < 0)
    {
        printf("bind failed");
        return -1;
    }
    if (listen(server_fd, 5) < 0)
    {
        printf("listen");
        return -1;
    }
    fd = server_fd;
    fd_address = *address;
    printf("finish client");
    return 0;
}


int server_to_client[3][2] = {-1, -1};

int scheduler(char *buffer)
{
    // check if there is no available server
    if (server_to_client[0][0] != -1 &&
        server_to_client[1][0] != -1 &&
        server_to_client[2][0] != -1)
    {
        return -1;
    }
    int cur_time;
    int server1_ttr;
    int server2_ttr;
    int server3_ttr;
    int server12_ttr;
    switch(buffer[0])
    {
        case 'M':
        {
            // server3 is available
            if (server_to_client[2][0] == -1)
            {
                return 2;
            }
            server3_ttr = server_to_client[2][0] - (time(NULL) - server_to_client[2][1]);
            // check if server3 time is less then the time to finish job
            if (server3_ttr < buffer[1] - '0')
            {
                // wait until server3 finish
                return -server3_ttr;
            }
            if (server_to_client[0][0] == -1)
            {
                return 0;
            }
            return 1;
        }
        case 'V':
        {
            // server1 is available
            if (server_to_client[0][0] == -1)
            {
                return 0;
            }
            // server2 is available
            if (server_to_client[1][0] == -1)
            {
                return 1;
            }
            cur_time = time(NULL);
            server1_ttr = server_to_client[0][0] - (cur_time - server_to_client[0][1]);
            server2_ttr = server_to_client[1][0] - (cur_time - server_to_client[1][1]);
            server12_ttr = std::min(server1_ttr, server2_ttr);
            // check if server1/2 time is less then the time to finish job
            if (server12_ttr < 2 * (buffer[1] - '0'))
            {
                // wait until server1/2 finish
                return -server12_ttr;
            }
            return 3;
        }
        case 'P':
        {
            // server1 is available
            if (server_to_client[0][0] == -1)
            {
                return 0;
            }
            // server2 is available
            if (server_to_client[1][0] == -1)
            {
                return 1;
            }
            cur_time = time(NULL);
            cur_time = time(NULL);
            server1_ttr = server_to_client[0][0] - (cur_time - server_to_client[0][1]);
            server2_ttr = server_to_client[1][0] - (cur_time - server_to_client[1][1]);
            server12_ttr = std::min(server1_ttr, server2_ttr);
            // check if server1/2 time is less then the time to finish job
            if (server12_ttr < (buffer[1] - '0'))
            {
                // wait until server1/2 finish
                return -server12_ttr;
            }
            return 3;
        }
//        default:
//            printf("req error");
//            return -1;
    }
    printf("req error");
    return -1;
}

int servers_fds[3] = {-1, -1, -1};
std::mutex mtx_sched;

void *client_handler(void *fd)
{
    char buffer[1024] = {0};
    int bytes_read = read(*((int *)fd), buffer, 1024);
    mtx_sched.lock();
    int server_num;
    printf("buf: %s", buffer);
    while ((server_num = scheduler(buffer)) < 0)
    {
        printf("not %d", server_num);
        mtx_sched.unlock();
        sleep(-server_num);
        mtx_sched.lock();
    }
    printf("yes %d", server_num);
    server_to_client[server_num][0] = buffer[1] - '0';
    server_to_client[server_num][1] = time(NULL);
    mtx_sched.unlock();
    printf("w\n");
    write(servers_fds[server_num], buffer, bytes_read);
    printf("r\n");
    bytes_read = read(servers_fds[server_num], buffer, 1024);
    printf("f\n");
    mtx_sched.lock();
    printf("l\n");
    server_to_client[server_num][0] = buffer[1] - '0';
    server_to_client[server_num][1] = time(NULL);
    mtx_sched.unlock();
    printf("ul\n");
    write(*((int *)fd), buffer, bytes_read);
    return fd;
}

int lb(int lb_fd, struct sockaddr_in fd_address)
{

    int *addrlen = (int *)malloc(sizeof(int));
    *addrlen = sizeof(fd_address);
    struct sockaddr_in *ptr_fd_addr = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
    *ptr_fd_addr = fd_address;
    pthread_t threads[100];
    int i = 0;
    int client_new_soc[100];
    while (1)
    {
        printf("here\n");

        if ((client_new_soc[i] = accept(lb_fd, (struct sockaddr*)&fd_address,(socklen_t*)addrlen)) < 0)
        //if ((client_new_soc[i] = accept(lb_fd, (struct sockaddr *)ptr_fd_addr,
         //                            (socklen_t *)addrlen)) < 0)
        {
            // if there is no client try to connect
            printf("con err\n");
            continue;   
        }
        pthread_create(threads + i, NULL, client_handler, (void *)(client_new_soc + i));
        i = (i+1)%100;
    }
}

int main()
{

    int lb_fd;
    struct sockaddr_in fd_address;
    // set up the lb 
    // set up connection with servers
    if (servers_connection((int *)servers_fds) < 0)
    {
        return -1;
    }
    // set up connection with clients
    if (clients_connection(lb_fd, fd_address) < 0)
    {
        return -1;
    }
    lb(lb_fd, fd_address);
}