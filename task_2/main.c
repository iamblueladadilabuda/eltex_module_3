#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAXLINE 1024
#define MAXARGS 64

// Функция для разбора введённой строки на аргументы
void Parse_Input(char *line, char **argv, int *argc) 
{
    *argc = 0;
    char *token = strtok(line, " \t\n");
    while (token != NULL && *argc < MAXARGS - 1) 
    {
        argv[(*argc)++] = token;
        token = strtok(NULL, " \t\n");
    }
    argv[*argc] = NULL;
}

int main() 
{
    char line[MAXLINE];
    char *argv[MAXARGS];
    int argc;

    while (1) 
    {
        printf("task_2> ");
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) 
        {
            printf("\n");
            break;  // Ctrl+D
        }

        if (line[0] == '\n') continue;  // игнор пустой строки

        Parse_Input(line, argv, &argc);
        if (argc == 0) continue;

        pid_t pid = fork();
        
        switch(pid)
        {
            case -1:
                // Обработка ошибки
                perror("fork");
                exit(EXIT_FAILURE);
            
            case 0:
                // Дочерний процесс

                // 1) Если программа начинается с ./ или /, пытаемся запустить так
                if (argv[0][0] == '/' || (argv[0][0] == '.' && argv[0][1] == '/')) 
                {
                    execv(argv[0], argv);
                } 
                else 
                {
                    // 2) Ищем в PATH
                    execvp(argv[0], argv);
                }
                // Если мы сюда дошли — значит exec не удался
                fprintf(stderr, "Команда не найдена: %s\n", argv[0]);
                _exit(EXIT_SUCCESS);
            
            default:
                // Родитель ждёт завершения дочернего
                int rv;
                wait(&rv);
        }
    }

    return 0;
}

