#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/poll.h>
#include <errno.h>
#include <sys/stat.h>
#include <signal.h>
#include <pthread.h>

#define MAX_DRIVERS 1000
#define MAX_LINE 1024
#define MAX_COMMANDS 4
#define FIFONAME "./fifo"
#define TIMEOUT_MS (-1)

// Структура драйвера
typedef struct Driver
{
    pid_t pid;
    int task_timer;
}Driver;

// Структура аргументов для Send_Task
typedef struct Task_Data 
{
    Driver* driver;
    int task_timer;
} Task_Data;

struct Driver** drivers = NULL; // Массив указателей на драйверы
int drivers_count; // Количество драйверов
struct pollfd fds[MAX_DRIVERS + 1];
int nfds = 2;
int fd_write[MAX_DRIVERS];
int i_write;

// Обработчик ошибок
void Error(const char* text)
{
    perror(text);
    exit(1);
}

// Парсинг строки
void Parse_Command(char *line, char **commands)
{
    char *token;
    token = strtok(line, " \t\n");
    int cmd = 0;
    
    while (token != NULL && cmd < MAX_COMMANDS)
    {
        commands[cmd++] = token;
        token = strtok(NULL, " \t\n");
    }
    
    commands[cmd] = NULL;
}

// Поиск нужного драйвера по PID
Driver* Find_Driver(pid_t pid)
{
    for (int i = 0; i < drivers_count; i++)
    {
        if (drivers[i]->pid == pid)
        {
            i_write = i;
            return drivers[i];
        }
    }
    
    return NULL;
}

// Создание драйвера
pid_t Create_Driver()
{
    Driver* new_driver = malloc(sizeof(Driver)); // Новый драйвер
    if (!new_driver) Error("malloc: create_driver");
    
    new_driver->pid = fork();
    if (new_driver->pid == 0) 
    {
        // Код дочернего процесса
        mkfifo(FIFONAME, 0666); // Именованный канал
        fd_write[nfds - 2] = open(FIFONAME, O_WRONLY); // Для записи
        if (fd_write[nfds - 2] == -1) Error("open fd_write");
        dup2(fd_write[nfds - 2], STDOUT_FILENO);
        
        fds[nfds].fd = STDOUT_FILENO;
        fds[nfds].events = POLLIN;
        nfds++;
        
        exit(0);
    } 
    else if (new_driver->pid < 0) 
    {
        perror("fork");
        free(new_driver);
        exit(1);
    }

    // Добавление в массив драйверов
    drivers = realloc(drivers, sizeof(Driver*) * (drivers_count + 1));
    if (!drivers) 
    {
        perror("realloc");
        free(new_driver);
        exit(1);
    }

    drivers[drivers_count++] = new_driver;
    return new_driver->pid;
}

// Задача на время для драйвера
void* Send_Task(void* arg)
{
    // Распаковка аргументов
    Task_Data* data = (Task_Data*)arg;
    Driver* driver = data->driver;
    int task_timer = data->task_timer;
    
    // Логика задачи для драйвера
    driver->task_timer = task_timer;
    char command[MAX_LINE];
    snprintf(command, sizeof(command), "driver <%d> working %ds\n\n", driver->pid, task_timer);
    ssize_t written = write(fd_write[i_write], command, strlen(command));
    
    while (driver->task_timer != 0)
    {
        sleep(1);
        driver->task_timer -= 1;
    }
    
    free(data); // Освобождаем память, выделенную под аргументы
    pthread_exit(NULL);
}

// Вывод статуса одного драйвера
int Get_Status(const struct Driver* driver)
{
    if (!driver) return -1; // Такого нет
    else if (driver->task_timer != 0) return 1; // Занят
    else return 0; // Свободен
}

// Вывод всех статусов драйверов
void Get_Drivers()
{
    if (drivers_count == 0) // Такого нет
    {
        printf("No driver has been created yet\n");
        return;
    }
    
    // Вывод всех драйверов: PID + статус
    for (int i = 0; i < drivers_count; i++)
    {
        printf("<%d>: ", drivers[i]->pid);
        if (Get_Status(drivers[i]))
        {
            printf("Busy <%d>\n", drivers[i]->task_timer);
        }
        else
        {
            printf("Avaible\n");
        }
    }
}

// Обработка сигнала для корректного завершения дочернего процесса
void Sigchld_Handler(int signum) 
{
    while(waitpid(-1, NULL, WNOHANG) > 0) {}
}

