#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

int count_sig, fd;

void listener(int sig)
{
    const char *output;
    
    if (count_sig >= 2)
    {
        close(fd);
        exit(0);
    }
    
    if (sig == SIGINT || sig == SIGQUIT)
    {
        output = "Сигнал обработан!\n";
        write(fd, output, sizeof(output));
        if (write(fd, output, sizeof(output)) != sizeof(output)) 
        {
            perror("Ошибка записи в файл (сигнал)");
            close(fd);
            exit(1);
        }
    }
    
    if (sig == SIGINT) 
    {
        count_sig++;
    }
}

int main()
{
    int count[1] = { 0 };
    int c = 0;
    
    remove("file");
    fd = open("file", O_CREAT | O_WRONLY | O_APPEND);
    if (fd < 0) 
    {
        perror("Ошибка открытия файла");
        return 1;
    }
    
    while (1)
    {
        count[0] = ++c;
        
        write(fd, count, sizeof(count));
        if (write(fd, count, sizeof(count)) != sizeof(count)) 
        {
            perror("Ошибка записи в файл");
            close(fd);
            return 1;
        }
        
        signal(SIGINT, listener);
        signal(SIGQUIT, listener);
        
        sleep(1);
    }
    
    close(fd);
    return 0;
}
