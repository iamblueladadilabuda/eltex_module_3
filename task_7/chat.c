#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <mqueue.h>
#include <unistd.h>

#define QUEUE "/queue_chat"
#define MAX_MSG 128
#define END_PRIORITY 10 // приоритет для завершения обмена

int main() 
{
    mqd_t queue;
    struct mq_attr attr = {0, 10, MAX_MSG, 0};
    char buf[MAX_MSG];
    unsigned int prio;
    int i = 1;

    // Создаем и открываем очереди
    mq_unlink(QUEUE);

    queue = mq_open(QUEUE, O_CREAT | O_RDWR, 0666, &attr);
    if (queue == (mqd_t)-1) 
    { 
        perror("mq_open queue"); 
        exit(1); 
    }

    printf("Отправляйте сообщения\n");
    
    printf("%d: ", i);
    if (!fgets(buf, MAX_MSG, stdin)) 
    {
        return -1;
    }
    
    if (mq_send(queue, buf, strlen(buf) + 1, prio) == -1) 
    {
        perror("mq_send");
        return -1;
    }
    
    i += 1;
    
    // Начинаем пинг-понг отправки
    while (1) 
    {        
        // Получаем ответ
        if (mq_receive(queue, buf, MAX_MSG, &prio) == -1) 
        {
            perror("mq_receive");
            break;
        }
        printf("Вам отправили сообщение: %s", buf);
        
        if (prio == END_PRIORITY) 
        {
            break;
        }
        
        printf("%d: ", i);
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

        if (mq_send(queue, buf, strlen(buf) + 1, prio) == -1) 
        {
            perror("mq_send");
            break;
        }
        
        if (prio == END_PRIORITY) 
        {
            break;
        }
        
        if (i == 1) i++;
        else i--;
    }

    mq_close(queue);
    mq_unlink(QUEUE);
    printf("Чат завершён\n");
    return 0;
}