int main() 
{
    signal(SIGCHLD, Sigchld_Handler);
    
    char line[MAX_LINE];
    char buffer[MAX_LINE];
    int fd_read;
    
    // Проверяем создание fifo-канала
    if ((mkfifo(FIFONAME, 0666) != 0) && (errno != EEXIST)) 
        Error("mkfifo");

    // Открыли канал на чтение
    fd_read = open(FIFONAME, O_RDONLY | O_NONBLOCK); 
    if (fd_read == -1) Error("open fd_read");
    
    // Подготовили первый элемент массива
    fds[0].fd = fd_read;
    fds[0].events = POLLIN; 
    
    fds[1].fd = STDIN_FILENO;
    fds[1].events = POLLIN;

    printf("Приложение запущено. Ожидание команд...\n");
    while (1) 
    {
        // Опрашиваем события
        int ret = poll(fds, nfds, TIMEOUT_MS);

        if (ret < 0) // Ошибка
        {
            Error("Ошибка в poll()");
        }
        else if (ret == 0) // Ничего не произошло
        {
            continue;
        }
        
        // Проверка fifo
        for (int i = 2; i < nfds; i++)
        {
            if (fds[i].revents & POLLOUT) 
            {
                fds[i].revents = 0; 
            }
            else if (fds[i].revents & POLLERR) 
            {
                printf("Ошибка дескриптора %d\n", fds[i].fd);
                fds[i].revents = 0;
            }
            else if (fds[i].revents & POLLHUP) 
            {
                printf("Соединение разорвано для дескриптора %d\n", fds[i].fd);
                fds[i].revents = 0;
            }
        }
        
        if (fds[0].revents & POLLIN) 
        {
            // Чтение результата из канала
            ssize_t bytes_read = read(fd_read, buffer, sizeof(buffer));
            if (bytes_read > 0) 
            {
                buffer[bytes_read] = '\0';
                printf("%s", buffer);
            }
        }
        
        // Проверка ввода с клавиатуры
        if (fds[1].revents & POLLIN) 
        {
            fflush(stdout);
                
            // CTRL+D
            if (!fgets(line, sizeof(line), stdin)) 
            {
                printf("\n");
                break;
            }
                
            // Игнорирование пустой строки
            if (line[0] == '\n') continue;
             
            // Парсинг строки    
            char *commands[MAX_COMMANDS];
            Parse_Command(line, commands);
            
            // Ищем и запускаем команду, которую ввёл пользователь    
            if (strcmp(commands[0], "create_driver") == 0)
            {
                pid_t pid = Create_Driver();
                printf("Create driver <%d>\n", pid);
            }
            else if (strcmp(commands[0], "send_task") == 0 && commands[1] != NULL && commands[2] != NULL)
            {
                Driver *driver = Find_Driver(atoi(commands[1]));
                if (Get_Status(driver) == -1)
                {
                    printf("<%s>: Unknown PID driver\n", commands[1]);
                }
                else if (Get_Status(driver) == 0)
                {                    
                    Task_Data* data = malloc(sizeof(Task_Data)); // Выделяем память под аргумент
                    if (!data) Error("malloc");

                    data->driver = driver;
                    data->task_timer = atoi(commands[2]);

                    pthread_t send;
                    if (pthread_create(&send, NULL, Send_Task, data)) 
                    {
                        perror("pthread_create");
                        free(data);
                        return 1;
                    }
                }
                else
                {
                    printf("Busy <%d>\n", driver->task_timer);
                }
            }
            else if (strcmp(commands[0], "get_status") == 0 && commands[1] != NULL)
            {
                Driver* driver = Find_Driver(atoi(commands[1]));
                int res = Get_Status(driver);
                if (res == -1) printf("<%d>: Unknown PID\n", atoi(commands[1]));
                else if (res) printf("Busy <%d>\n", driver->task_timer);
                else printf("Avaible\n");
            }
            else if (strcmp(commands[0], "get_drivers") == 0)
            {
                Get_Drivers();
            }
            else
            {
                printf("%s: Unknown command!\n", commands[0]);
            }
                
            printf("\n");
        }
       
    }
    
    close(fd_read);
    for (int i = 0; i < nfds; i++) close(fd_write[i]);
    unlink(FIFONAME);
    
    // Освободить память
    for (int i = 0; i < drivers_count; i++)
    {
        free(drivers[i]);
    }
    free(drivers);
    
    return 0;
}
