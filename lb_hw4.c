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
#define PORT 80
#define NUM_OF_SERVERS 3

int servers_connection(int *fds)
{
    const char *address[] =
        {
            "192.168.0.101",
            "192.168.0.102",
            "192.168.0.103"};
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
    switch (buffer[0])
    {
    case 'M':
    {
        printf("\nM - %d -\n", server_to_client[2][0]);
        // server3 is available
        if (server_to_client[2][0] == -1)
        {
            printf("\n-M-2-\n");
            return 2;
        }
        printf("\n-M-A-\n");
        server3_ttr = server_to_client[2][0] - (time(NULL) - server_to_client[2][1]);
        printf("\n-M-B-\n");
        // check if server3 time is less then the time to finish job
        if (server3_ttr < buffer[1] - '0')
        {
            printf("\n-M-C %d-\n", server3_ttr);
            // wait until server3 finish
            return -server3_ttr;
        }
        printf("\n-M-D-\n");
        if (server_to_client[0][0] == -1)
        {
            printf("\n-M-0-\n");
            return 0;
        }
        printf("\n-M-1-\n");
        return 1;
    }
    case 'V':
    {
        printf("\nV\n");
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
        server12_ttr = MIN(server1_ttr, server2_ttr);
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
        printf("\nP\n");
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
        server12_ttr = MIN(server1_ttr, server2_ttr);
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
pthread_mutex_t lock;

void *client_handler(void *fd)
{
    char buffer[1024] = {0};
    int bytes_read = read(*((int *)fd), buffer, 1024);
    pthread_mutex_lock(&lock);
    int server_num;
    printf("buf: %s", buffer);
    while ((server_num = scheduler(buffer)) < 0)
    {
        printf("not %d", server_num);
        pthread_mutex_unlock(&lock);
        sleep(-server_num);
        pthread_mutex_lock(&lock);
    }
    printf("yes %d", server_num);
    server_to_client[server_num][0] = buffer[1] - '0';
    server_to_client[server_num][1] = time(NULL);
    pthread_mutex_unlock(&lock);
    printf("w\n");
    write(servers_fds[server_num], buffer, bytes_read);
    printf("r\n");
    bytes_read = read(servers_fds[server_num], buffer, 1024);
    printf("f\n");
    pthread_mutex_lock(&lock);
    printf("l\n");
    server_to_client[server_num][0] = buffer[1] - '0';
    server_to_client[server_num][1] = time(NULL);
    pthread_mutex_unlock(&lock);
    printf("ul\n");
    write(*((int *)fd), buffer, bytes_read);    
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
        printf("here\n");
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
    if (servers_connection((int *)servers_fds) < 0)
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