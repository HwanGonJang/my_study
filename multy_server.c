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

int free_connection(struct connection *close_connection)
{
    printf("Server Removed connection: %s:%d \n", close_connection->client->addr, close_connection->client->port);
    pthread_mutex_lock(&mutex);
    close(close_connection->client->socket);
    free(close_connection->client); // 소켓할당 제거
    free(close_connection);         // 커넥션 자원할당 제거
    pthread_mutex_unlock(&mutex);
    return 0;
}

int close_socket_connection()
{
    struct connection *cur = head;
    struct connection *before = cur;

    // if (cur->next == NULL) //연결이 한개 밖에 없을 경우 head 값을 다시 NULL로 세팅
    //     head = NULL;

    while (cur != NULL)
    {

        if (cur->is_alive == 0) // 내 소켓이 아니고 연결이 살아있으면
        {
            before->next = cur->next; // 삭제 진행
            free_connection(cur);
            if (cur == head)
            {
                head = before->next;
            }
        }

        before = cur;
        cur = cur->next;
    }

    return 0;
}

void *handle_client(void *arg)
{

    /*
    사용자로 부터 메시지를 읽는 스레드 구동부입니다.
    각 커넥션 마다 스레드에 해당 함수가 배정됩니다.
    */

    struct connection *connection = (struct connection *)arg;
    struct client_socket *client = connection->client;

    int port = client->port;        // 클라이언트 포트
    char *addr = client->addr;      // 클라이언트 주소
    int my_socket = client->socket; // 소켓

    char msg[MAX_MESSAGE_LEN]; // 메시지 전송용 버퍼
    ssize_t bytes_received;    // 전달 받은 데이터 크킥

    while (1)
    {
        // 소켓 데이터 읽기
        bytes_received = read(my_socket, client->buffer, MAX_MESSAGE_LEN);

        // 연결 종료시
        if (bytes_received <= 0)
        {
            // pthread_mutex_lock(&mutex);
            connection->is_alive = 0;
            close_socket_connection(); // 연결 종료할 경우 커넥션 자원 수거
            // pthread_mutex_lock(&mutex);
            break;
        }

        sprintf(msg, "%s: %s", addr, client->buffer); // 전송자 데이터 포함 메시지 생성
        printf("Received from: %s:%d : %s \n", addr, port, msg);

        struct connection *cur = head; // 커넥션 연결 리스트의 헤드

        // 다른 클라이언트들에게 메시지 전송
        while (cur != NULL)
        {
            struct client_socket *now_client = cur->client;
            // printf("hello: %d %d  %s \n", now_client->socket, my_socket, now_client->addr);

            if ((now_client->socket != my_socket) && (cur->is_alive > 0)) // 내 소켓이 아니고 연결이 살아있으면
            {
                write(cur->client->socket, msg, MAX_MESSAGE_LEN); // msg 전송
                int send_port = cur->client->port;
                printf("Send to %s:%d msg:%s \n", addr, send_port, msg);
            }
            cur = cur->next;
        }

        memset(client->buffer, 0, strlen(client->buffer));
        memset(msg, 0, strlen(msg)); // msg 버퍼를 비워준다.
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
    printf("Server listening on port %d...\n", PORT); // 서버 설정
    struct connection *cur = NULL;
    while (1)
    {
        // 클라이언트 연결 대기
        client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_address_len); // 클라이언트 커넥션 할당
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
    }

    for (int i = 0; i < num_clients; i++)
    {
        pthread_join(client_threads[i], NULL); // 스레드가 종료될 때 까지 대기
    }

    // 서버 소켓 닫기
    close(server_socket);
    return 0;
}
