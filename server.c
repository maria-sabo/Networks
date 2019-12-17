#include <stdio.h>
#include <string.h>
#include <windows.h>

#define MESSAGE_SIZE 50

struct CLIENT {
    int number;
    HANDLE thread;
    int socket;
    char *address;
    int port;
} *clients;

DWORD WINAPI listeningThread(void *info);

DWORD WINAPI clientHandler(void *clientIndex);

int readN(int socket, char *buf);

void *kickClient(int number);

int numberOfClients = 0;
HANDLE mutex;

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("WRONG COMMAND LINE FORMAT (server.exe ip port) \n");
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
    int listeningSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listeningSocket == INVALID_SOCKET) {
        perror("CANNOT CREATE SOCKET TO LISTEN: \n");
        exit(1);
    } else {
        printf("LISTENING SOCKET CREATED SUCCESSFULLY \n");
        fflush(stdout);
    }
    // связывание сокета с портом
    int rc = bind(listeningSocket, (struct sockaddr *) &local, sizeof(local));
    if (rc == SOCKET_ERROR) {
        perror("CANNOT BIND SOCKET: \n");
        exit(1);
    } else {
        printf("LISTENING SOCKET BINDED SUCCESSFULLY.\n");
        fflush(stdout);
    }

    // перевод сокета в слушающий режим
    // 5 - максимальное число запросов на соединение, которые будут помещены в очередь
    if (listen(listeningSocket, 5) == SOCKET_ERROR) {
        perror("CANNOT LISTEN SOCKET: \n");
        exit(1);
    } else {
        printf("LISTENING INPUT MESSAGES STARTED \n");
        fflush(stdout);
    }

    // создание мьютекса
    mutex = CreateMutex(NULL, FALSE, NULL);
    if (mutex == NULL) {
        perror("FAILED TO CREATE MUTEX: \n");
        exit(1);
    }

    // создание  потока
    HANDLE thread = CreateThread(NULL, 0, &listeningThread, &listeningSocket, 0, NULL);
    if (thread == NULL) {
        printf("CANNOT CREATE LISTENING THREAD \n");
        fflush(stdout);
        exit(1);
    }

    printf("WAITING FOR INPUT MESSAGES...\n");
    fflush(stdout);
    char buf[100];

    // основной цикл работы
    for (;;) {
        memset(buf, 0, 100);
        fgets(buf, 100, stdin);
        buf[strlen(buf) - 1] = '\0';

        if (!strcmp("-help", buf)) {
            printf("HELP:\n");
            printf("USE \'-list\' TO SHOW ALL ONLINE USERS \n");
            printf("USE \'-kick No\' TO KICK CLIENT BY NUMBER \n");
            printf("USE \'-quit\' TO SHUTDOWN SERVER \n");
            fflush(stdout);
        } else if (!strcmp("-list", buf)) {
            printf("CLIENTS ONLINE:\n");
            WaitForSingleObject(&mutex, INFINITE);
            for (int i = 0; i < numberOfClients; i++) {
                if (clients[i].socket != INVALID_SOCKET)
                    printf(" %d          %s          %d\n", clients[i].number, clients[i].address, clients[i].port);
            }
            ReleaseMutex(&mutex);
            fflush(stdout);
        } else if (!strcmp("-quit", buf)) {
            break;
        } else {
            char *separator = " ";
            char *string = strtok(buf, separator);
            if (string == NULL) {
                printf("UNDEFINED MESSAGE \n");
                fflush(stdout);
                continue;
            }
            if (!strcmp("-kick", string)) {
                string = strtok(NULL, separator);
                int id = atoi(string);
                kickClient(id);
            }
        }
    }
    shutdown(listeningSocket, 2);
    closesocket(listeningSocket);
    WaitForSingleObject(thread, INFINITE);
    printf("SERVER TERMINATED \n");
    WSACleanup();
    return 0;
}

DWORD WINAPI listeningThread(void *info) {
    int listener = *((int *) info);

    int new, clientIndex;
    struct sockaddr_in addr;
    int length = sizeof(addr);

    for (;;) {
        // прием соединений
        new = accept(listener, (struct sockaddr *) &addr, &length);
        if (new == INVALID_SOCKET) {
            break;
        }

        WaitForSingleObject(&mutex, INFINITE);
        clients = (struct CLIENT *) realloc(clients, sizeof(struct CLIENT) * (numberOfClients + 1));
        clients[numberOfClients].socket = new;
        clients[numberOfClients].address = inet_ntoa(addr.sin_addr);
        clients[numberOfClients].port = addr.sin_port;
        clients[numberOfClients].number = numberOfClients;
        clientIndex = numberOfClients;
        clients[numberOfClients].thread = CreateThread(NULL, 0, &clientHandler, &clientIndex, 0, NULL);
        if (clients[numberOfClients].thread == NULL) {
            printf("CANNOT CREATE THREAD FOR NEW CLIENT \n");
            fflush(stdout);
            continue;
        } else {
            printf("NEW CLIENT CONNECTED!\n");
            fflush(stdout);
        }
        ReleaseMutex(&mutex);
        numberOfClients++;
    }
    printf("SERVER TERMINATION PROCESS STARTED...\n");
    fflush(stdout);
    WaitForSingleObject(&mutex, INFINITE);
    for (int i = 0; i < numberOfClients; i++) {
        shutdown(clients[i].socket, 2);
        closesocket(clients[i].socket);
        clients[i].socket = INVALID_SOCKET;
    }
    for (int i = 0; i < numberOfClients; i++) {
        WaitForSingleObject(clients[i].thread, INFINITE);
    }
    ReleaseMutex(&mutex);
    printf("LISTENING ENDED \n");
    fflush(stdout);
}

// обработчик одного клиента
DWORD WINAPI clientHandler(void *clientIndex) {
    WaitForSingleObject(&mutex, INFINITE);
    int index = *((int *) clientIndex);
    int socket = clients[index].socket;
    ReleaseMutex(&mutex);

    char message[MESSAGE_SIZE] = {0};
    for (;;) {
        if (readN(socket, message) <= 0) {
            break;
        } else {
            printf("RECEIVED MESSAGE: %s\n", message);
            fflush(stdout);
            send(socket, message, sizeof(message), 0);
        }
        memset(message, 0, sizeof(message));
    }
    printf("CLIENT %d DISCONNECTED \n", index);
    fflush(stdout);
    shutdown(socket, 2);
    closesocket(socket);
    clients[index].socket = INVALID_SOCKET;
    printf("CLIENT %d HANDLER THREAD TERMINATED \n", index);
    fflush(stdout);
    ExitThread(0);
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

// удаление клиента по номеру
void *kickClient(int number) {
    WaitForSingleObject(&mutex, INFINITE);
    for (int i = 0; i < numberOfClients; i++) {
        if (clients[i].number == number) {
            // сокет закрывается для чтения и для записи
            shutdown(clients[i].socket, 2);
            closesocket(clients[i].socket);
            clients[i].socket = INVALID_SOCKET;
            printf("CLIENT %d TERMINATED \n", number);
            break;
        }
    }
    ReleaseMutex(&mutex);
}