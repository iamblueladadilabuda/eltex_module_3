#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <limits.h>

#define MAX_LINE_LENGTH 50 // Максимальная длина одной строки
#define MAX_NUMS_PER_LINE 10 // Максимальное количество чисел в строке
#define SHARED_MEMORY_NAME "shared_memory"

const char *SEM_NAME = "/file_sem";

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
            if (shared_mem->numbers[i] > max_val) max_val = shared_mem->numbers[i];
            if (shared_mem->numbers[i] < min_val) min_val = shared_mem->numbers[i];
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

int main() 
{
    int shm_id;
    Shared_Data *shared_mem;
    
    sem_t *sem = sem_open(SEM_NAME, O_CREAT | O_EXCL, 0644, 1);
    if (sem == SEM_FAILED) 
    {
        if (errno == EEXIST) 
        {
            sem_unlink(SEM_NAME);
            sem = sem_open(SEM_NAME, O_CREAT | O_EXCL, 0644, 1);
        }
        if (sem == SEM_FAILED) Error("sem_open");
    }
  
    // Разделяемая память
    
    shm_id = shm_open(SHARED_MEMORY_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_id == -1) Error("shm_open");
    
    ftruncate(shm_id, sizeof(Shared_Data));
    
    shared_mem = mmap(NULL, sizeof(Shared_Data), PROT_READ | PROT_WRITE, MAP_SHARED, shm_id, 0);
    if(shared_mem == MAP_FAILED) 
    {
        perror("mmap");
        close(shm_id);
        exit(1);
    }
    
    signal(SIGINT, End); // Регистрация обработчика сигнала
    
    pid = fork();
    
    switch(pid)
    {
        case -1:
            // Ошибка
            Error("fork");
        
        case 0:
            // Дочерний процесс
            while (end) 
            {
                usleep(3);
                sem_wait(sem);
                Process_Line(shared_mem);
                sem_post(sem);
            }
        
        default:
            // Родительский процесс
            while (end)
            {
                sem_wait(sem);
                Numbers_Generation(shared_mem);
                sem_post(sem);

                sleep(2); 
                    
                sem_wait(sem);    
                if (shared_mem->min_value != INT_MAX && shared_mem->max_value != INT_MIN) 
                {
                    if (end) printf("Минимальное значение: %d\nМаксимальное значение: %d\n", shared_mem->min_value, shared_mem->max_value);
                    processed_count++;
                }
                sem_post(sem);
            }
    }
    
    if (pid > 0) 
    {
        wait(NULL); // Ждем завершения дочернего процесса
    }

    // Удаление разделяемой памяти
    munmap(NULL, sizeof(Shared_Data));
    shm_unlink(SHARED_MEMORY_NAME);

    return 0;
}
