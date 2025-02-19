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

#define BUFFER_SIZE 256

int port;

void comunicare(int server);
void trimite_mesaj(int sd, char* msg);
char* primeste_mesaj(int sd);


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
    comunicare(sd);
    close(sd);

    return 0;
}

void comunicare(int server)
{
    char* prompt=primeste_mesaj(server);
    printf("%s\n", prompt);
    fflush(stdout);
    char buffer[BUFFER_SIZE];
    bzero(buffer, sizeof(buffer));
    fgets(buffer, sizeof(buffer), stdin);
    trimite_mesaj(server, buffer);
    free(prompt);
    prompt=primeste_mesaj(server);
    printf("%s\n", prompt);
    fflush(stdout);
    free(prompt);
    prompt=primeste_mesaj(server);
    printf("%s\n", prompt);
    fflush(stdout);

    for(int i=0; i<3; i++)
    {
        prompt=primeste_mesaj(server);
        printf("%s\n", prompt);
        fflush(stdout);
        bzero(buffer, sizeof(buffer));
        fgets(buffer, sizeof(buffer), stdin);
        trimite_mesaj(server, buffer);
        free(prompt);
        fflush(stdout);
        prompt=primeste_mesaj(server);
        printf("%s\n", prompt);
        fflush(stdout);
        free(prompt);
    }

    prompt=primeste_mesaj(server);
    printf("%s\n", prompt);
    fflush(stdout);
    free(prompt);
    prompt=primeste_mesaj(server);
    printf("%s\n", prompt);
    fflush(stdout);
    free(prompt);
    close(server);
}

void trimite_mesaj(int sd, char* msg)
{
    int lungime=strlen(msg);
    if(-1==send(sd, &lungime, sizeof(int), 0))
    {
        perror("[client]: Eroare la trimiterea lungimii mesajului");
        exit(1);
    }
    if(-1==send(sd, msg, lungime, 0))
    {
        perror("[client]: Eroare la trimiterea mesajului");
        exit(1);
    }
}

char* primeste_mesaj(int sd)
{
    int length;
    if(-1==recv(sd, &length, sizeof(int), 0))
    {
        perror("[client]: Eroare la recv() de lungimea mesajului");
        exit(1);
    }
    char* mesaj=malloc((length+1)*sizeof(char));
    int bytes_received=0;
    while(bytes_received<length)
    {
        int r=recv(sd, mesaj+bytes_received, length-bytes_received,0);
        if(-1==r)
        {
            perror("[client]: Eroare la recv() de mesaj");
            exit(1);
        }
        if(0==r)
        {
            perror("[client]: Server-ul a fost oprit");
            exit(1);
        }
        bytes_received+=r;
    }
    mesaj[length]='\0';

    return mesaj;
}
