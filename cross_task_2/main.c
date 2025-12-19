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
#define MAX_PATH 1024

// Структура драйвера
typedef struct Driver
{
    pid_t pid;
    int task_timer;
}Driver;

// Структура аргументов для Send_Task
typedef struct Task_Data {
    pid_t driver_pid;
    int task_timer;
}Task_Data;

struct Driver** drivers = NULL; // Массив указателей на драйверы
int drivers_count; // Количество драйверов
struct pollfd fds[MAX_DRIVERS + 1];
int nfds = 2;
int fd_write[MAX_DRIVERS];
int i_write;
int fd_read;

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

// Отправка задачи драйверу 
void *Send_Task(void * arg)
{
    // Распаковка аргументов
    Task_Data* data = (Task_Data*)arg;
    pid_t driver_pid = data->driver_pid;
    int task_time = data->task_timer;
    
    Driver* driver = Find_Driver(driver_pid);
    if (!driver) 
    {
        printf("Error: Driver %d not found\n", driver_pid);
        free(data); 
        pthread_exit(NULL);
        return NULL;
    }
    
    // Создаем FIFO для отправки команды драйверу
    char driver_fifo[MAX_PATH];
    snprintf(driver_fifo, MAX_PATH, "/tmp/driver_fifo_%d", driver_pid);
    
    // Проверяем, существует ли драйвер (процесс)
    if (kill(driver_pid, 0) == -1 && errno == ESRCH) 
    {
        printf("Error: Driver %d process not found\n", driver_pid);
        free(data); 
        pthread_exit(NULL);
        return NULL;
    }
    
    // Открываем FIFO драйвера для записи команды
    int fd = open(driver_fifo, O_WRONLY);
    if (fd == -1) 
    {
        printf("Error: Cannot connect to driver %d\n", driver_pid);
        free(data); 
        pthread_exit(NULL);
        return NULL;
    }
    
    // Формируем команду
    char command[MAX_LINE];
    snprintf(command, sizeof(command), "send_task %d\n", task_time);
    
    // Отправляем команду драйверу
    if (write(fd, command, strlen(command)) <= 0) 
    {
        printf("Error: Failed to send task to driver %d\n", driver_pid);
    } 
    
    close(fd);
    
    // Создаем FIFO для получения результата
    char result_fifo[MAX_PATH];
    snprintf(result_fifo, MAX_PATH, "/tmp/result_fifo_%d", driver_pid);
    
    // Ждем немного, чтобы драйвер успел создать FIFO для результата
    usleep(50000);
    
    // Открываем FIFO для чтения результата
    int result_fd = open(result_fifo, O_RDONLY);
    if (result_fd != -1) 
    {
        char buffer[256];
        ssize_t bytes_read;
        
        // Читаем первый результат (подтверждение начала задачи)
        bytes_read = read(result_fd, buffer, sizeof(buffer) - 1);
        if (bytes_read > 0) 
        {
            buffer[bytes_read] = '\0';
            printf("%s", buffer);
        }
        close(result_fd);
        
        // Помечаем драйвер как занятый сразу
        driver->task_timer = task_time;
        
        // Ждем завершения задачи
        while (driver->task_timer != 0)
        {
            sleep(1);
            driver->task_timer -= 1;
        }
        sleep(1);
        
        // Читаем финальный результат
        result_fd = open(result_fifo, O_RDONLY);
        if (result_fd != -1) 
        {
            bytes_read = read(result_fd, buffer, sizeof(buffer) - 1);
            if (bytes_read > 0) 
            {
                buffer[bytes_read] = '\0';
                printf("%s", buffer);
            }
            close(result_fd);
        }
    }
    
    free(data); 
    pthread_exit(NULL);
}

