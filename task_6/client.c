#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

#define SERVER_QUEUE_KEY 'S'
#define MAX_MSG_SIZE 256

// Структура сообщения
typedef struct Message 
{
    long mtype; // Тип сообщения (приоритет)
    char mtext[MAX_MSG_SIZE]; // Содержимое сообщения
} Message;

int client_id = -1;

void Read_Messages(int client_number) // Чтение сообщений из очереди
{
    Message msg_read;
    while (1) 
    {
        if (msgrcv(client_id, &msg_read, sizeof(msg_read.mtext), 0, 0) == -1) 
        {
            perror("msgrcv");
            exit(1);
        }
        
        if (msg_read.mtype != client_number)
        {
            printf("Получено от %ld: %s\n", msg_read.mtype, msg_read.mtext);
        }
    }
}

int main(int argc, char *argv[]) 
{
    if (argc != 2 || atoi(argv[1]) <= 10) 
    {
        fprintf(stderr, "Используйте: %s <номер клиента > 10>\n", argv[0]);
        exit(1);
    }

    int client_number = atoi(argv[1]); // Читаем номер клиента 

    // Генерируем уникальный ключ для очереди сообщений
    key_t server_key = ftok(".", SERVER_QUEUE_KEY);
    if (server_key == -1) 
    {
        perror("ftok");
        exit(1);
    }

    // Подключение к существующей очереди сообщений
    client_id = msgget(server_key, 0666 | IPC_CREAT);
    if (client_id == -1) 
    {
        perror("msgget");
        exit(1);
    }

    printf("Клиент %d запустился. Вводите сообщения:\n", client_number);

    // Параллельно читаем входящие сообщения
    pid_t child_pid = fork();
    if (child_pid == 0) 
    {
        Read_Messages(client_number); 
        exit(0);
    }

    // Основной процесс продолжает отправлять сообщения
    char input[MAX_MSG_SIZE];
    while (1) 
    {
        fgets(input, MAX_MSG_SIZE, stdin);
        input[strlen(input)-1] = '\0'; // Удаляем завершающий символ перевода строки

        // Формируем и отправляем сообщение
        Message msg;
        msg.mtype = client_number;
        strncpy(msg.mtext, input, MAX_MSG_SIZE);

        if (msgsnd(client_id, &msg, strlen(msg.mtext)+1, 0) == -1) 
        {
            perror("msgsnd");
        }
        
        if (msgsnd(client_id, &msg, strlen(msg.mtext)+1, 0) == -1) 
        {
            perror("msgsnd");
        }
        
        if (strcmp(input, "shutdown") == 0) 
        {
            break;
        }
    }

    // Завершаем работу клиента
    kill(child_pid, SIGTERM); // Останавливаем дочерний процесс чтения сообщений
    wait(NULL); // Дождёмся завершения процесса
    printf("Клиент %d завершил работу\n", client_number);

    return 0;
}
