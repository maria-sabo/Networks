#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <ctype.h>
#include <io.h>
#include <time.h>

#define MESSAGE_SIZE 10000
#define LOGIN_SIZE 10

struct CLIENT {
    int number;
    char id[LOGIN_SIZE];
    char *address;
    unsigned short port;
} *clients;

DWORD WINAPI listeningThread(void *info);

void *kickClient(int number, int listener);

int numberOfClients = 0;
HANDLE mutex;

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("Wrong command line format (server.exe ip port) \n");
        exit(1);
    }
    struct sockaddr_in local;
    local.sin_family = AF_INET;
    local.sin_port = htons(atoi(argv[2]));
    local.sin_addr.s_addr = inet_addr(argv[1]);

    WSADATA lpWSAData;
    // поддерживается ли запрошенная версия DLL
    if (WSAStartup(MAKEWORD(1, 1), &lpWSAData) != 0) return 0;

    // создание сокета
    int listeningSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (listeningSocket == INVALID_SOCKET) {
        perror("Cannot create socket to listen \n");
        exit(1);
    }
    // связывание сокета с портом
    int rc = bind(listeningSocket, (struct sockaddr *) &local, sizeof(local));
    if (rc == SOCKET_ERROR) {
        perror("Cannot bind socket \n");
        exit(1);
    }

    // создание мьютекса
    mutex = CreateMutex(NULL, FALSE, NULL);
    if (mutex == NULL) {
        perror("Failed to create mutex \n");
        exit(1);
    }

    // создание  потока
    HANDLE thread = CreateThread(NULL, 0, &listeningThread, &listeningSocket, 0, NULL);
    if (thread == NULL) {
        printf("Cannot create listening thread \n");
        fflush(stdout);
        exit(1);
    }

    printf("Use \'-who\' to show all online users \n");
    printf("Use \'-kick No\' to kick client by number \n");
    printf("Use \'-quit\' to shutdown server \n");
    printf("Waiting for input messages...\n");

    fflush(stdout);
    char buf[100];
    // основной цикл работы
    for (;;) {
        memset(buf, 0, 100);
        fgets(buf, 100, stdin);
        buf[strlen(buf) - 1] = '\0';

        if (!strcmp("-help", buf)) {
            printf("Help:\n");
            printf("Use \'-who\' to show all online users \n");
            printf("Use \'-kick No\' to kick client by number \n");
            printf("Use \'-quit\' to shutdown server \n");
            fflush(stdout);
        } else if (!strcmp("-who", buf)) {
            printf("Clients online:\n");
            WaitForSingleObject(&mutex, INFINITE);
            for (int i = 0; i < numberOfClients; i++) {
                if (strcmp(clients[i].id, "")) {
                    printf(" %3d    %10s      %10s          %6d\n", clients[i].number, clients[i].id,
                           clients[i].address,
                           clients[i].port);
                }
            }
            ReleaseMutex(&mutex);
        } else if (!strcmp("-quit", buf)) {
            break;
        } else {
            char *separator = " ";
            char *string = strtok(buf, separator);
            if (string == NULL) {
                printf("Undefined message \n");
                fflush(stdout);
                continue;
            }
            if (!strcmp("-kick", string)) {
                string = strtok(NULL, separator);
                int id = atoi(string);
                kickClient(id, listeningSocket);
            }
        }
    }
    shutdown(listeningSocket, 2);
    closesocket(listeningSocket);
    WaitForSingleObject(thread, INFINITE);
    printf("Server terminated \n");
    WSACleanup();
    return 0;
}

