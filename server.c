#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>


#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024
#define MAX_KEYS 10

typedef struct {
    char key[BUFFER_SIZE];
    char value[BUFFER_SIZE];
} KeyValue;

KeyValue* key_value_stores = NULL;
int kv_count = 0;

void Resp(int client_socket, char* buffer);
void set_kv(const char* key, const char* value, int client_socket);
char* get_value(const char* key);

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        return -1;
    }

    int PORT = atoi(argv[1]); // 포트 번호 받기
    char buffer[BUFFER_SIZE];

    // 동적 메모리 할당
    key_value_stores = (KeyValue*)malloc(sizeof(KeyValue) * MAX_KEYS);
    if (key_value_stores == NULL) {
        printf("Failed to allocate memory\n");
        return -1;
    }

    // 저장소 NULL 초기화
    for (int i = 0; i < MAX_KEYS; i++) {
        memset(&key_value_stores[i], 0, sizeof(KeyValue));
    }

    int server_socket, client_socket, max_sd, sd, activity, new_socket;
    int client_sockets[MAX_CLIENTS] = { 0 };
    struct sockaddr_in serv_addr, clnt_addr;
    socklen_t clnt_addr_size = sizeof(clnt_addr); // 클라이언트 주소 길이 초기화
    //char buffer[BUFFER_SIZE];
    fd_set readfds;

    // 서버 소켓 생성
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        printf("socket creation error\n");
        return -1;
    }

    // 소켓 주소 설정
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET; // 주소 체계가 IPv4
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 모든 연결 허용
    serv_addr.sin_port = htons(PORT); // 호스트 바이트 순서를 네트워크 바이트 순서로 변환

    // 소켓을 주소에 바인딩
    if (bind(server_socket, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
        printf("Bind failed\n");
        close(server_socket);
        return -1;
    }

    // TCP 연결 listen
    if (listen(server_socket, 3) == -1) { // 최대 3
        printf("listen failed\n");
        close(server_socket);
        return -1;
    }

   

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(server_socket, &readfds);
        max_sd = server_socket;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = client_sockets[i];
            if (sd > 0) FD_SET(sd, &readfds);
            if (sd > max_sd) max_sd = sd;
        }

        // select 호출
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0) {
            printf("Select error\n");
            continue;
        }

        // 새로운 연결 요청 수락
        if (FD_ISSET(server_socket, &readfds)) {
            new_socket = accept(server_socket, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
            if (new_socket < 0) {
                printf("Accept error\n");
                return -1;
            }

            printf("New connection, socket fd is %d, ip is : %s, port : %d\n",
                new_socket, inet_ntoa(clnt_addr.sin_addr), ntohs(clnt_addr.sin_port));

            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_sockets[i] == 0) {
                    client_sockets[i] = new_socket;
                    break;
                }
            }
        }

        // 데이터 수신 및 처리
        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = client_sockets[i];
            if (FD_ISSET(sd, &readfds)) {
                int valueRead = recv(sd, buffer, BUFFER_SIZE, 0);
                if (valueRead == 0) {
                    close(sd);
                    client_sockets[i] = 0;
                    printf("Host disconnected, socket fd is %d\n", sd);
                }
                else {
                    buffer[valueRead] = '\0';
                    Resp(sd, buffer); // RESP 프로토콜에 따라 처리
                    memset(buffer, '\0', BUFFER_SIZE);
                }
            }
        }
    }

    free(key_value_stores); // 동적 할당된 메모리 해제
    close(server_socket);
    return 0;
}

