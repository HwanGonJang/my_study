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
    char buffer[MAX_MESSAGE_LEN];
    char write_buffer[MAX_MESSAGE_LEN];
};

void *read_with_timeout(void *arg)
{
    int rx_len, wx_len = 0;
    struct timeval timeout;
    fd_set readFds;

    // recive time out config
    // Set 1ms timeout counter
    struct my_socket *socket = (struct my_socket *)arg;
    timeout.tv_sec = 0;
    timeout.tv_usec = 5000;
    while (1)
    {
        FD_ZERO(&readFds);
        FD_SET(socket->socket, &readFds);
        // FD_SET(STDIN_FILENO, &readFds);
        select(socket->socket + 1, &readFds, NULL, NULL, &timeout);

        if (FD_ISSET(socket->socket, &readFds))
        {
            rx_len = read(socket->socket, socket->buffer, MAX_MESSAGE_LEN);
            if (rx_len > 0)
            {
                pthread_mutex_lock(&mutex);
                printf("\r\033[K");
                printf("%s \n", socket->buffer);
                pthread_mutex_unlock(&mutex);
                memset(socket->buffer, 0, MAX_MESSAGE_LEN);
            }
            else
            {
                printf("Fail to read data\n");
                return NULL;
            }
        }
    }
    return NULL;
}

// char *input(char *buffer)
// {
//     int i = 0;
//     while (1)
//     {
//         printf("\r\033[K");
//         printf(">> %s", buffer);
//         int c = getchar();         // 문자 하나를 읽음
//         buffer[i++] = c;           // 입력 배열에 문자 추가
//         if (c == '\n' || c == EOF) // 줄바꿈 등의 이스케이프 문자를 넣어야 서버에서 바이트의 종료시점을 파악한다.
//         {                          // '\n' 또는 EOF(End Of File)를 만나면 입력 종료
//             break;
//         }
//     }
//     return buffer;
// }

int main()
{
    int client_socket;
    // char write_buffer[MAX_MESSAGE_LEN];
    // 클라이언트 소켓 생성
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_address;
    pthread_t read_msg;

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

    if (connect(client_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1)
    {
        perror("Connection failed\n");
        close(client_socket);
        exit(1);
    }

    char *write_buffer = socket.write_buffer;
    pthread_create(&read_msg, NULL, read_with_timeout, &socket);
    memset(write_buffer, 0, strlen(write_buffer));

    while (1)
    {
        // printf("\r\033[K");
        printf(">> ");
        scanf("%s", write_buffer);
        if ((strncmp(write_buffer, "exit", 4)) == 0)
        {
            printf("\nClient exit...");
            return 0;
        }
        write(client_socket, write_buffer, strlen(write_buffer));
        memset(write_buffer, 0, strlen(write_buffer));
    }
    // 클라이언트 소켓 닫기
    close(client_socket);
    return 0;
}
