#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <pthread.h>

// Максимальный размер принимаемого пакета
#define MAX_PACKET_SIZE 65535
#define DST_PORT 51000

int sockfd;
socklen_t client_size;
struct sockaddr_in cliaddr;
uint16_t src_port;

// Структура для IPv4
typedef struct Ip_Hdr 
{
    unsigned char   ihl : 4, // Длина заголовка (в словах по 32 бита)
                    version : 4; // Версия IP
    unsigned char   tos; // Тип обслуживания
    unsigned short  tot_len; // Общая длина пакета
    unsigned short  id; // Уникальный идентификатор пакета
    unsigned short  frag_off; // Нет фрагментов
    unsigned char   ttl; // Время жизни
    unsigned char   protocol; // Транспортный протокол (UDP)
    unsigned short  check; // Контрольная сумма будет рассчитана позже
    unsigned long   saddr; // Источник
    unsigned long   daddr; // Получатель
}IP;

// Структура для UDP
typedef struct Udp_Hdr 
{
    uint16_t source; // Порт источника
    uint16_t dest; // Порт получателя
    uint16_t len; // Длина
    uint16_t check; // Контрольная сумма
}UDP;

// Функция отображения содержимого пакета
void Dump_Packet(const char *buffer, size_t len) 
{
    int j = 0;
    for (size_t i = 0; i < len; i++) 
    {
        if ((j++ % 16) == 0) printf("\n%04zx: ", i);
        printf("%02hhx ", buffer[i]);
    }
    printf("\n");
}

// Функция анализа принятого пакета
void Decode_Message(const char *buffer, size_t len) 
{
    const UDP *udph = (UDP*)(buffer + 20); // Смещение UDP заголовка после IP заголовка
    const char *payload = (char*)(buffer + 20 + sizeof(UDP));
    size_t payload_len = len - (20 + sizeof(UDP));
    
    src_port = ntohs(udph->source);

    printf("\nПолучено новое сообщение!\n");
    printf("Заголовок UDP:\n");
    printf("Порт отправителя: %hu\n", ntohs(udph->source));
    printf("Порт получателя: %hu\n", ntohs(udph->dest));
    printf("Длина пакета: %hu байт\n", ntohs(udph->len));
    printf("Контрольная сумма: %hu\n", udph->check);
    printf("Дамп:");
    Dump_Packet(payload, payload_len);
    printf("Сообщение: ");
    fwrite(payload, payload_len, 1, stdout);
    printf("\n");
}

// Завершение программы
void End() 
{
    close(sockfd);
    printf("Завершаем работу.\n");
    exit(0);
}

// Завершение программы с ошибкой
void Error(const char * text) 
{
    perror(text);
    close(sockfd);
    exit(1);
}

// Логика формирования и отправки пакета
void* Send_Message(void* arg) 
{
    char sendline[1000];
    char packet[MAX_PACKET_SIZE];

    struct sockaddr_in dst_addr;
    memset(&dst_addr, 0, sizeof(dst_addr));
    dst_addr.sin_family = AF_INET;
    dst_addr.sin_port = htons(DST_PORT);

    struct ip *iph = (struct ip*)packet;
    struct udphdr *udph = (struct udphdr *)(packet + sizeof(struct ip));

    while (1) 
    {
        if (!fgets(sendline, sizeof(sendline), stdin)) 
        {
            close(sockfd);
            break;
        }

        size_t data_len = strlen(sendline);
        size_t packet_len = sizeof(struct ip) + sizeof(struct udphdr) + data_len;

        iph->ip_hl = 5;
        iph->ip_v = 4;
        iph->ip_tos = 0;
        iph->ip_len = htons(packet_len);
        iph->ip_id = rand();
        iph->ip_off = 0;
        iph->ip_ttl = 64;
        iph->ip_p = IPPROTO_UDP;
        iph->ip_sum = 0;
        iph->ip_src.s_addr = inet_addr("127.0.0.1"); 
        iph->ip_dst.s_addr = inet_addr("127.0.0.1"); 

        udph->uh_sport = htons(src_port);
        udph->uh_dport = htons(DST_PORT);
        udph->uh_ulen = htons(data_len + sizeof(struct udphdr));
        udph->uh_sum = 0;

        memcpy(packet + sizeof(struct ip) + sizeof(struct udphdr), sendline, data_len);

        if (sendto(sockfd, packet, packet_len, 0, (struct sockaddr *)&dst_addr, sizeof(dst_addr)) != (ssize_t)packet_len) 
        {
            perror("sendto");
            close(sockfd);
            break;
        }

        if (strcmp(sendline, "exit\\n") == 0) End();
    }
}

// Поток для приёма сообщений
void* Read_Message(void* arg) 
{
    char buf[MAX_PACKET_SIZE];
    while (1) 
    {
        ssize_t received_bytes = recvfrom(sockfd, buf, MAX_PACKET_SIZE, 0, (struct sockaddr *)&cliaddr, &client_size);
        if (received_bytes > 0) 
        {
            Decode_Message(buf, received_bytes);
        }
    }
}

int main() 
{
    pthread_t read, write;
    
    memset(&cliaddr, 0, sizeof(cliaddr));
    cliaddr.sin_family = AF_INET;
    cliaddr.sin_port = htons(DST_PORT);
    cliaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
    if (sockfd < 0) Error("socket");

    fcntl(sockfd, F_SETFL, O_NONBLOCK);
    
    if (bind(sockfd, (struct sockaddr *)&cliaddr, sizeof(cliaddr)) < 0) 
        Error("bind");

    printf("Ожидание входящих пакетов...\n");

    pthread_create(&read, NULL, Read_Message, NULL);
    pthread_create(&write, NULL, Send_Message, 0);

    pthread_join(read, NULL);
    pthread_join(write, NULL);

    return 0;
}