// Создание драйвера 
pid_t Create_Driver()
{
    Driver* new_driver = malloc(sizeof(Driver));
    if (!new_driver) Error("malloc: create_driver");
    
    memset(new_driver, 0, sizeof(Driver));
    new_driver->task_timer = 0;
    
    // Создаем пару FIFO для драйвера
    char driver_fifo[MAX_PATH];
    char result_fifo[MAX_PATH];
    
    new_driver->pid = fork();
    if (new_driver->pid == 0) 
    {        
        // Создаем свои FIFO
        snprintf(driver_fifo, MAX_PATH, "/tmp/driver_fifo_%d", getpid());
        snprintf(result_fifo, MAX_PATH, "/tmp/result_fifo_%d", getpid());
        
        // Создаем FIFO для получения команд
        if (mkfifo(driver_fifo, 0666) == -1 && errno != EEXIST) Error("mkfifo driver");
        
        // Создаем FIFO для отправки результатов
        if (mkfifo(result_fifo, 0666) == -1 && errno != EEXIST) Error("mkfifo result");
        
        // Отправляем сигнал о готовности
        int main_fifo = open(FIFONAME, O_WRONLY);
        if (main_fifo != -1) 
        {
            char ready_msg[100];
            snprintf(ready_msg, sizeof(ready_msg), "Create driver <%d>\n\n", getpid());
            write(main_fifo, ready_msg, strlen(ready_msg));
            close(main_fifo);
        }
        
        while (1) 
        {
            // Открываем свой FIFO для чтения команд
            int fd = open(driver_fifo, O_RDONLY);
            if (fd == -1) 
            {
                perror("open driver fifo (child)");
                sleep(1);
                continue;
            }
            
            char buffer[MAX_LINE];
            ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
            close(fd);
            
            if (bytes_read > 0) 
            {
                buffer[bytes_read] = '\0';
                
                // Парсим команду
                char *commands[MAX_COMMANDS];
                Parse_Command(buffer, commands);
                
                if (strcmp(commands[0], "send_task") == 0 && commands[1] != NULL) 
                {
                    int task_time = atoi(commands[1]);
                    
                    if (task_time <= 0) 
                    {
                        // Отправляем ошибку
                        int result_fd = open(result_fifo, O_WRONLY);
                        if (result_fd != -1) 
                        {
                            write(result_fd, "Error: invalid task time\n", 25);
                            close(result_fd);
                        }
                        continue;
                    }
                    
                    // Обновляем статус драйвера (в локальной памяти процесса)
                    new_driver->task_timer = task_time;
                    
                    // Отправляем подтверждение о начале задачи
                    int result_fd = open(result_fifo, O_WRONLY);
                    if (result_fd != -1) 
                    {
                        char msg[100];
                        snprintf(msg, sizeof(msg), "driver <%d> working %ds\n\n", getpid(), task_time);
                        write(result_fd, msg, strlen(msg));
                        close(result_fd);
                    }
                    
                    // ВЫПОЛНЯЕМ ЗАДАЧУ (блокирующий вызов)
                    sleep(task_time);
                    
                    // Задача выполнена
                    new_driver->task_timer = 0;
                    
                    // Отправляем результат
                    result_fd = open(result_fifo, O_WRONLY);
                    if (result_fd != -1) {
                        char msg[100];
                        snprintf(msg, sizeof(msg), "\nDriver %d: Task completed\n", getpid());
                        write(result_fd, msg, strlen(msg));
                        close(result_fd);
                    }
                }
            }
        }
    } 
    else if (new_driver->pid < 0) 
    {
        perror("fork");
        free(new_driver);
        exit(1);
    }

    // Сохраняем информацию о драйвере
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
    close(fd_read);
    for (int i = 0; i < nfds; i++) close(fd_write[i]);
    unlink(FIFONAME);
    
    // Освободить память
    for (int i = 0; i < drivers_count; i++)
    {
        free(drivers[i]);
    }
    free(drivers);
    while(waitpid(-1, NULL, WNOHANG) > 0) {}
}

int main() 
{
    signal(SIGCHLD, Sigchld_Handler);
    
    char line[MAX_LINE];
    char buffer[MAX_LINE];
    
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
            }
            else if (strcmp(commands[0], "send_task") == 0 && commands[1] != NULL && commands[2] != NULL)
            {
                pid_t driver_pid = atoi(commands[1]);
                int task_time = atoi(commands[2]);
                
                if (task_time <= 0) {
                    printf("Error: Task time must be positive\n");
                } else {
                    // Вызываем новую функцию Send_Task
                    Task_Data* data = malloc(sizeof(Task_Data)); // Выделяем память под аргумент
                    if (!data) Error("malloc");

                    data->driver_pid = driver_pid;
                    data->task_timer = atoi(commands[2]);
                    
                    pthread_t send;
                    if (pthread_create(&send, NULL, Send_Task, data)) 
                    {
                        perror("pthread_create");
                        free(data);
                        return 1;
                    }
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
    
    return 0;
}
