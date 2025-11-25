#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <signal.h>
#include <time.h>
#include <limits.h>
#include <errno.h>
#include <sys/sem.h>
#include <semaphore.h>

#define MAX_LINE_LENGTH 50 // Максимальная длина одной строки
#define MAX_NUMS_PER_LINE 10 // Максимальное количество чисел в строке
#define SHM_SIZE sizeof(Shared_Data)

// Структура разделяемой памяти
typedef struct Shared_Data 
{
    int count; // Количество элементов
    int numbers[100]; // Массив чисел
    int min_value; // Минимальное значение
    int max_value; // Максимальное значение
}Shared_Data;

int processed_count = 0; // Общее количество обработанных наборов
pid_t pid;
int end = 1;

void Error(const char* text)
{
    perror(text);
    exit(1);
}

// Основная логика родительского процесса
void Numbers_Generation(Shared_Data * shared_mem) 
{    
    char random_line[MAX_LINE_LENGTH];
    int num_count = rand() % MAX_NUMS_PER_LINE + 2; // Количество чисел в строке
        
    // Генерируем случайную строку с числами
    for (int i = 0; i < num_count; i++) 
    {
         shared_mem->numbers[i] = rand() % 1000; // Заполняем массив случайными значениями
    }
    shared_mem->count = num_count;
}

// Логика обработки строки дочерним процессом
void Process_Line(Shared_Data * shared_mem) 
{
    if (shared_mem->count > 0) 
    {
        int min_val = INT_MAX;
        int max_val = INT_MIN;
                    
        for (int i = 0; i < shared_mem->count; i++) 
        {
            if (shared_mem->numbers[i] < min_val) min_val = shared_mem->numbers[i];
            if (shared_mem->numbers[i] > max_val) max_val = shared_mem->numbers[i];
        }
                    
        shared_mem->min_value = min_val;
        shared_mem->max_value = max_val;
    }
}

void End(int signum) 
{
    if (signum == SIGINT) 
    {
        if (pid != 0)
        {
            printf("\nПрограмма завершила процесс\n");
            printf("Обработано наборов данных: %d\n", processed_count);
        }
        end = 0;
    }
}

union Semun 
{
    int val; // значение для SETVAL
    struct semid_ds *buf; // буферы для  IPC_STAT, IPC_SET
    unsigned short *array; // массивы для GETALL, SETALL
                           // часть, особенная для Linux:
    struct seminfo *__buf; // буфер для IPC_INFO
};

int main() 
{
    // IPC
    key_t key;
    int shm_id;
    Shared_Data *shared_mem;
    int semid;
    union Semun arg;
    struct sembuf lock_res = {0, -1, SEM_UNDO};
    struct sembuf rel_res = {0, 1, SEM_UNDO};
  
    // Разделяемая память
    key = ftok(".", 'S');
    if(key == -1) 
    {
        perror("ftok");
        exit(1);
    }
    
    shm_id = shmget(key, SHM_SIZE, IPC_CREAT | 0777);
    if (shm_id == -1)
    {
        perror("shmget");
        exit(1);
    }
    
    shared_mem = (Shared_Data *)shmat(shm_id, NULL, 0); // Привязываем адрес к процессу
    
    // Создать набор из 1 семафора
    semid = semget(key, 1, 0666 | IPC_CREAT);
    if(semid == -1) Error("semget");
    
    // Установить в семафоре № 0 (Контроллер ресурсов) значение "1"
    arg.val = 1;
    int current_val = semctl(semid, 0, SETVAL, arg);
    if (current_val == -1) Error("semctl(SETVAL)");
    
    signal(SIGINT, End); // Регистрация обработчика сигнала

    pid = fork();
    
    switch(pid)
    {
        case -1:
            // Ошибка
            perror("fork");
            exit(1);
        
        case 0:
            // Дочерний процесс
            while (end) 
            {
                sleep(3);
                current_val = semctl(semid, 0, GETVAL, arg);
                if (current_val == -1) Error("semctl(GETVAL)");
                
                if (current_val == 1) 
                {
                    if (semop(semid, &lock_res, 1) == -1)
                    { 
                        perror("semop:lock_res");  
                    }
                    Process_Line(shared_mem);
                }
                
                current_val = semctl(semid, 0, GETVAL, arg);
                if (current_val == -1) Error("semctl(GETVAL)");
                
                // Разблокировать ресурс
                if (current_val == 0)
                {
                    semop(semid, &rel_res, 1);
                }
            }
        
        default:
            // Родительский процесс
            while (end)
            {
                sleep(1);
                current_val = semctl(semid, 0, GETVAL, arg);
                if (current_val == -1) Error("semctl(GETVAL)");
                
                if (current_val == 1) 
                {
                    if (semop(semid, &lock_res, 1) == -1)
                    { 
                        perror("semop:lock_res");  
                    }
                    Numbers_Generation(shared_mem);
                }
                
                current_val = semctl(semid, 0, GETVAL, arg);
                if (current_val == -1) Error("semctl(GETVAL)");
                
                // Разблокировать ресурс
                if (current_val == 0)
                {
                    semop(semid, &rel_res, 1);
                }
                
                sleep(2);
                
                current_val = semctl(semid, 0, GETVAL, arg);
                if (current_val == -1) Error("semctl(GETVAL)");
                
                if (current_val == 1) 
                {
                    if (semop(semid, &lock_res, 1) == -1)
                    { 
                        perror("semop:lock_res");  
                    }
                    
                    if (shared_mem->min_value != INT_MAX && shared_mem->max_value != INT_MIN) 
                    {
                        if (end) printf("Минимальное значение: %d\nМаксимальное значение: %d\n", shared_mem->min_value, shared_mem->max_value);
                        processed_count++;
                    }
                }
                
                current_val = semctl(semid, 0, GETVAL, arg);
                if (current_val == -1) Error("semctl(GETVAL)");
                
                // Разблокировать ресурс
                if (current_val == 0)
                {
                    semop(semid, &rel_res, 1);
                }                      
            }
    }
    
    if (pid > 0) 
    {
        wait(NULL); // Ждем завершения дочернего процесса
    }

    // Удаление разделяемой памяти
    shmdt(shared_mem);
    shmctl(shm_id, IPC_RMID, NULL);

    return 0;
}

