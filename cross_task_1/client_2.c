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
#define MAX_CLIENTS 100

int sockfd;
socklen_t client_size;
struct sockaddr_in cliaddr;
uint16_t src_port;
char prev_message[MAX_PACKET_SIZE] = "";
uint16_t prev_src_port = 0;

// Структура сообщения
typedef struct Message
{
    uint16_t src;
    int count;
}Mes;

Mes messages[MAX_CLIENTS];
int client_count = 0;

// Структура для UDP
typedef struct Udp_Hdr 
{
    uint16_t source; // Порт источника
    uint16_t dest; // Порт получателя
    uint16_t len; // Длина
    uint16_t check; // Контрольная сумма
}UDP;

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
void Send_Message(char *payload) 
{
    char packet[MAX_PACKET_SIZE];

    struct sockaddr_in dst_addr;
    memset(&dst_addr, 0, sizeof(dst_addr));
    dst_addr.sin_family = AF_INET;
    dst_addr.sin_port = htons(DST_PORT);

    struct udphdr *udph = (struct udphdr *)(packet);

     size_t data_len = strlen(payload);
     size_t packet_len = + sizeof(struct udphdr) + data_len;

     udph->uh_sport = htons(DST_PORT);
     udph->uh_dport = htons(src_port);
     udph->uh_ulen = htons(data_len + sizeof(struct udphdr));
     udph->uh_sum = 0;

     memcpy(packet + sizeof(struct udphdr), payload, data_len);

     if (sendto(sockfd, packet, packet_len, 0, (struct sockaddr *)&dst_addr, sizeof(dst_addr)) != (ssize_t)packet_len) 
     {
        perror("sendto");
        close(sockfd);
     }

     if (strcmp(payload, "exit\\n") == 0) End();
}

// Изменение текста сообщения
void Change_Message(const char *buf, size_t received_bytes) 
{
    const UDP *udph = (UDP*)(buf + 20);
    char *payload = (char*)(buf + 20 + sizeof(UDP));
    size_t payload_len = received_bytes - (20 + sizeof(UDP));
    src_port = ntohs(udph->source);
    if (src_port == DST_PORT) return;
    
    printf("Всего клиентов: %d\n", client_count);
    
    if (strcmp(prev_message, payload) != 0 || 
        strcmp(prev_message, payload) == 0 && src_port != prev_src_port)
    {
        strcpy(prev_message, payload);
        prev_src_port = src_port;
        
        printf("Сообщение: ");
        fwrite(payload, payload_len, 1, stdout);

        int flag = 0;
        for (int i = 0; i < client_count; i++)
        {
            if (messages[i].src == src_port)
            {
                if (strcmp("exit", payload))
                {
                    messages[i] = messages[client_count];
                    client_count--;
                    return;
                }
                flag = 1;
                
                messages[i].count += 1;
                
                char new_payload[sizeof(payload) + sizeof(messages[i].count) + 1];
                payload[strcspn(payload, "\n")] = '\0';
                snprintf(new_payload, sizeof(new_payload), "%s %d\n", payload, messages[i].count);
                
                Send_Message(new_payload);
            }
        }
        
        if (flag == 0 && client_count + 1 < MAX_CLIENTS)
        {
            messages[client_count].src = src_port;
            messages[client_count].count = 1;
            
            char new_payload[sizeof(payload) + sizeof(messages[client_count].count) + 1];
            payload[strcspn(payload, "\n")] = '\0';
            snprintf(new_payload, sizeof(new_payload), "%s %d\n", payload, messages[client_count].count);
                
            client_count++;
                
            Send_Message(new_payload);
        }
    }
}

int main() 
{    
    memset(&cliaddr, 0, sizeof(cliaddr));
    cliaddr.sin_family = AF_INET;
    cliaddr.sin_port = htons(DST_PORT);
    cliaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
    if (sockfd < 0) Error("socket");

    fcntl(sockfd, F_SETFL, O_NONBLOCK);

    printf("Ожидание входящих пакетов...\n");

    char buf[MAX_PACKET_SIZE];
    while (1) 
    {
        ssize_t received_bytes = recvfrom(sockfd, buf, MAX_PACKET_SIZE, 0, (struct sockaddr *)&cliaddr, &client_size);
        if (received_bytes > 0)
        {
            Change_Message(buf, received_bytes);
        }
    }

    return 0;
}

    return 0;
}

