#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>

#define BUFFER_SIZE 1024

void send_command(int sock, const char* command, const char* key, const char* value);

int main(int argc, char* argv[]) {
    int sock;
    struct sockaddr_in serv_addr;
    int str_len;
    char buffer[BUFFER_SIZE];

    if (argc != 3) {
        printf("Invalid format\n");
        return -1;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        printf("socket creation error\n");
        return -1;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("connect error\n");
        close(sock);
        return -1;

    }

    while (1) {
        memset(buffer, '\0', BUFFER_SIZE);
        char command[BUFFER_SIZE];
        char key[BUFFER_SIZE];
        char value[BUFFER_SIZE];



        fgets(buffer, BUFFER_SIZE, stdin);
        buffer[strcspn(buffer, "\n")] = 0; // 개행 문자 제거

        if (!strncmp(buffer, "EXIT", 4)) { //EXIT입력
            break;
        }


        //get과 set command를 보내는 부분
        if (strncmp(buffer, "get", 3) == 0) {
            int str_lennth = strlen(buffer);
            char* command_input = strtok(buffer , " ");
            char* key_input = strtok(NULL, " ");
            strcpy(command, command_input);
            strcpy(key, key_input);
            send_command(sock, command, key, NULL);
        }
        else if (strncmp(buffer, "set", 3) == 0) {
            int str_lennth = strlen(buffer);
            char* command_input = strtok(buffer, " ");
            char* key_input = strtok(NULL, " ");
            strcpy(command, command_input);
            strcpy(key, key_input);

            char* value_input = strtok(NULL, " ");
            strcpy(value, value_input);

            send_command(sock, command, key, value);

        }
        else {
            printf("Invalid command\n");
            continue;
        }


        //서버로부터 받는다
        int str_len = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        if (str_len == -1) {
            printf("recv error\n");
            break;
        }
        else if (str_len == 0) {
            printf("Server closed\n");
            break;
        }

        buffer[str_len] = '\0'; // 개행추가
      
        //파싱
        if (buffer[0] == '+') {
            if (strncmp(buffer, "+OK", 3) == 0) {
                printf("+OK\n"); //제대로 set이 됐군
            }

        }
        else if (buffer[0] == '-') {
            printf("Improper response\n");
            printf("EXIT\n");
            break;
        }
        else if (buffer[0] == '$') {
            if (strncmp(buffer, "$-1", 3) == 0) {
                printf("$-1\n");
                printf("EXIT\n");
                break;
            }
            char* token = strtok(buffer, "\r\n"); // 버리는거
            token = strtok(NULL, "\r\n"); // value추출
            printf("%s\n", token);
        }

    }



    close(sock);
    return 0;
}

void send_command(int sock, const char* command, const char* key, const char* value) {
    char buffer[BUFFER_SIZE];

    if (value) {
        snprintf(buffer, sizeof(buffer), "*3\r\n$%ld\r\n%s\r\n$%ld\r\n%s\r\n$%ld\r\n%s\r\n",
            strlen(command), command, strlen(key), key, strlen(value), value);
    }
    else {
        snprintf(buffer, sizeof(buffer), "*2\r\n$%ld\r\n%s\r\n$%ld\r\n%s\r\n",
            strlen(command), command, strlen(key), key);
    }

    if (send(sock, buffer, strlen(buffer), 0) == -1) {
        printf("send error\n");
    }
}