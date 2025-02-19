#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h> 
#include <sqlite3.h>
#include <time.h>
#include <sys/time.h>

#define PORT 8080
#define BUFFER_SIZE 4096
#define TIME_LIMIT 3
#define ANSWER_TIME 7

typedef struct player
{
    char* nume;
    int id;
    int client_descriptor;
    int punctaj;
}player;

typedef struct intrebare
{
    char* question;
    char* A;
    char* B;
    int id;
    char* R;
}intrebare;

static void* handle_client(void* arg);
void trimite_mesaj(int cd, char* msg);
char* primeste_mesaj(int cd);
int timed_response(int cd, int qid);
void get_questions();
static int callback(void *data, int argc, char** argv, char** rez);
void get_winners();

pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t condp = PTHREAD_COND_INITIALIZER;


time_t start;
int ready=1;
int nrThreads=0;
int nrPlayers=0;
int nrterminati=0;
//struct player* players;
pthread_t* threads;
sqlite3* db;
int rc;
intrebare test[3];
char castigatori[BUFFER_SIZE];
int maxim=0;

int main()
{
    struct sockaddr_in server;
    struct sockaddr_in from;
    int sd;
    
    if(-1==(sd=socket(AF_INET, SOCK_STREAM, 0)))
    {
        perror("[server]: Eroare la socket");
        exit(1);
    }
    
    int opt=1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    bzero (&server, sizeof(server));
    bzero (&from, sizeof(from));
    
    server.sin_family=AF_INET;
    server.sin_addr.s_addr=htonl(INADDR_ANY);
    server.sin_port=htons(PORT);

    if(-1==(bind(sd, (struct sockaddr*) &server, sizeof(struct sockaddr))))
    {
        perror("[server]: Eroare la bind()");
        exit(1);
    }

    if(-1==(listen(sd, 5)))
    {
        perror("[server]: Eroare la listen()");
        exit(1);
    }
    
    while(1)
    {
        printf("%d\n", nrPlayers);
        fflush(stdout);
        if(nrPlayers>0)
        {
            pthread_mutex_lock(&mutex);
            fflush(stdout);
            if(start!=0)
            {
                fflush(stdout);
                if(difftime(time(NULL), start)>TIME_LIMIT)
                {
                    printf("if3\n");
                    fflush(stdout);
                    pthread_mutex_unlock(&mutex);
                    break;
                }
                else
                {
                    printf("%f\n", difftime(time(NULL), start));
                    fflush(stdout);
                }
            }
            pthread_mutex_unlock(&mutex);
        }
        int client;
        struct player* p=NULL;
        unsigned int length=sizeof(from);
        
        printf("Astept conectarea participantilor la portul %d.\n", PORT);
        fflush(stdout);

        if(-1==(client=accept(sd, (struct sockaddr *)&from, &length)))
        {
            perror("[server]: Eroare la accept()");
            continue;
        }
        
        if(nrPlayers==0)
        {
            //players=(struct player*)malloc(sizeof(struct player));
            threads=(pthread_t*)malloc(sizeof(pthread_t));
        }
        else
        {
            //players=(struct player*)realloc(players, (nrPlayers+1)*sizeof(struct player));
            threads=(pthread_t*)realloc(threads, (nrPlayers+1)*sizeof(pthread_t));
        }
        
        p=(struct player*)malloc(sizeof(struct player));
        //p=&players[nrPlayers];
        p->nume=NULL;
        p->id=nrPlayers++;
        p->client_descriptor=client;
        p->punctaj=0;
        //players[nrPlayers-1]=*p;

        if(pthread_create(&threads[nrPlayers-1], NULL, handle_client, p)<0)
        {
            perror("[server]: Eroare la crearea thread-ului");
            free(p);
            if(nrPlayers!=1)
            {
                nrPlayers--;
            }
            else
            {
                //free(players);
                free(threads);
                nrPlayers--;
            }
            continue;
        }
        
    }
    printf("Jocul a inceput\n");
    fflush(stdout);
    
    sleep(5);

    pthread_mutex_lock(&mutex);
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&mutex);

    pthread_mutex_lock(&mutex);
    pthread_cond_wait(&condp, &mutex);
    pthread_mutex_unlock(&mutex);

    pthread_mutex_lock(&mutex);
    rc=sqlite3_open("intrebari.db", &db);
    if(rc!=0)
    {
        fprintf(stderr, "[server]: Nu am putut deschide baza de date: %s\n", sqlite3_errmsg(db));
        exit(1);
    }
    else
    {
        printf("Baza de date deschisa cu succes!\n");
    }

    get_questions();
    
    pthread_cond_broadcast(&cond);

    for(int i=0; i<3; i++)
    {
        pthread_cond_wait(&condp, &mutex);
        pthread_cond_broadcast(&cond);
    }
    
    pthread_cond_wait(&condp, &mutex);

    pthread_cond_broadcast(&cond);

    pthread_cond_wait(&condp, &mutex);

    pthread_mutex_unlock(&mutex);
    
    return 0;
}