DWORD WINAPI listeningThread(void *info) {
    int listener = *((int *) info);
    struct sockaddr_in addre;
    int length = sizeof(addre);
    char message[MESSAGE_SIZE + 1];
    char ansmessage[MESSAGE_SIZE + 1];
    char rmessage[MESSAGE_SIZE + 1];
    char login[LOGIN_SIZE + 1];
    int newClient;
    for (;;) {
        memset(message, 0, sizeof(message));
        if (recvfrom(listener, message, MESSAGE_SIZE, 0, (struct sockaddr *) &addre, &length) == -1) {
            printf("Incorrect receive \n");
        } else {
            char *l = strtok(message, " ");
            strcpy(login, l);
            l = strtok(NULL, "\0");
            strcpy(rmessage, l);
            strcpy(message, rmessage);
            newClient = 1;
            for (int i = 0; i < numberOfClients; i++) {
                if ((clients[i].address == inet_ntoa(addre.sin_addr))
                    && (clients[i].port == addre.sin_port)) {
                    newClient = 0;
                    break;
                }
            }
            if (newClient) {
                clients = (struct CLIENT *) realloc(clients, sizeof(struct CLIENT) * (numberOfClients + 1));
                clients[numberOfClients].address = inet_ntoa(addre.sin_addr);
                clients[numberOfClients].port = addre.sin_port;
                strcpy(clients[numberOfClients].id, login);
                clients[numberOfClients].number = numberOfClients;
                numberOfClients++;
            }
            memset(ansmessage, 0, sizeof(ansmessage));
            strcpy(ansmessage, message);
            char ans[MESSAGE_SIZE + 1];

            if (!strcmp("-who", message)) {
                memset(ansmessage, 0, sizeof(ansmessage));
                memset(ans, 0, sizeof(ans));
                for (int i = 0; i < numberOfClients; i++) {
                    if (strcmp(clients[i].id, "")) {
                        sprintf(ans, " %3d   %10s      %10s          %6d\n", clients[i].number, clients[i].id,
                                clients[i].address,
                                clients[i].port);
                        strcat(ansmessage, ans);
                    }
                }
            }
            if (!strcmp("-ls", message)) {
                struct _finddata_t f;
                intptr_t hFile;

                memset(ansmessage, 0, sizeof(ansmessage));
                strcat(ansmessage, "\n");

                if ((hFile = _findfirst("*.*", &f)) == -1L)
                    strcpy(ansmessage, "No files in current directory!\n");
                else {
                    do {
                        char str[MESSAGE_SIZE + 1] = {0};
                        sprintf(str, " %-20s %10ld %30s \n",
                                f.name, f.size, ctime(&f.time_write));
                        strcat(ansmessage, str);
                    } while (_findnext(hFile, &f) == 0);
                    _findclose(hFile);
                }
            }
            char msgcd[MESSAGE_SIZE] = {0};
            strcpy(msgcd, message);
            char *stringcd = strtok(msgcd, " ");

            if (!strcmp("-cd", stringcd)) {
                stringcd = strtok(NULL, " ");
                int result;
                result = chdir(stringcd);
                if (result != 0)
                    strcpy(ansmessage, "Can't change directory \n");
                else strcpy(ansmessage, stringcd);
            }

            if (!strcmp("-kick", stringcd)) {
                stringcd = strtok(NULL, " ");
                int result;
                result = atoi(stringcd);
                kickClient(result, listener);
            }
            if (!strcmp("-logout", stringcd)) {
                for (int i = 0; i < numberOfClients; i++) {
                    if ((clients[i].address == inet_ntoa(addre.sin_addr))
                        && (clients[i].port == addre.sin_port)) {
                        printf("Client number %d terminated \n", clients[i].number);
                        strcpy(clients[i].id, "\0");
                    }
                }
            }
            if (sendto(listener, ansmessage, strlen(ansmessage), 0, (struct sockaddr *) &addre, length) == -1) {
                printf("Incorrect sending \n");
                break;
            }
            printf("%s \n", message);
        }
    }
}

// удаление клиента по номеру
void *kickClient(int number, int listener) {
    WaitForSingleObject(&mutex, INFINITE);
    for (int i = 0; i < numberOfClients; i++) {
        if (i == number) {
            struct sockaddr_in addrek;
            memset(&addrek, 0, sizeof(struct sockaddr_in));
            addrek.sin_family = AF_INET;
            addrek.sin_addr.s_addr = inet_addr((clients[i].address));
            addrek.sin_port = clients[i].port;
            strcpy(clients[i].id, "\0");
            if (sendto(listener, "-kickout \0", strlen("-kickout \0"), 0, (struct sockaddr *) &addrek,
                       sizeof(addrek)) == -1) {
                printf("Incorrect sending \n");
            }
            break;
        }
    }
    ReleaseMutex(&mutex);
}
