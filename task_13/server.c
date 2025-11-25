#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

int nclients = 0; // Количество активных пользователей
void dostuff(int); // Функция обслуживания подключившихся пользователей

void error(const char *msg) // Функция обработки ошибок
{
    perror(msg);
    exit(1);
}

int myfunc(int a, int b, char c) // Функция обработки данных
{
    switch (c)
    {
        case '+':
            return a + b;
        case '-':
            return a - b;
        case '*':
            return a * b;
        case '/':
            if (b != 0) return a / b;
            else return 2147483647;
        default:
            return 2147483647;
    }
}

void receiveFile(int newsockfd) // Функция для приёма файла
{
    size_t filename_len;
    read(newsockfd, &filename_len, sizeof(size_t));

    char filename[filename_len + 1];
    read(newsockfd, filename, filename_len);
    filename[filename_len] = '\0';

    off_t fileSize;
    read(newsockfd, &fileSize, sizeof(off_t));

    // Создание нового файла для записи полученных данных
    FILE *fp = fopen(filename, "wb");
    if(fp == NULL) error ("ERROR opening file\n");

    unsigned char buffer[1024];
    ssize_t totalReceived = 0;
    while(totalReceived < fileSize) 
    {
        ssize_t bytesRead = read(newsockfd, buffer, sizeof(buffer));
        fwrite(buffer, 1, bytesRead, fp);
        totalReceived += bytesRead;
    }

    fclose(fp);
    char buf[1024];
    snprintf(buf, sizeof(buf), "Получил файл '%s' размером %ld байт.\n", filename, fileSize); // Преобразование результата в строку
    printf("%s", buf);
    send(newsockfd, buf, strlen(buf), 0);
}

// Печать количества активных пользователей
void printusers()
{
    if (nclients) 
    {
        printf("%d user online\n\n", nclients);
    }
    else 
    {
        printf("No users online\n\n");
    }
}

int main(int argc, char *argv[])
{
    char buffer[1024]; // Буфер для различных нужд
    int sockfd, newsockfd; // Дескрипторы сокетов
    int portno; // Номер порта
    int pid; // ID номера потока
    socklen_t clilen; // Размер адреса клиента типа socklen_t
    struct sockaddr_in serv_addr, cli_addr; // Структура сокета сервера и клиента
    struct addrinfo hints, *res; // Информация о хосте
    char hostname[NI_MAXHOST]; // Имя хоста
    char service[NI_MAXSERV]; // Служба (порт)

    printf("TCP SERVER DEMO\n");

    // Ошибка в случае если мы не указали порт
    if (argc < 2) 
    {
        fprintf(stderr, "ERROR, no port provided\n");
        exit(1);
    }

    // Шаг 1 - создание сокета
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) error("ERROR opening socket");

    // Шаг 2 - связывание сокета с локальным адресом
    memset(&serv_addr, 0, sizeof(serv_addr)); // Обнуляем всю структуру
    portno = atoi(argv[1]); // Читаем порт из аргументов
    serv_addr.sin_family = AF_INET; // Используем IPv4
    serv_addr.sin_addr.s_addr = INADDR_ANY; // Прослушиваем любые интерфейсы
    serv_addr.sin_port = htons(portno); // Устанавливаем порт сервера

    if (bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR on binding");

    // Шаг 3 - ожидание подключений, размер очереди - 5
    listen(sockfd, 5);
    clilen = sizeof(cli_addr);
    
    // Шаг 4 - извлекаем сообщение из очереди (цикл извлечения запросов на подключение)
    while (1)
    {   
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if(newsockfd < 0) error("ERROR on accept");
        receiveFile(newsockfd); 
        nclients++;

        // Вывод сведений о клиенте
        getnameinfo((struct sockaddr*)&cli_addr, clilen, hostname, NI_MAXHOST, 
            service, NI_MAXSERV, NI_NUMERICHOST | NI_NUMERICSERV);

        printf("+%s [%s] new connect!\n", hostname, inet_ntoa(cli_addr.sin_addr));
        printusers();

        pid = fork();
        if (pid < 0) error("Error on fork");
        if (pid == 0) 
        {
            close(sockfd);
            dostuff(newsockfd);
            exit(0);
        }
        else close(newsockfd); // Родитель закрывает новый сокет
    }

    close(sockfd);
    return 0;
}

void dostuff(int sock)
{
    int bytes_recv; // Размер принятого сообщения
    int a, b; // Переменные для myfunc
    char buffer[20*1024];
    #define str1 "Enter function (+, -, *, /)\r\n"
    #define str2 "Enter 1 parameter\r\n"
    #define str3 "Enter 2 parameter\r\n"
    #define err "ERROR entering\r\n"

    // Отправляем клиенту сообщение
    write(sock, str1, sizeof(str1)-1);

    // Обработка первого параметра
    bytes_recv = recv(sock, buffer, sizeof(buffer), 0);
    if (bytes_recv <= 0) error("ERROR reading from socket");
    char c = buffer[0];

    // Отправляем клиенту сообщение
    write(sock, str2, sizeof(str2)-1);

    bytes_recv = recv(sock, buffer, sizeof(buffer), 0);
    if (bytes_recv <= 0) error("ERROR reading from socket");
    a = atoi(buffer); // Преобразование первого параметра в int
    
    // Отправляем клиенту сообщение
    write(sock, str3, sizeof(str3)-1);

    // Обработка первого параметра
    bytes_recv = recv(sock, buffer, sizeof(buffer), 0);
    if (bytes_recv <= 0) error("ERROR reading from socket");
    b = atoi(buffer); // Преобразование второго параметра в int

    float res = myfunc(a, b, c); // Вызов пользовательской функции
    if (res != 2147483647)
    {
        snprintf(buffer, sizeof(buffer), "%f\r\n", res); // Преобразование результата в строку
        send(sock, buffer, strlen(buffer), 0);
    }
    else 
    {
        send(sock, err, strlen(err), 0);
    }

    nclients--; // Уменьшаем счетчик активных клиентов
    printf("-disconnect\n");
    printusers();
    return;
}
