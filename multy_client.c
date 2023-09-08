#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>

#define PORT 8000
#define MAX_MESSAGE_LEN 256
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

struct my_socket
{
    int socket;
    struct sockaddr_in address;
    int buffer_size;
    char *buffer;
};

int read_with_timeout(int fd, char *buffer, int buffer_size, int timeout_ms)
{
    int rx_len, wx_len = 0;
    struct timeval timeout;
    fd_set readFds;

    // recive time out config
    // Set 1ms timeout counter
    timeout.tv_sec = 0;
    timeout.tv_usec = timeout_ms * 1000;
    memset(buffer, 0, buffer_size);

    FD_ZERO(&readFds);
    FD_SET(fd, &readFds);
    // FD_SET(STDIN_FILENO, &readFds);
    select(fd + 1, &readFds, NULL, NULL, &timeout);

    if (FD_ISSET(fd, &readFds))
    {
        rx_len = read(fd, buffer, buffer_size);
    }
    // if (FD_ISSET(STDIN_FILENO, &readFds))
    // {
    //     if (!input(buffer, buffer_size))
    //         return 0;
    //     wx_len = write(fd, buffer, buffer_size);
    //     return wx_len;
    // }
    return rx_len;
}

// void *read_message(void *arg)
// {
//     struct my_socket *socket = (struct my_socket *)arg;
//     fd_set read_fds;
//     int client_socket = socket->socket;
//     int ret;
//     char temp[MAX_MESSAGE_LEN];
//     struct timeval timeout;
//     // FD_ZERO(&read_fds);
//     int thread_count = 0;
//     timeout.tv_sec = 5;
//     timeout.tv_usec = 0; // 0.5초

//     while (1)
//     {
//         pthread_mutex_lock(&mutex); // 컨텍스트 스위칭 방지
//         int read_success = 0;
//         FD_ZERO(&read_fds);
//         FD_SET(client_socket, &read_fds);

//         printf("out: %d\n", thread_count++);

//         ret = select(client_socket + 1, &read_fds, NULL, NULL, &timeout);
//         if (ret < 0)
//         {
//             printf("Select failed");
//             break;
//         }
//         if (FD_ISSET(client_socket, &read_fds))
//         {
//             read_success = read(client_socket, socket->read_buffer, sizeof(socket->read_buffer));
//             // printf("hello %s \n", socket->read_buffer);
//             printf("in: %d\n", thread_count++);
//             memset(socket->read_buffer, 0, sizeof(socket->read_buffer));
//         }
//         pthread_mutex_unlock(&mutex); // 락 풀어줌
//     }
//     return NULL;
// }

char *input(char *buffer, int buffer_size)
{
    int i = 0;
    memset(buffer, 0, buffer_size);
    while (1)
    {
        printf("\r>> %s", buffer);
        int c = getchar();         // 문자 하나를 읽음
        buffer[i++] = c;           // 입력 배열에 문자 추가
        if (c == '\n' || c == EOF) // 줄바꿈 등의 이스케이프 문자를 넣어야 서버에서 바이트의 종료시점을 파악한다.
        {                          // '\n' 또는 EOF(End Of File)를 만나면 입력 종료
            break;
        }
    }
    return buffer;
}

void *write_message(void *arg)
{
    struct my_socket *socket = (struct my_socket *)arg;
    int client_socket = socket->socket;

    while (1)
    {
        // pthread_mutex_lock(&mutex);

        // pthread_mutex_unlock(&mutex);
        input(socket->buffer, MAX_MESSAGE_LEN);
        //         // printf("\r>> %d", (socket->buffer)[0]);

        if ((socket->buffer)[0] != 0)
        {
            if ((strncmp(socket->buffer, "exit", 4)) == 0)
            {
                printf("Client exit...");
                return NULL;
            }
            write(client_socket, socket->buffer, MAX_MESSAGE_LEN);
        }
    }
    return NULL;
}

int main()
{
    int client_socket;
    char read_buffer[MAX_MESSAGE_LEN];
    char write_buffer[MAX_MESSAGE_LEN];
    // 클라이언트 소켓 생성
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_address;
    pthread_t write_msg;

    if (client_socket == -1)
    {
        perror("Socket creation failed");
        exit(1);
    }

    // 서버 주소 설정
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1"); // 서버 IP 주소

    struct my_socket socket;
    socket.socket = client_socket;
    socket.address = server_address;
    socket.buffer = write_buffer;

    if (connect(client_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1)
    {
        perror("Connection failed");
        close(client_socket);
        exit(1);
    }

    int count = 0;
    pthread_create(&write_msg, NULL, write_message, &socket);

    while (1)
    {
        // if (!input(write_buffer, MAX_MESSAGE_LEN))
        // return 0;
        // write(client_socket, write_buffer, strlen(write_buffer));

        int res = read_with_timeout(client_socket, read_buffer, MAX_MESSAGE_LEN, 1000);
        if (res > 0)
        {
            printf("\033[1A");
            printf("%d %s \n", count, read_buffer);
        }

        else
        {
            printf("Fail to read data\n");
            break;
        }
    }
    // 클라이언트 소켓 닫기
    close(client_socket);

    return 0;
}