static void* handle_client(void* arg)
{
    struct player p=*((struct player*) arg);
    printf("Thread %d:\n", p.id);
    fflush(stdout);
    trimite_mesaj(p.client_descriptor, "Bun venit in QUIZZGAME! Cum te numesti?");
    char* aux;
    aux=primeste_mesaj(p.client_descriptor);
    if(strcmp(aux,"d")==0)
    {
        pthread_detach(pthread_self());
        pthread_mutex_lock(&mutex);
        nrPlayers--;
        pthread_mutex_unlock(&mutex);
        pthread_exit(NULL);
    }
    p.nume=(char*)malloc(strlen(aux)+1);
    strcpy(p.nume, aux);
    free(aux);
    char buffer[BUFFER_SIZE];
    sprintf(buffer, "Buna, %s! Id-ul tau este %d. Jocul va incepe...", p.nume, p.id);
    trimite_mesaj(p.client_descriptor, buffer);
    if(p.id==0)
    {
        pthread_mutex_lock(&mutex);
        start=time(NULL);
        pthread_mutex_unlock(&mutex);
    }

    pthread_mutex_lock(&mutex);
    pthread_cond_wait(&cond, &mutex);
    pthread_mutex_unlock(&mutex);

    trimite_mesaj(p.client_descriptor, "Jocul incepe...");

    pthread_mutex_lock(&mutex);
    if(difftime(time(NULL), start)>TIME_LIMIT && p.id==(nrPlayers-1))
    {
        pthread_cond_broadcast(&condp);
    }
    pthread_mutex_unlock(&mutex);

    pthread_mutex_lock(&mutex);
    pthread_cond_wait(&cond, &mutex);

    for(int i=0; i<3; i++)
    {
        bzero(buffer, sizeof(buffer));
        sprintf(buffer, "Intrebarea %d: %s\nVariante de raspuns:\nA: %s\nB: %s\nIntroduceti majuscula corespunzatoare raspunsului corect:", test[i].id, test[i].question, test[i].A, test[i].B);
        trimite_mesaj(p.client_descriptor, buffer);
        p.punctaj+=timed_response(p.client_descriptor, i);
        nrterminati++;
        if(nrterminati==nrPlayers)
        {
            nrterminati=0;
            pthread_cond_broadcast(&condp);
        }
        pthread_cond_wait(&cond, &mutex);
    }

    if(p.punctaj>maxim)
    {
        maxim=p.punctaj;
        bzero(castigatori, sizeof(castigatori));
        sprintf(castigatori, "Castigatorii sunt:\n%s\n", p.nume);
    }
    else if(p.punctaj==maxim)
    {
        strcat(castigatori, p.nume);
        strcat(castigatori, "\n");
    }
    nrterminati++;
    if(nrterminati==nrPlayers)
    {
        nrterminati=0;
        pthread_cond_broadcast(&condp);
    }

    pthread_cond_wait(&cond, &mutex);
    bzero(buffer, sizeof(buffer));
    sprintf(buffer, "\nPunctajul tau este de %d puncte\nPunctajul maxim a fost de %d puncte\n", p.punctaj, maxim);
    trimite_mesaj(p.client_descriptor, buffer);
    trimite_mesaj(p.client_descriptor, castigatori);
    nrterminati++;
    if(nrterminati==nrPlayers)
    {
        nrterminati=0;
        pthread_cond_broadcast(&condp);
    }

    pthread_mutex_unlock(&mutex);

    close(p.client_descriptor);
    pthread_detach(pthread_self());
    return(NULL);
}

