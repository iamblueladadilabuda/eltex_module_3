#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <signal.h>

#define SERVER_QUEUE_KEY 'S'
#define MAX_MSG_SIZE 256
#define MAX_CLIENTS 10
#define SERVER_ID 10

// Структура сообщения
typedef struct Message 
{
    long mtype; // Тип сообщения (приоритет)
    char mtext[MAX_MSG_SIZE]; // Содержимое сообщения
} Message;

int server_id = -1;
int clients[MAX_CLIENTS]; // Массив номеров активных клиентов
int clients_count = 0; // Количество активных клиентов

int Find_Client(int client_number) // Поиск индекса клиента по номеру
{
    for (int i = 0; i < clients_count; i++) 
    {
        if (clients[i] == client_number) 
        {
            return i;
        }
    }
    return -1;
}

void Remove_Client(int client_number) // Удаление клиента из списка
{
    int index = Find_Client(client_number);
    if (index >= 0) 
    {
        for (int i = index; i < clients_count - 1; i++) 
        {
            clients[i] = clients[i + 1];
        }
        clients_count--;
        printf("Клиент %d отключился\n", client_number);
    }
}

void Add_Client(int client_number) // Добавление клиента в список
{
    if (Find_Client(client_number) < 0 && clients_count < MAX_CLIENTS) 
    {
        clients[clients_count++] = client_number;
        printf("Подключён клиент %d\n", client_number);
    }
}

void Cleanup(int signum) // Обработчик сигнала завершения
{
    if (server_id != -1) 
    {
        msgctl(server_id, IPC_RMID, NULL); // Удаляем очередь
    }
    printf("\nСервер остановлен\n");
    exit(0);
}

int main() 
{
    signal(SIGINT, Cleanup); // Установка обработчика сигнала Ctrl+C

    // Генерируем уникальный ключ для очереди сообщений
    key_t server_key = ftok(".", SERVER_QUEUE_KEY);
    if (server_key == -1) 
    {
        perror("ftok");
        exit(1);
    }

    // Создаем очередь сообщений
    server_id = msgget(server_key, 0666 | IPC_CREAT);
    if (server_id == -1) 
    {
        perror("msgget");
        exit(1);
    }

    printf("Сервер готов принимать сообщения\n");

    Message msg;

    while (1) 
    {
        // Ждем получение сообщения
        if (msgrcv(server_id, &msg, sizeof(msg.mtext), 0, 0) == -1) 
        {
            perror("msgrcv");
            exit(0);
        }

        int sender = msg.mtype; // Получатель сообщения 
        Add_Client(sender); // Регистрация клиента, если он ранее не зарегистрирован

        printf("Сообщение от %d: %s\n", sender, msg.mtext);

        if (strcmp(msg.mtext, "shutdown") == 0) // Проверка на завершение работы
        { 
            Remove_Client(sender);
            continue;
        }

        // Пересылаем сообщение всем активным клиентам кроме отправившего
        for (int i = 0; i < clients_count; i++) 
        {
            if (clients[i] == sender) continue; // Пропускаем отправляющего клиента
            
            // Формирование и отправка сообщения
            Message new_msg;
            new_msg.mtype = sender; // Адресат сообщения
            strncpy(new_msg.mtext, msg.mtext, MAX_MSG_SIZE);

            if (msgsnd(server_id, &new_msg, strlen(new_msg.mtext)+1, 0) == -1) 
            {
                perror("msgsnd");
            }
        }
    }

    Cleanup(0);
    return 0;
}

