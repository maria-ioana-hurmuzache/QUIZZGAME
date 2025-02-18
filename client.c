#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>

#define BUFFER_SIZE 256

int port;

void* trimite_mesaj(void* arg);

int main(int argc, char *argv[])
{
    int sd;
    struct sockaddr_in server;

    if (argc!=3)
    {
        printf("[client]: Sintaxa apelului trebuie sa fie de forma: %s <adresa server> <port>\n", argv[0]);
        exit(1);
    }

    port=atoi(argv[2]);
    if(-1==(sd=socket(AF_INET, SOCK_STREAM, 0)))
    {
        perror("[client]: Eroare la socket()");
        exit(1);
    }

    server.sin_family=AF_INET;
    server.sin_addr.s_addr=inet_addr(argv[1]);
    server.sin_port=htons(port);

    if(-1==(connect(sd, (struct sockaddr*)&server, sizeof(struct sockaddr))))
    {
        perror("[client]: Eroare la connect()");
        exit(1);
    }

    printf("Conexiune la server reusita!\n");
    fflush(stdout);
    pthread_t writing;
    if(pthread_create(&writing, NULL, trimite_mesaj, &sd))
    {
        perror("[client]: Eroare la crearea thread-ului de citire");
        close(sd);
        exit(1);
    }

    while(1)
    {
        int length=0;
        if(-1==recv(sd, &length, sizeof(int), 0))
        {
            perror("[client]: Eroare la recv() de lungimea mesajului");
            break;
        }
        char* mesaj=malloc((length+1)*sizeof(char));
        int bytes_received=0;
        while(bytes_received<length)
        {
            int r=recv(sd, mesaj+bytes_received, length-bytes_received,0);
            if(-1==r)
            {
                perror("[client]: Eroare la recv() de mesaj");
                free(mesaj);
                exit(1);
            }
            if(0==r)
            {
                perror("[client]: Server-ul a fost oprit");
                free(mesaj);
                break;
            }
            bytes_received+=r;
        }
        mesaj[length]='\0';
        printf("%s\n", mesaj);
        if(strcmp(mesaj,"\0")==0)
        {
            free(mesaj);
            break;
        }
        free(mesaj);
    }
    
    close(sd);
    pthread_cancel(writing);

    return 0;
}

void* trimite_mesaj(void* arg)
{
    int sd=*(int*)arg;
    char buffer[BUFFER_SIZE];
    while(1)
    {
        bzero(buffer, sizeof(buffer));
        fgets(buffer, sizeof(buffer), stdin);
        int lungime=strlen(buffer);
        if(-1==send(sd, &lungime, sizeof(int), 0))
        {
            perror("[client]: Eroare la trimiterea lungimii mesajului");
            break;
        }
        if(-1==send(sd, buffer, lungime, 0))
        {
            perror("[client]: Eroare la trimiterea mesajului");
            break;
        }
    }
    return NULL;
}