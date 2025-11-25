#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

#define MAX_CMDLINE 1024 // Длина строки
#define MAX_ARGS 128 // Максимальное количество аргументов в строке
#define MAX_CMDS 64 // Максимальное количество разбиваемых аргументов

int Split_Pipeline(char *cmdline, char **cmds) // Парсит команду по символу |
{
    int i = 0;
    cmds[i] = strtok(cmdline, "|");
    while (cmds[i] != NULL) 
    {
        i++;
        cmds[i] = strtok(NULL, "|");
    }
    return i;
}

void Parse_Command(char *cmd, char **argv, char **infile, char **outfile, int *append_out, int *append_in) // Парсит одну подкоманду
{
    *infile = NULL;
    *outfile = NULL;
    *append_out = 0;
    *append_in = 0;

    char *token;
    int argc = 0;

    token = strtok(cmd, " \t\n");
    while (token != NULL) 
    {
        if (strcmp(token, "<") == 0) // Перезапись ввода 
        {
            token = strtok(NULL, " \t\n");
            *infile = token;
            *append_in = 0;
        } 
        else if (strcmp(token, "<<") == 0) // Дозапись ввода
        {
            token = strtok(NULL, " \t\n");
            *infile = token;
            *append_in = 1; 
        } 
        else if (strcmp(token, ">") == 0) // Перезапись вывода
        {
            token = strtok(NULL, " \t\n");
            *outfile = token;
            *append_out = 0;
        } 
        else if (strcmp(token, ">>") == 0) // Дозапись вывода
        {
            token = strtok(NULL, " \t\n");
            *outfile = token;
            *append_out = 1;
        } 
        else 
        {
            argv[argc++] = token;
        }
        token = strtok(NULL, " \t\n");
    }
    argv[argc] = NULL;
}

int main() 
{
    char line[MAX_CMDLINE];

    while (1) 
    {
        printf("task_4> ");
        
        // CTRL+D
        if (!fgets(line, sizeof(line), stdin)) 
        {
            printf("\n");
            break;
        }
        
        // Игнорирование пустой строки
        if (line[0] == '\n') 
        {
            continue;
        }

        // Разбиение по |
        char *cmds[MAX_CMDS];
        int n = Split_Pipeline(line, cmds);
        if (n == 0) // Нет команд
        {
            continue;
        }

        int i;
        int pipefds[2 * (MAX_CMDS - 1)];

        // Создаем необходимые каналы
        for (i = 0; i < n - 1; i++) 
        {
            if (pipe(pipefds + i * 2) < 0) 
            {
                perror("pipe");
                exit(EXIT_FAILURE);
            }
        }

        for (i = 0; i < n; i++) 
        {
            char *argv[MAX_ARGS];
            char *infile = NULL, *outfile = NULL;
            int append_out = 0, append_in = 0;

            // Парсим команду
            Parse_Command(cmds[i], argv, &infile, &outfile, &append_out, &append_in);

            pid_t pid = fork();
            
            if (pid == 0) 
            {
                if (i > 0) // Не первый процесс
                {
                    if (dup2(pipefds[(i - 1) * 2], STDIN_FILENO) < 0) 
                    {
                        perror("dup2 stdin");
                        exit(EXIT_FAILURE);
                    }
                } 
                else if (infile) // Ввод
                {
                    int fd_in;
                    if (append_in) 
                    {
                        // Дозапись файла
                        fd_in = open(infile, O_RDWR | O_APPEND);
                        
                        if (fd_in < 0) 
                        {
                            perror(infile);
                            exit(EXIT_FAILURE);
                        }
                    } 
                    else 
                    {
                        // Перезапись файла
                        fd_in = open(infile, O_RDONLY);
                        
                        if (fd_in < 0) 
                        {
                            perror(infile);
                            exit(EXIT_FAILURE);
                        }
                    }
                    
                    if (dup2(fd_in, STDIN_FILENO) < 0) 
                    {
                        perror("dup2 infile");
                        exit(EXIT_FAILURE);
                    }
                    
                    close(fd_in);
                }

                if (i < n - 1) // Не последний процесс
                {
                    if (dup2(pipefds[i * 2 + 1], STDOUT_FILENO) < 0) 
                    {
                        perror("dup2 stdout");
                        exit(EXIT_FAILURE);
                    }
                } 
                else if (outfile) // Вывод
                {
                    int fd_out;
                    
                    if (append_out) 
                    {
                        // Дозаписываем в файл
                        fd_out = open(outfile, O_WRONLY | O_CREAT | O_APPEND, 0644);
                    } 
                    else 
                    {
                        // Перезаписываем в файл
                        fd_out = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    }
                    
                    if (fd_out < 0) 
                    {
                        perror(outfile);
                        exit(EXIT_FAILURE);
                    }
                    
                    if (dup2(fd_out, STDOUT_FILENO) < 0) 
                    {
                        perror("dup2 outfile");
                        exit(EXIT_FAILURE);
                    }
                    
                    close(fd_out);
                }

                for (int j = 0; j < 2 * (n - 1); j++) // Дочерний процесс закрывает каналы
                {
                    close(pipefds[j]);
                }

                execvp(argv[0], argv);
                exit(EXIT_FAILURE);
            }
            
            else if (pid < 0)
            {
                perror("fork");
                exit(EXIT_FAILURE);
            }
        }

        // Родитель закрывает все каналы
        for (i = 0; i < 2 * (n - 1); i++)
        {
            close(pipefds[i]);
        }
        
        // Ожидание завершения всех дочерних процессов
        for (i = 0; i < n; i++)
        {
            wait(NULL);
        }
    }

    return 0;
}

