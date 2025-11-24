#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ctype.h>

int Is_Number(const char *str) // Проверка число ли эта строка
{
    if (str == NULL || *str == '\0') // Пустая строка
    {
        return 0;
    }

    char *endptr;
    // Попытка перевести в вещественное число
    strtod(str, &endptr);
    
    // Если указатель на конец строки, то это число
    return (*endptr == '\0');
}

int Is_Double(const char *str) // Проверка на вещественное число
{
    for (int i = 0; str[i] != '\0'; i++)
    {
        if (str[i] == '.') // Если в строке есть ".", то вещественное
        {
            return 1;
        }
    }
    return 0;
}

void Process_Args(char *args[], int argc, int start) // Обработка
{
    for (int i = start; i < argc; i += 2) 
    {
        if (Is_Number(args[i])) 
        {
            if (Is_Double(args[i]))
            {
                // Вывод вещественного числа
                double val = strtod(args[i], NULL);
                printf("%s --> %.3f\n", args[i], val * 2);
            }
            else
            {
                // Вывод целого числа
                int val = atoi(args[i]);
                printf("%s --> %d\n", args[i], val * 2);
            }
        } 
        else 
        {
            // Вывод строки
            printf("%s\n", args[i]);
        }
    }
}

int main(int argc, char *argv[]) 
{
    if (argc < 2) {
        // Нет аргументов
        printf("No arguments\n");
        return 0;
    }

    pid_t pid = fork();

    switch(pid) 
    {
        case -1:
            // Обработка ошибки
            perror("fork");
            exit(EXIT_FAILURE);

        case 0:
            // Потомок
            printf("CHILD: PID - %d\n", getpid());
            printf("CHILD: PPID - %d\n", getppid());

            // Дочерний процесс: обрабатывает нечётные позиции
            Process_Args(argv, argc, 2);

            _exit(EXIT_SUCCESS);

        default: 
            // Родитель
            printf("PARENT: PID - %d\n", getpid());
            printf("PARENT: CHILD PID - %d\n", pid);

            // Родительский процесс: обрабатывает чётные позиции
            Process_Args(argv, argc, 1);

            wait(NULL);

            exit(EXIT_SUCCESS);
    }
}