void trimite_mesaj(int cd, char* msg)
{
    int lungime=strlen(msg);
    if(-1==send(cd, &lungime, sizeof(int), 0))
    {
        perror("[server]: Eroare la trimiterea lungimii mesajului");
        pthread_detach(pthread_self());
        pthread_exit(NULL);
    }
    if(-1==send(cd, msg, lungime, 0))
    {
        perror("[server]: Eroare la trimiterea mesajului");
        pthread_detach(pthread_self());
        pthread_exit(NULL);
    }
}

char* primeste_mesaj(int cd)
{
    int length;
    if(-1==recv(cd, &length, sizeof(int), 0))
    {
        perror("[server]: Eroare la recv() de lungimea mesajului");
        exit(1);
    }
    char* mesaj=malloc((length+1)*sizeof(char));
    int bytes_received=0;
    while(bytes_received<length)
    {
        int r=recv(cd, mesaj+bytes_received, length-bytes_received,0);
        if(-1==r)
        {
            perror("[server]: Eroare la recv() de mesaj");
            return "e";
        }
        if(0==r)
        {
            printf("[server]: Jucatorul s-a deconectat");
            return "d";
        }
        bytes_received+=r;
    }
    mesaj[length-1]='\0';
    printf("am primit mesajul: %s\n", mesaj);
    fflush(stdout);
    return mesaj;
}

void get_questions()
{
    char select[BUFFER_SIZE];
    sprintf(select, "select * from test;");
    char* errmsg=NULL;
    rc=sqlite3_exec(db, select, callback, NULL, &errmsg);
    if(rc!=SQLITE_OK)
    {
        fprintf(stderr, "SQL error: %s\n", errmsg);
        sqlite3_free(errmsg);
    }
}

static int callback(void *data, int nr_col, char** atribute, char** coloana)
{
    int id=atoi(atribute[0]);
    test[id-1].id=id;
    test[id-1].question=(char*)malloc(strlen(atribute[1]));
    strcpy(test[id-1].question,atribute[1]);
    test[id-1].A=(char*)malloc(strlen(atribute[2]));
    strcpy(test[id-1].A,atribute[2]);
    test[id-1].B=(char*)malloc(strlen(atribute[3]));
    strcpy(test[id-1].B,atribute[3]);
    test[id-1].R=(char*)malloc(strlen(atribute[4]));
    strcpy(test[id-1].R,atribute[4]);
    return 0;
}

// void get_winners()
// {
//     printf("ajung");
//     fflush(stdout);
//     int maxim=0;
//     for(int i=0; i<nrPlayers; i++)
//     {
//         if(players[i].punctaj>maxim)
//         {
//             maxim=players[i].punctaj;
//         }
//     }
//     printf("%d", maxim);
//     fflush(stdout);
//     sprintf(castigatori, "Punctajul maxim obtinut este %d. Castigatorii sunt: ", maxim);
//     printf("ffff\n");
//     for(int i=0; i<nrPlayers; i++)
//     {
//         if(players[i].punctaj==maxim)
//         {
//             printf("%s\n", players[i].nume);
//             strcat(castigatori, players[i].nume);
//             strcat(castigatori, " ");
//         }
//     }
// }

int timed_response(int cd, int qid)
{
    int punctaj=0;
    struct timeval timeout;
    fd_set read_fds;
    timeout.tv_sec=ANSWER_TIME;
    timeout.tv_usec=0;
    FD_ZERO(&read_fds);
    FD_SET(cd, &read_fds);
    int activity=select(cd+1, &read_fds, NULL, NULL, &timeout);
    if(activity<0)
    {
        perror("[server]: Eroare la select");
    }
    else if(activity==0)
    {
        trimite_mesaj(cd, "Timpul de raspuns a expirat.");
    }
    else
    {
        if(FD_ISSET(cd, &read_fds))
        {
            char* raspuns=primeste_mesaj(cd);
            if(strcmp(raspuns, test[qid].R)==0)
            {
                punctaj+=5;
                trimite_mesaj(cd,"Raspuns corect! Ai acumulat 5 puncte in plus!");
            }
            else if(strcmp(raspuns, "d")==0)
            {
                pthread_detach(pthread_self());
                nrPlayers--;
                pthread_mutex_unlock(&mutex);
                pthread_exit(NULL);
            }
            else
            {
                trimite_mesaj(cd,"Raspuns gresit!");
            }
        }
    }
    return punctaj;
}