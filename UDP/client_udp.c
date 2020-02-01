#include <stdio.h>
#include <string.h>
#include <windows.h>

#define MESSAGE_SIZE 10000
#define LOGIN_SIZE 10

int main(int argc, char **argv) {

    WSADATA lpWSAData;
    if (WSAStartup(MAKEWORD(1, 1), &lpWSAData) != 0) return 0;

    struct sockaddr_in peer;
    struct sockaddr_in client;

    peer.sin_family = AF_INET;
    client.sin_family = AF_INET;

    char inputBuf[MESSAGE_SIZE];
    char login[LOGIN_SIZE];
    int s;
    int flag = 1;

    while (flag) {
        printf("Enter IP address: \n");
        fflush(stdout);
        memset(inputBuf, 0, sizeof(inputBuf));
        fgets(inputBuf, sizeof(inputBuf), stdin);
        inputBuf[strlen(inputBuf) - 1] = '\0';
        peer.sin_addr.s_addr = inet_addr(inputBuf);
        client.sin_addr.s_addr = inet_addr(inputBuf);

        printf("Enter server port number: \n");
        fflush(stdout);
        memset(inputBuf, 0, sizeof(inputBuf));
        fgets(inputBuf, sizeof(inputBuf), stdin);
        inputBuf[strlen(inputBuf) - 1] = '\0';
        peer.sin_port = htons(atoi(inputBuf));
        printf("Enter client login:\n");
        fflush(stdout);
        memset(login, 0, sizeof(login));
        fgets(login, sizeof(login), stdin);
        login[strlen(login) - 1] = '\0';

        s = socket(AF_INET, SOCK_DGRAM, 0); // создание сокета
        if (s == INVALID_SOCKET) {
            printf("Cannot create socket\n");
            fflush(stdout);
            continue;
        }

        char message[MESSAGE_SIZE - LOGIN_SIZE + 1];
        char smessage[MESSAGE_SIZE + 1];
        int length = sizeof(peer);

        for (;;) {
            memset(message, 0, sizeof(message));
            gets(&message);
            strcpy(smessage, login);
            strcat(smessage, " ");
            strcat(smessage, message);
            strcat(smessage, "\0");
            if (sendto(s, smessage, strlen(smessage), 0, &peer, length) == -1) {
                printf("Incorrect sending \n");
                break;
            }
            memset(message, 0, sizeof(message));
            if (recvfrom(s, message, MESSAGE_SIZE, 0, &peer, &length) == -1) {
                printf("Incorrect receive \n");
                break;
            }
            printf("%s \n", message);
            if (!strcmp("-kickout \0", message)) {
                flag = 0;
                break;
            }
            if (!strcmp("-logout", message)) {
                flag = 0;
                break;
            }
        }
    }
            shutdown(s, 2);
            closesocket(s);
            printf("Client terminated \n");
            fflush(stdout);
            return 0;
}