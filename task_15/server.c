#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/poll.h>

#define TIMEOUT_MS (-1)
#define MAX_CLIENTS 100

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
    struct pollfd fds[MAX_CLIENTS + 1]; // Массив структур для poll(), плюс слушающий сокет
    int nfds = 1; // Текущее количество активных файловых дескрипторов

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
    
    // Подготовили первый элемент массива для прослушивающего сокета
    fds[0].fd = sockfd;
    fds[0].events = POLLIN; 

    // Цикл обслуживания клиентов
    while (1)
    {
        // Опрашиваем события
        int ret = poll(fds, nfds, TIMEOUT_MS);

        if (ret < 0) 
        {
            error("Ошибка в poll()");
        }
        else if (ret == 0)
        {
            continue;
        }
        else
        {
            // Поочередно проверяем все файловые дескрипторы
            for (int i = 0; i < nfds; i++) 
            {
                if (fds[i].revents & POLLIN) 
                {
                    if (fds[i].fd == sockfd) 
                    {
                        // Приняли новое соединение
                        newsockfd = accept(sockfd, (struct sockaddr*)&cli_addr, &clilen);
                        if (newsockfd < 0) error("Ошибка при приеме соединения");
                        receiveFile(newsockfd);                         

                        // Добавляем новый сокет в массив для дальнейшего слежения
                        if (nclients >= MAX_CLIENTS) 
                        {
                            close(newsockfd); // Больше клиентов не принимаем
                            continue;
                        }
                        
                        getnameinfo((struct sockaddr*)&cli_addr, clilen, hostname, NI_MAXHOST, 
                             service, NI_MAXSERV, NI_NUMERICHOST | NI_NUMERICSERV);

                        fds[nfds].fd = newsockfd;
                        fds[nfds].events = POLLIN;
                        nfds++;
                        nclients++;

                        printf("+%s [%s] new connect!\n", hostname, inet_ntoa(cli_addr.sin_addr));
                        printusers();
                    }
                    else
                    {
                        dostuff(newsockfd);
                        nclients--; // Уменьшаем счетчик активных клиентов
                        printf("-disconnect\n");
                        printusers();
                        close(newsockfd);
                        fds[i] = fds[nfds - 1];
                        nfds--;
                    }
                }
            }
        }
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
    send(sock, str1, strlen(str1), 0);

    // Обработка первого параметра
    bytes_recv = recv(sock, buffer, sizeof(buffer), 0);
    if (bytes_recv <= 0) error("ERROR reading from socket");
    if (!strcmp(&buffer[0], "quit\n")) return;
    char c = buffer[0];

    // Отправляем клиенту сообщение
    send(sock, str2, strlen(str2), 0);

    bytes_recv = recv(sock, buffer, sizeof(buffer), 0);
    if (bytes_recv <= 0) error("ERROR reading from socket");
    if (!strcmp(&buffer[0], "quit\n")) return;
    a = atoi(buffer); // Преобразование первого параметра в int
    
    // Отправляем клиенту сообщение
    send(sock, str3, strlen(str1), 0);

    // Обработка первого параметра
    bytes_recv = recv(sock, buffer, sizeof(buffer), 0);
    if (bytes_recv <= 0) error("ERROR reading from socket");
    if (!strcmp(&buffer[0], "quit\n")) return;
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

    return;
}
