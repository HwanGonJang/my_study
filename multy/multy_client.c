#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <pthread.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>

#define PORT 8000
#define MAX_MESSAGE_LEN 128
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
struct termios initial_settings, new_settings;

struct my_socket
{
    int socket;
    struct sockaddr_in address;
    char buffer[MAX_MESSAGE_LEN];
    char write_buffer[MAX_MESSAGE_LEN];
};

void reset_terminal()
{
    tcsetattr(STDIN_FILENO, TCSANOW, &initial_settings);
}

void *read_with_timeout(void *arg)
{
    int rx_len, wx_len = 0;
    struct timeval timeout;
    fd_set readFds;

    // recive time out config
    // Set 1ms timeout counter
    struct my_socket *socket = (struct my_socket *)arg;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    memset(socket->buffer, 0, MAX_MESSAGE_LEN);
    memset(socket->write_buffer, 0, MAX_MESSAGE_LEN);

    int flags = fcntl(STDIN_FILENO, F_GETFL);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    int index = 0;
    char unicode[20];

    while (1)
    {
        FD_ZERO(&readFds);
        FD_SET(STDIN_FILENO, &readFds);
        FD_SET(socket->socket, &readFds);
        select(socket->socket + 1, &readFds, NULL, NULL, &timeout);
        printf("\r\033[K");
        printf("\r>> %s", socket->write_buffer);
        fflush(stdout);

        if (FD_ISSET(socket->socket, &readFds))
        {
            rx_len = read(socket->socket, socket->buffer, MAX_MESSAGE_LEN);
            if (rx_len > 0)
            {
                pthread_mutex_lock(&mutex);
                printf("\n%s\n", socket->buffer);
                pthread_mutex_unlock(&mutex);
                memset(socket->buffer, 0, MAX_MESSAGE_LEN);
            }
            else
            {
                printf("Fail to read data\n");
                exit(0);
            }
        }
        if (FD_ISSET(STDIN_FILENO, &readFds))
        {
            pthread_mutex_lock(&mutex);

            rx_len = read(STDERR_FILENO, unicode, 5);
            if ((c != '\n'))
                printf("key got: %s\n", c);
            // socket->write_buffer[index++] = c;
            // else
            // {
            //     if (strcmp(socket->write_buffer, "exit") == 0)
            //         break;
            //     write(socket->socket, socket->write_buffer, index);
            //     memset(socket->write_buffer, 0, MAX_MESSAGE_LEN);
            //     index = 0;
            // }
            pthread_mutex_unlock(&mutex);
            // }
        }
    }
    return NULL;
}

int main()
{
    int client_socket;
    // 클라이언트 소켓 생성
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_address;
    pthread_t read_msg, write_msg, render_msg;

    tcgetattr(STDIN_FILENO, &initial_settings);
    new_settings = initial_settings;
    new_settings.c_lflag &= ~ICANON; // 정규 모드 비활성화
    new_settings.c_lflag &= ~ECHO;   // 에코 비활성화
    tcsetattr(STDIN_FILENO, TCSANOW, &new_settings);
    atexit(reset_terminal);

    if (client_socket == -1)
    {
        perror("Socket creation failed");
        exit(1);
    }

    // 서버 주소 설정
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    // 54.180.134.95
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

    printf("Server connected: %s:%d\n", inet_ntoa(server_address.sin_addr), ntohs(server_address.sin_port));

    // 닉네임 입력
    printf("Input your name\n");
    printf(">> ");
    char name[20] = "jmj";
    write(socket.socket, name, strlen(name));
    pthread_create(&read_msg, NULL, read_with_timeout, &socket);
    pthread_join(read_msg, NULL);
    return EXIT_SUCCESS;
}

// 클라이언트 소켓 닫기
// pthread_cancel(read_msg);
// close(client_socket);
// return 0;
// }
