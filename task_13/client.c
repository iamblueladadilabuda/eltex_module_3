#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/stat.h>

void error(const char *msg) 
{
    perror(msg);
    exit(0);
}

// Функция для отправки файла
void sendFile(int sockfd, const char* filename) 
{
    struct stat st;
    // Получение размера файла
    if(stat(filename, &st) != 0) error ("ERROR opening file\n");

    // Длина имени файла
    size_t filename_len = strlen(filename);
    write(sockfd, &filename_len, sizeof(size_t));

    // Имя файла
    write(sockfd, filename, filename_len);

    // Размер файла
    off_t fileSize = st.st_size;
    write(sockfd, &fileSize, sizeof(off_t));

    // Чтение и отправка файла кусочками
    FILE *fp = fopen(filename, "rb");
    if(fp == NULL) error ("ERROR opening file\n");

    unsigned char buffer[1024];
    ssize_t bytes_read;
    while((bytes_read = fread(buffer, 1, sizeof(buffer), fp)) > 0) 
    {
        write(sockfd, buffer, bytes_read);
    }

    fclose(fp);
}

int main(int argc, char *argv[])
{
    int my_sock, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    char buff[1024];
    
    printf("TCP DEMO CLIENT\n");
    
    if (argc < 3) 
    {
        fprintf(stderr, "usage %s hostname port\n",
        argv[0]);
        exit(0);
    }

    // Извлечение порта
    portno = atoi(argv[2]);

    // Шаг 1 - создание сокета
    my_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (my_sock < 0) error("ERROR opening socket");

    // Извлечение хоста
    server = gethostbyname(argv[1]);
    if (server == NULL) 
    {
    fprintf(stderr, "ERROR, no such host\n");
    exit(0);
    }
    
    // Заполнение структуры serv_addr
    bzero((char*) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char*)server->h_addr, (char *)&serv_addr.sin_addr.s_addr,
    server->h_length);
    
    // Установка порта
    serv_addr.sin_port = htons(portno);

    // Шаг 2 - установка соединения
    if (connect(my_sock, (struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0)
        error("ERROR connecting");
    
    // Пример отправки файла example.txt
    sendFile(my_sock, "example.txt");
    n = recv(my_sock, &buff[0], sizeof(buff) - 1, 0);
    buff[n] = 0;
    printf("S=>C: %s", buff);
    
    // Шаг 3 - чтение и передача сообщений
    while ((n = recv(my_sock, &buff[0], sizeof(buff) - 1, 0)) > 0)
    {        
        // Ставим завершающий ноль в конце строки
        buff[n] = 0;
        
        // Выводим на экран
        printf("S=>C: %s", buff);
        
        // Читаем пользовательский ввод с клавиатуры
        printf("S<=C: ");
        fgets(&buff[0], sizeof(buff) - 1, stdin);

        // Проверка на "quit"
        if (!strcmp(&buff[0], "quit\n")) 
        {
            // Корректный выход
            printf("Exit...\n");
            close(my_sock);
            return 0;
        }
    
        // Передаем строку клиента серверу
        send(my_sock, &buff[0], strlen(&buff[0]), 0);
    }
    
    printf("Exit...\n");
    close(my_sock);
    return 0;
}
