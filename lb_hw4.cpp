#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>
#include <list>
#define PORT 80
#define NUM_OF_SERVERS 3


int servers_connection(int **fds)
{
    char * address[] = 
    { 
        "192.168.0.101",
        "192.168.0.102",
        "192.168.0.103"
    };
    for (int i = 0; i < NUM_OF_SERVERS; i++)
    {
        int sock = 0, valread, client_fd;
        struct sockaddr_in serv_addr;
        if ((sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0)
        {
            printf("\n Socket creation error \n");
            return -1;
        }
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(PORT);
        if (inet_pton(AF_INET, address[i], &serv_addr.sin_addr) <= 0)
        {
            printf("\nInvalid address/ Address not supported \n");
            return -1;
        }
        if (((*fds)[i] = connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) < 0)
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
    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) == 0)
    {
        printf("socket failed");
        return -1;
    }
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

struct task
{
    int client_fd;
    char buf[1024];
};


std::list<struct task> tasks_queue;
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
    switch(buffer[0])
    {
        case 'M':
        {
            // server3 is available
            if (server_to_client[2][0] == -1)
            {
                return 2;
            }
            // check if server3 time is less then the time to finish job
            if (time(NULL) - server_to_client[2][1] < buffer[1] - '0')
            {
                // wait until server3 finish
                return -1;
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
            // check if server1/2 time is less then the time to finish job
            if (cur_time - server_to_client[0][1] < 2 * (buffer[1] - '0') ||
                cur_time - server_to_client[1][1] < 2 * (buffer[1] - '0'))
            {
                // wait until server1/2 finish
                return -1;
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
            // check if server1/2 time is less then the time to finish job
            if (cur_time - server_to_client[0][1] < (buffer[1] - '0') ||
                cur_time - server_to_client[1][1] < (buffer[1] - '0'))
            {
                // wait until server1/2 finish
                return -1;
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

void queue_scheduler(int *changed)
{
    int finish = 0;
    while ((changed[0] || changed[1]) && !finish)
    {
        finish = 1;
        for (auto it = tasks_queue.begin(); it != tasks_queue.end(); it++)
        {
            // search for the first not music task
            if (it->buf[0] != 'M')
            {
                int server_num;
                if ((server_num = scheduler(it->buf)) < 0)
                {
                    finish = 1;
                    break;
                }
                server_to_client[server_num][0] = it->client_fd;
                server_to_client[server_num][1] = time(NULL);
                tasks_queue.erase(it);
                changed[server_num] = 0;
                finish = 0;
                break;
            }
        }
    }
    finish = 0;
    while (changed[2])
    {
        finish = 1;
        for (auto it = tasks_queue.begin(); it != tasks_queue.end(); it++)
        {
            // search for the first not music task
            if (it->buf[0] == 'M')
            {
                int server_num;
                if ((server_num = scheduler(it->buf)) < 0)
                {
                    finish = 1;
                    break;
                }
                server_to_client[server_num][0] = it->client_fd;
                server_to_client[server_num][1] = time(NULL);
                tasks_queue.erase(it);
                changed[server_num] = 0;
                finish = 0;
                break;
            }
        }
    }
    for (auto it = tasks_queue.begin(); it != tasks_queue.end(); it++)
    {
        int server_num;
        if ((server_num = scheduler(it->buf)) < 0)
        {
            continue;
        }
        server_to_client[server_num][0] = it->client_fd;
        server_to_client[server_num][1] = time(NULL);
        tasks_queue.erase(it);
        changed[server_num] = 0;
        return;
    }
}


int lb(int *servers_fds, int lb_fd, struct sockaddr_in fd_address)
{

    int addrlen = sizeof(fd_address);

    while (1)
    {
        char buffer[1024] = {0};
        int client_new_soc;
        // check (non-blocking) if servers done jobs
        int changed[3] = {0};
        for (int i = 0; i < NUM_OF_SERVERS; i++)
        {
            int bytes_read;
            // pass answer form server to client
            if ((bytes_read = read(servers_fds[i], buffer, 1024)) > 0)
            {
                // return the answer to the client
                write(server_to_client[i][0], buffer, bytes_read);
                changed[i] = 1;
            }
        }
        if (changed && !tasks_queue.empty())
        {
            queue_scheduler(changed);
        }
        if ((client_new_soc = accept(lb_fd, (struct sockaddr *)&fd_address,
                                     (socklen_t *)&addrlen)) < 0)
        {
            // if there is no client try to connect
            continue;
        }
        read(client_new_soc, buffer, 1024);
        // sent to server and updated server_to_client
        int server_num;
        if ((server_num = scheduler(buffer)) < 0)
        {
            struct task *temp_task;
            temp_task->client_fd = client_new_soc;
            strcpy(temp_task->buf, buffer);
            tasks_queue.push_back(*temp_task);
            continue;
        }
        server_to_client[server_num][0] = client_new_soc;
        server_to_client[server_num][1] = time(NULL);
    }
}

int main()
{
    int servers_fds[3];
    int lb_fd;
    struct sockaddr_in fd_address;
    // set up the lb 
    // set up connection with servers
    if (servers_connection((int **)&servers_fds) < 0)
    {
        return -1;
    }
    // set up connection with clients
    if (clients_connection((int *)&lb_fd, &fd_address) < 0)
    {
        return -1;
    }
}