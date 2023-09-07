#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>

#define MAX_CLIENTS 10
#define PORT 8000
#define MAX_MESSAGE_LEN 256
struct connection *head = NULL;

// 클라이언트 정보 구조체
struct client_socket
{
    int socket;
    char *addr;
    int port;
    char buffer[MAX_MESSAGE_LEN];
};

struct connection
{
    struct client_socket *client;
    struct connection *next;
    unsigned int is_alive;
};

// struct client_socket clients[MAX_CLIENTS];
pthread_t client_threads[MAX_CLIENTS];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
// 클라이언트와 채팅하는 함수

struct connection *create_conenction(int client_fd, struct sockaddr_in addr)
{
    struct client_socket *client = (struct client_socket *)malloc(sizeof(struct client_socket));
    client->socket = client_fd;
    client->addr = inet_ntoa(addr.sin_addr);
    client->port = ntohs(addr.sin_port);

    struct connection *new_connection = (struct connection *)malloc(sizeof(struct connection));
    new_connection->client = client; // 클라이언트 구조체의 정보 저장
    new_connection->is_alive = 1;    // connection is alive
    new_connection->next = NULL;

    return new_connection;
}

int close_socket_connection()
{
    struct connection *cur = head;
    pthread_mutex_lock(&mutex);
    while (cur != NULL)
    {
        if (cur->is_alive == 0) // 내 소켓이 아니고 연결이 살아있으면
        {
            struct client_socket *client = cur->client;
            if (cur->next == NULL) // 연결이 한개만 존재하는 경우
            {
                printf("Server Removed connection: %s:%d \n", client->addr, client->port);
                close(client->socket);
                free(cur);    // 커넥션 자원할당 제거
                free(client); // 소켓할당 제거
                head = NULL;  // 연결이 하나도 없는 경우 HEAD를 NULL로 초기화
                break;
            }
            else
            {
                cur->next = cur->next->next; // 삭제 진행
                printf("Server Removed connection: %s:%d \n", client->addr, client->port);
                close(client->socket);
                free(cur);    // 커넥션 자원할당 제거
                free(client); // 소켓할당 제거
            }
        }
        cur = cur->next;
    }
    pthread_mutex_unlock(&mutex);
    return 0;
}

void *handle_client(void *arg)
{
    struct connection *connection = (struct connection *)arg;
    struct client_socket *client = connection->client;

    int port = client->port;
    char *addr = client->addr;
    int my_socket = client->socket;

    char msg[MAX_MESSAGE_LEN];
    ssize_t bytes_received;

    if (client == NULL)
        return NULL;

    while (1)
    {
        // printf("my socket %d addr %s port %d \n", my_socket, addr, port);
        // 소켓 데이터 읽기
        bytes_received = read(my_socket, client->buffer, MAX_MESSAGE_LEN);
        // 연결 종료시
        if (bytes_received <= 0)
        {
            // printf("Client disconnected\n");
            connection->is_alive = 0;
            close_socket_connection();
            break;
        }

        sprintf(msg, "%s: %s", addr, client->buffer); // 전송자 데이터 포함 메시지 생성
        printf("Received from: %s:%d : %s \n", addr, port, msg);

        // 다른 클라이언트들에게 메시지 전송

        struct connection *cur = head;
        printf("hello: %d %d  %s \n", connection->client->socket, my_socket, connection->client->addr);

        // pthread_mutex_lock(&mutex);

        while (cur != NULL)
        {
            struct client_socket *now_client = cur->client;
            printf("hello: %d %d  %s \n", now_client->socket, my_socket, now_client->addr);

            if ((now_client->socket != my_socket) && (cur->is_alive > 0)) // 내 소켓이 아니고 연결이 살아있으면
            {
                write(cur->client->socket, msg, MAX_MESSAGE_LEN); // msg 전송
                int send_port = cur->client->port;
                printf("Send to %s:%d msg:%s \n", addr, send_port, msg);
            }
            cur = cur->next;
        }
        // pthread_mutex_unlock(&mutex);

        memset(client->buffer, 0, strlen(client->buffer));
        memset(msg, 0, strlen(msg)); // msg 버퍼를 비워준다.
        // close_socket_connection();
    }

    return NULL;
}

int main()
{
    int server_socket, client_socket;
    struct sockaddr_in server_address, client_address;
    socklen_t client_address_len = sizeof(client_address);
    int num_clients = 0;

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
    struct connection *cur = NULL;
    while (1)
    {
        // 클라이언트 연결 대기
        client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_address_len);
        if (client_socket > 0)
        {
            printf("Client connected: %s:%d\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));
            struct connection *new_connection = create_conenction(client_socket, client_address);
            if (head == NULL)
            {
                head = new_connection;
                cur = head;
            }
            else
            {
                pthread_mutex_lock(&mutex);
                cur->next = new_connection;
                cur = cur->next;
                pthread_mutex_unlock(&mutex);
            }
            // 멀티 채팅을 위한 스레드 생성
            pthread_create(&client_threads[num_clients++], NULL, handle_client, new_connection);
        }
        // printf("Close...\n");
        // close_socket_connection();
    }

    for (int i = 0; i < num_clients; i++)
    {
        pthread_join(client_threads[i], NULL); // 스레드가 종료될 때 까지 대기
    }

    // 서버 소켓 닫기
    close(server_socket);
    return 0;
}
