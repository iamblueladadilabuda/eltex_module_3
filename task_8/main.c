#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <semaphore.h>
#include <pthread.h>
#include <limits.h>

#define MAX_LINE_LENGTH 50 // Максимальная длина одной строки
#define MAX_NUMS_PER_LINE 10 // Максимальное количество чисел в строке

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

union Semun 
{
    int val; // значение для SETVAL
    struct semid_ds *buf; // буферы для  IPC_STAT, IPC_SET
    unsigned short *array; // массивы для GETALL, SETALL
                           // часть, особенная для Linux:
    struct seminfo *__buf; // буфер для IPC_INFO
};

int main(int argc, char *argv[])
{
    // IPC
    pid_t pid;
    key_t key;
    int semid;
    union Semun arg;
    struct sembuf lock_res = {0, -1, SEM_UNDO};
    struct sembuf rel_res = {0, 1, SEM_UNDO};
    const char *filename = argv[2]; // Имя файла
    
    // Остальное
    if(argc < 3) 
    {
        printf("Использование программы: %s название процесса файл\n", argv[0]);
        exit(0);
    }
    
    // Если нет файла, создаём
    if (!fopen(filename, "a"))
    {
        FILE *file = fopen(filename, "a");
        fclose(file);
    }
  
    // Семафоры
    key = ftok(filename, getpid());
    if(key == -1) Error("ftok");

    // Создать набор из 1 семафора
    semid = semget(key, 1, 0666 | IPC_CREAT);
    if(semid == -1) Error("semget");
    
    // Установить в семафоре № 0 (Контроллер ресурсов) значение "1"
    arg.val = 1;
    int current_val = semctl(semid, 0, SETVAL, arg);
    if (current_val == -1) Error("semctl(SETVAL)");

    if (strcmp(argv[1], "producer") == 0) 
    {
        printf("Производитель запущен с файлом <%s>\n", filename);
        
        while (1)
        {
            //sleep(rand()%3);
            // Попытаться заблокировать ресурс (семафор номер 0)
            current_val = semctl(semid, 0, GETVAL, arg);
            if (current_val == -1) Error("semctl(GETVAL)");
            
            if (current_val == 1) 
            {
                if (semop(semid, &lock_res, 1) == -1)
                { 
                    perror("semop:lock_res");  
                }
                    
                Producer(filename);
                printf("Создана запись в файле <%s>\n", filename);
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
    else if (strcmp(argv[1], "consumer") == 0) 
    {
        printf("Потребитель запущен с файлом <%s>\n", filename);
        
        while (1)
        {
            sleep(rand()%3);
            current_val = semctl(semid, 0, GETVAL, arg);
            if (current_val == -1) Error("semctl(GETVAL)");
                    
            // Попытаться заблокировать ресурс (семафор номер 0)
            if (current_val == 1)
            {
                if (semop(semid, &lock_res, 1) == -1)
                { 
                    perror("semop:lock_res"); 
                }
      
                Consumer(filename);
            }
            
            current_val = semctl(semid, 0, GETVAL, arg);
            if (current_val == -1) Error("semctl(GETVAL)");

            // Разблокировать ресурс
            if (current_val == 0)
            {
                semop(semid, &rel_res, 1);
            }
            
            sleep(rand()%3);
        }
    } 
    else 
    {
        Error("Неправильно введено название процесса. Используйте 'producer' или 'consumer'.\n");
    }
   
    return 0;
}