void Resp(int client_socket, char* buffer) {
    char command[BUFFER_SIZE];
    char key[BUFFER_SIZE];
    char value[BUFFER_SIZE];
    char* token;
    int num_args;

    // 파싱
    // 들어오는 양식
    //*3\r\n$ % ld\r\n % s\r\n$ % ld\r\n % s\r\n$ % ld\r\n % s\r\n
    //"*2\r\n$%ld\r\n%s\r\n$%ld\r\n%s\r\n",
   
      // 첫 번째 토큰: *3
    token = strtok(buffer, "\r\n");
    if (token == NULL || token[0] != '*') {
        printf("Invalid format\n");
        return;
    }

    num_args = atoi(token + 1); // * 뒤의 숫자를 읽음
    if (num_args < 2 || num_args > 3) {
        printf("Invalid number of arguments\n");
        return;
    }

    // 두 번째 토큰: $<command length>
    token = strtok(NULL, "\r\n");
    if (token == NULL || token[0] != '$') {
        printf("Invalid format\n");
        return;
    }

    // 세 번째 토큰: <command>
    token = strtok(NULL, "\r\n");
    strcpy(command, token);

    // 네 번째 토큰: $<key length>
    token = strtok(NULL, "\r\n");
    if (token == NULL || token[0] != '$') {
        printf("Invalid format\n");
        return;
    }

    // 다섯 번째 토큰: <key>
    token = strtok(NULL, "\r\n");
    strcpy(key, token);

    if (num_args == 3) {
        // 여섯 번째 토큰: $<value length>
        token = strtok(NULL, "\r\n");
        if (token == NULL || token[0] != '$') {
            printf("Invalid format\n");
            return;
        }

        // 일곱 번째 토큰: <value>
        token = strtok(NULL, "\r\n"); 
        strcpy(value, token);
    }


    /*
    if (buffer[0] == '$') {
        char* arr_num = strtok(buffer + 1, "\r\n");
        char* command_data = strtok(NULL, )

        if (buffer[1] == 3) { //set명령
            char* arr_num = strtok(buffer + 1, "\r\n"); //"*3\r\n$%ld\r\n%s\r\n$%ld\r\n%s\r\n$%ld\r\n%s\r\n", 여기서 *3부분
            char* length_Str = strtok(NULL, "\r\n");
            //int length = atoi(length_Str);
            char* key_data = strtok(NULL, "\r\n");
            char* value_data = strtok(NULL, "\r\n");


            strcpy(command, "set");
            strcpy()


          //  sscanf(value_data, "%s %s %s", command, key, value);
        }
        else if (buffer[1] == 2) {//get명령 //"*2\r\n$%ld\r\n%s\r\n$%ld\r\n%s\r\n",
            char* arr_num = strtok(buffer + 1, "\r\n")
                char* length_Str = strtok(NULL, "\r\n");
            //int length = atoi(length_Str);

        }
    }
    else {
        send(client_socket, "-ERR Invalid format\r\n", 21, 0);
        return;
    }
    */

    // 명령에 따라 처리
    if (strcmp(command, "get") == 0) {
        char* result = get_value(key);
        if (result != NULL) {
            char response[BUFFER_SIZE];
            snprintf(response, BUFFER_SIZE, "$%ld\r\n%s\r\n", strlen(result), result);
            send(client_socket, response, strlen(response), 0);
        }
        else {
            send(client_socket, "$-1\r\n", 5, 0);
        }
    }
    else if (strcmp(command, "set") == 0) {
        set_kv(key, value, client_socket);
    }
    else {
        send(client_socket, "-ERR unknown command\r\n", 22, 0);
    }
}

void set_kv(const char* key, const char* value, int client_socket) {
    // key가 이미 있는 경우 value를 업데이트
    for (int i = 0; i < kv_count; i++) {
        if (strcmp(key_value_stores[i].key, key) == 0) {
            strcpy(key_value_stores[i].value, value);
            send(client_socket, "+OK\r\n", 5, 0);
            return;
        }
    }

    // key가 없는 경우 새로운 키-값 쌍을 저장
    if (kv_count < MAX_KEYS) {
        strcpy(key_value_stores[kv_count].key, key);
        strcpy(key_value_stores[kv_count].value, value);
        kv_count++;
        send(client_socket, "+OK\r\n", 5, 0);
    }
    else {
        // 저장 공간이 가득 찬 경우 에러 메시지 반환
        send(client_socket, "-ERR storage full\r\n", 19, 0);
    }
}

char* get_value(const char* key) {
    for (int i = 0; i < kv_count; i++) {
        if (strcmp(key_value_stores[i].key, key) == 0) {
            return key_value_stores[i].value;
        }
    }
    return NULL;
}