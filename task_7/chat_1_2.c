#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <mqueue.h>
#include <unistd.h>

#define QUEUE1 "/queue_chat_1to2"
#define QUEUE2 "/queue_chat_2to1"
#define MAX_MSG 128
#define END_PRIORITY 10 // приоритет для завершения обмена

void Chat_1(struct mq_attr attr)
{
    mqd_t q_send, q_recv;
    char buf[MAX_MSG];
    unsigned int prio;

    // Создаем и открываем очереди
    mq_unlink(QUEUE1);
    mq_unlink(QUEUE2);

    q_send = mq_open(QUEUE1, O_CREAT | O_WRONLY, 0666, &attr);
    if (q_send == (mqd_t)-1) 
    { 
        perror("mq_open send"); 
        exit(1); 
    }

    q_recv = mq_open(QUEUE2, O_CREAT | O_RDONLY, 0666, &attr);
    if (q_recv == (mqd_t)-1) 
    { 
        perror("mq_open recv"); 
        exit(1); 
    }

    // Начинаем пинг-понг отправки
    while (1) 
    {
        printf("Вы: ");
        if (!fgets(buf, MAX_MSG, stdin)) 
        {
            break;
        }

        // Для продолжения: приоритет 5
        prio = END_PRIORITY;
        if (strcmp(buf, "exit\n") == 0) 
        {
            prio = END_PRIORITY;
        } 
        else 
        {
            prio = 5;
        }

        if (mq_send(q_send, buf, strlen(buf) + 1, prio) == -1) 
        {
            perror("mq_send");
            break;
        }
        
        if (prio == END_PRIORITY) 
        {
            break;
        }
        
        // Получаем ответ
        if (mq_receive(q_recv, buf, MAX_MSG, &prio) == -1) 
        {
            perror("mq_receive");
            break;
        }

        printf("Собеседник: %s\n", buf);
        if (prio == END_PRIORITY) 
        {
            break;
        }
    }

    mq_close(q_send);
    mq_close(q_recv);
    mq_unlink(QUEUE1);
    mq_unlink(QUEUE2);
}

void Chat_2(struct mq_attr attr)
{
    mqd_t q_send, q_recv;
    char buf[MAX_MSG];
    unsigned int prio;

    // Открываем очереди (предполагается, что chat1 их уже создал)
    q_recv = mq_open(QUEUE1, O_RDONLY);
    if (q_recv == (mqd_t)-1) 
    {
        perror("mq_open recv");
        exit(1);
    }
    
    q_send = mq_open(QUEUE2, O_WRONLY);
    if (q_send == (mqd_t)-1) 
    {
        perror("mq_open send");
        exit(1);
    }

    while (1) 
    {
        if (mq_receive(q_recv, buf, MAX_MSG, &prio) == -1) 
        {
            perror("mq_receive");
            break;
        }
        printf("Собеседник: %s\n", buf);

        if (prio == END_PRIORITY) 
        {
            break;
        }
        
        printf("Вы: ");
        if (!fgets(buf, MAX_MSG, stdin)) 
        {
            break;
        }
        
        prio = END_PRIORITY;
        if (!(strcmp(buf, "exit\n") == 0)) 
        {
            prio = 5;
        } 

        if (mq_send(q_send, buf, strlen(buf)+1, prio) == -1) 
        {
            perror("mq_send");
            break;
        }

        if (prio == END_PRIORITY) 
        {
            break;
        }
    }

    mq_close(q_send);
    mq_close(q_recv);
}

int main(int argc, char* argv[]) 
{
    if (argc != 2)
    {
        printf("Укажите номер чата!");
        return 1;
    }
    
    struct mq_attr attr = {0, 10, MAX_MSG, 0};
    printf("Чат запущен\n"); 

    if (atoi(argv[1]) == 1) Chat_1(attr);
    else Chat_2(attr);

    printf("Чат завершён\n");
    return 0;
}

