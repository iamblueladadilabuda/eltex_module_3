#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <pthread.h>

int sockfd;
socklen_t client;
struct sockaddr_in cliaddr, cliaddr_2;

void End()
{
    close(sockfd);
    printf("Чат завершён\n");
    exit(0);
}

void* Send_Message(void* arg) 
{
    ssize_t n;
    char sendline[1000];
    
    while(1)
    {
        if (!fgets(sendline, sizeof(sendline), stdin)) 
        {
            close(sockfd);
            break;
        }

        size_t sendlen = strlen(sendline);
        if (sendto(sockfd, sendline, sendlen, 0, (struct sockaddr *)&cliaddr, sizeof(cliaddr)) != (ssize_t)sendlen) 
        {
            perror("sendto");
            close(sockfd);
            break;
        }
        
        if (strcmp(sendline, "exit\n") == 0) End();
    }
}

void* Read_Message(void* arg) 
{
    ssize_t n;
    char recvline [1000];
    
    while(1)
    {
        client = sizeof(cliaddr);     
        n = recvfrom(sockfd, recvline, sizeof(recvline) - 1, 0, (struct sockaddr *)&cliaddr, &client);
        while (n >= 0)
        {
            if (strcmp(recvline, "exit\n") == 0) End();
            recvline[n] = '\0';
            printf("%d: %s", ntohs(cliaddr.sin_port), recvline);
            n = recvfrom(sockfd, recvline, sizeof(recvline) - 1, 0, (struct sockaddr *)&cliaddr, &client);
        }
    }
}

int main(int argc, char **argv)
{
    pthread_t read, write;
    
    if (argc != 2) 
    {
        fprintf(stderr, "Usage: %s <IP address>\n", argv[0]);
        return 1;
    }

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) 
    {
        perror("socket");
        return 1;
    }
    
    fcntl(sockfd, F_SETFL, O_NONBLOCK);

    memset(&cliaddr, 0, sizeof(cliaddr));
    cliaddr.sin_family = AF_INET;
    cliaddr.sin_port = htons(51000); 
    if (inet_aton(argv[1], &cliaddr.sin_addr) == 0) 
    {
        fprintf(stderr, "Invalid IP address\n");
        close(sockfd);
        return 1;
    }
    
    printf("Чат запущен. Отправляйте сообщения.\n");
    
    pthread_create(&read, NULL, Send_Message, 0);
    pthread_create(&write, NULL, Read_Message, 0);
    
    pthread_join(read, NULL); 
    pthread_join(write, NULL); 
    
    return 0;
}
