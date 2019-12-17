#include <stdio.h>
#include <string.h>
#include <windows.h>

#define MESSAGE_SIZE 50

int readN(int socket, char *buf);

int main(int argc, char **argv) {

    WSADATA lpWSAData;
    if (WSAStartup(MAKEWORD(1, 1), &lpWSAData) != 0) return 0;

    struct sockaddr_in peer;
    peer.sin_family = AF_INET;

    char inputBuf[MESSAGE_SIZE];
    int s;

    for (;;) {
        printf("ENTER IP ADDRESS:\n");
        fflush(stdout);
        memset(inputBuf, 0, sizeof(inputBuf));
        fgets(inputBuf, sizeof(inputBuf), stdin);
        inputBuf[strlen(inputBuf) - 1] = '\0';
        peer.sin_addr.s_addr = inet_addr(inputBuf);

        printf("ENTER SERVER PORT NUMBER:\n");
        fflush(stdout);
        memset(inputBuf, 0, sizeof(inputBuf));
        fgets(inputBuf, sizeof(inputBuf), stdin);
        inputBuf[strlen(inputBuf) - 1] = '\0';
        peer.sin_port = htons(atoi(inputBuf));

        s = socket(AF_INET, SOCK_STREAM, 0); // создание сокета
        if (s == INVALID_SOCKET) {
            printf("CANNOT CREATE SOCKET.\n");
            fflush(stdout);
            continue;
        }
        // подключение к серверу
        int rc = connect(s, (struct sockaddr *) &peer, sizeof(peer));
        if (rc == SOCKET_ERROR) {
            printf("CANNOT CONNECT TO SERVER.\n");
            fflush(stdout);
            continue;
        } else {
            printf("CONNECTED TO SERVER SUCCESSFULLY.\n");
            fflush(stdout);
            break;
        }
    }

    char message[MESSAGE_SIZE] = {0};
    printf("INPUT MESSAGES:\n");
    fflush(stdout);
    for (;;) {
        fgets(inputBuf, sizeof(inputBuf), stdin);
        inputBuf[strlen(inputBuf) - 1] = '\0';
        if (!strcmp("-quit", inputBuf)) {
            break;
        } else {
            // передача данных
            send(s, inputBuf, sizeof(inputBuf), 0);
            if (readN(s, message) == -1) {
                printf("CANNOT READ MESSAGES FROM SERVER.\n");
                fflush(stdout);
                break;
            } else {
                printf("MESSAGE FROM SERVER: %s\n", message);
                fflush(stdout);
            }
            memset(inputBuf, 0, MESSAGE_SIZE);
        }
    }
    shutdown(s, 2);
    closesocket(s);
    printf("CLIENT TERMINATED.\n");
    fflush(stdout);
    return 0;
}


int readN(int socket, char *buf) {
    int result = 0;
    int readedBytes = 0;
    int messageSize = MESSAGE_SIZE;
    while (messageSize > 0) {
        readedBytes = recv(socket, buf + result, messageSize, 0);
        if (readedBytes <= 0) {
            return -1;
        }
        result += readedBytes;
        messageSize -= readedBytes;
    }
    return result;
}
