#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>

#define MAX_CLIENTS 10
#define PORT 8000

// 클라이언트 정보 구조체
struct client_info
{
    int socket;
    struct sockaddr_in address;
    char *name;
};

struct client_info clients[MAX_CLIENTS];
pthread_t client_threads[MAX_CLIENTS];
int num_clients = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

#define MAX_MESSAGE_LEN 256
// 클라이언트와 채팅하는 함수
void *handle_client(void *arg)
{
    struct client_info *client = (struct client_info *)arg;
    int client_socket = client->socket;
    char *addr = inet_ntoa(client->address.sin_addr);
    int port = ntohs(client->address.sin_port);
    char buffer[MAX_MESSAGE_LEN];
    char msg[MAX_MESSAGE_LEN];
    ssize_t bytes_received;

    while (1)
    {
        memset(buffer, 0, MAX_MESSAGE_LEN);
        memset(msg, 0, MAX_MESSAGE_LEN); // msg 버퍼를 비워준다.
        bytes_received = read(client_socket, buffer, MAX_MESSAGE_LEN);

        if (bytes_received <= 0)
        {
            printf("Client disconnected\n");
            break;
        }

        // 다른 클라이언트들에게 메시지 전송
        pthread_mutex_lock(&mutex);
        sprintf(msg, "%s: %s", addr, buffer);
        printf("Received from: %s:%d : %s \n", addr, port, msg);
        for (int i = 0; i < num_clients; i++)
        {
            if (clients[i].socket != client_socket)
            {
                write(clients[i].socket, msg, MAX_MESSAGE_LEN); // msg 전송
                int send_port = ntohs(clients[i].address.sin_port);
                printf("Send to %s:%d msg:%s \n", addr, send_port, msg);
            }
        }
        pthread_mutex_unlock(&mutex);
    }

    // 클라이언트 소켓 닫기
    close(client_socket);
    return NULL;
}

int main()
{
    int server_socket, client_socket;
    struct sockaddr_in server_address, client_address;
    socklen_t client_address_len = sizeof(client_address);

    // 서버 소켓 생성
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
        printf("setsockopt(SO_REUSEADDR) failed");

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(PORT);

    // 서버 바인딩
    bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address));

    // 서버 리스닝
    listen(server_socket, 5);
    printf("Server listening on port %d...\n", PORT);

    while (1)
    {
        // 클라이언트 연결 대기
        client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_address_len);
        printf("Client connected: %s:%d\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));

        // 클라이언트 정보 저장
        clients[num_clients].socket = client_socket;
        clients[num_clients].address = client_address;

        // 클라이언트 스레드 생성
        pthread_create(&client_threads[num_clients], NULL, handle_client, &clients[num_clients]);
        num_clients++;
    }
    for (int i = 0; i < num_clients; i++)
    {
        pthread_join(client_threads[i], NULL); // 스레드가 종료될 때 까지 대기
    }

    // 서버 소켓 닫기
    close(server_socket);
    return 0;
}
