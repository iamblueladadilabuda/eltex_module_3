#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <errno.h>
#include <limits.h>

#define MAX_LINE_LENGTH 50 // Максимальная длина одной строки
#define MAX_NUMS_PER_LINE 10 // Максимальное количество чисел в строке

const char *SEM_NAME = "/file_sem";

void Error(const char* text)
{
    perror(text);
    exit(1);
}

// Основная логика производителя
void Producer(const char *filename) 
{
    FILE *file = fopen(filename, "a+");
    if(file == NULL) Error("fopen");
    
    char random_line[MAX_LINE_LENGTH] = ""; 
    int num_count = rand() % MAX_NUMS_PER_LINE + 1; // Количество чисел в строке
        
    // Генерируем случайную строку с числами
    for (int i = 0; i < num_count; i++) 
    {
        snprintf(random_line + strlen(random_line),
        sizeof(random_line)-strlen(random_line), "%d ", rand() % 100); // Случайные целые числа
    }
        
    fprintf(file, "%s\n", random_line);
    fflush(file);
    sleep(rand()%2); // Задержка перед следующей итерацией
    
    fclose(file);
}

// Логика обработки строки потребителем
void Process_Line(char *line) 
{
    char *token = strtok(line, " ");
    int min_value = INT_MAX, max_value = INT_MIN;
    
    while (token != NULL) 
    {
        int value = atoi(token);
        if (value > max_value) max_value = value;
        if (value < min_value && value != 0) min_value = value;
        token = strtok(NULL, " ");
    }
    
    printf("Минимум: %d Максимум: %d\n", min_value, max_value);
}

// Основная логика потребителя
void Consumer(const char *filename) 
{
    FILE *file = fopen(filename, "r");
    if(file == NULL) Error("fopen(file)");
    
    size_t len = strlen(filename) + 8; 
    char filename_2[len];
    snprintf(filename_2, len, "%s_lseek", filename);
    FILE *file_lseek = fopen(filename_2, "r+");
    if (file_lseek == NULL) 
    {
        file_lseek = fopen(filename_2, "w+"); // создаём пустой файл, если его ещё нет
        if (file_lseek == NULL) Error("fopen(file_lseek)");
    }
    char buffer_lseek[10];
    fgets(buffer_lseek, sizeof(buffer_lseek), file_lseek);
    int length = atoi(buffer_lseek);
    if (length > 0) fseek(file, length, SEEK_SET);

    char buffer[MAX_LINE_LENGTH]; // Чтение строки из файла
    while(fgets(buffer, sizeof(buffer), file)) 
    {
        Process_Line(buffer);
    }
    
    int current_pos = ftell(file);
    
    rewind(file_lseek);  
    fprintf(file_lseek, "%d\n", current_pos); // записываем новую позицию
    fclose(file_lseek);
    
    fclose(file);
    
}

int main() 
{
    const char *filename = "file";
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

    FILE *file = fopen(filename, "a");
    if (!file) 
    {
        perror("fopen");
        sem_close(sem);
        sem_unlink(SEM_NAME);
        exit(1);
    }
    fclose(file);

    pid_t pid = fork();
    
    switch(pid)
    {
        case -1:
            // Ошибка
            perror("fork");
            fclose(file);
            sem_close(sem);
            sem_unlink(SEM_NAME);
            exit(1);
        
        case 0:
            // Дочерний процесс
            while (1) 
            {
                sem_wait(sem);
                
                Consumer(filename);

                sem_post(sem);
                sleep(2);
            }
            
            
            fclose(file);
            sem_close(sem);
            exit(0);
        
        default:
            // Родительский процесс
            srand(time(NULL) ^ getpid());

            while (1)
            {
                sem_wait(sem);

                Producer(filename);

                sem_post(sem);
                sleep(1);
            }

            wait(NULL);

            fclose(file);
            sem_close(sem);
            sem_unlink(SEM_NAME);
    }

    return 0;
}


