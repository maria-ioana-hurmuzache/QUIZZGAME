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
#include <queue>
#include <vector>
#include <algorithm>
#include "Client.cpp"

#define PORT 8080
#define BUFFERSIZE 1024

typedef struct intrebare
{
    char* question;
    char* A;
    char* B;
    int id;
    char* R;
}intrebare;

void start_server();
void start_sesiune();
void setari_sesiune();
int configureaza_server();
void* accepta_conexiuni(void*);
void creeaza_threadpool(int nr);
void* timer_thread(void* arg);
void* treat(void*);
void trimite_mesaj(int cd, const char* msg);
const char* primeste_mesaj(int cd);
void get_questions();
static int callback(void *data, int argc, char** argv, char** rez);
int timed_response(int cd, int qid);
bool compara_punctaje(Client*a, Client*b);

int timp_de_conectare=0;
int nrclienti=0;
int gata=0;
bool conectare_expirata=false;
int timp_de_raspuns=0;
char clasament[BUFFERSIZE];
char parola[BUFFERSIZE]="";
queue <Client*> clienti;
vector <Client*> jucatori;
queue <pair<Client*,string>> sarcini;
int nrthreads=0;
pthread_t* threadpool;
int sd;

sqlite3* db;
int rc;
int nrintrebari=3;
intrebare test[3];

int raspunsuri=0;

pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond1 = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond2 = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond3 = PTHREAD_COND_INITIALIZER;

int main()
{
    char comanda[BUFFERSIZE];
    start_server();
    while(1)
    {
        printf("Comanda: ");
        bzero(comanda, sizeof(comanda));
        fgets(comanda, sizeof(comanda), stdin);   
        if(strcmp(comanda, "start\n")==0)
        {
            start_sesiune();
        }
        else if(strcmp(comanda, "quit\n")==0)
        {
            printf("La revedere!\n");
            fflush(stdout);
            break;
        }
        else
        {
            printf("Comanda introdusa nu e valida!\n");
            continue;
        }
    }
    return(0);
}

void start_server()
{
    printf("Bun venit la QuizzGame! Iata ce poti face:\n");
    printf("- Introduceti comanda \"start\" pentru a porni o noua sesiune de joc.\n");
    printf("- Introduceti comanda \"quit\" pentru a inchide aplicatia.\n");
    fflush(stdout);
}

void start_sesiune()
{
    setari_sesiune();
    //etapa de conectare
    sd=configureaza_server();
    pthread_t timer;
    if(pthread_create(&timer, NULL, timer_thread, &timp_de_conectare))
    {
        perror("[server]: Eroare la crearea thread-ului care accepta conexiunile");
        exit(1);
    }

    pthread_t accept;
    if(pthread_create(&accept, NULL, accepta_conexiuni, NULL))
    {
        perror("[server]: Eroare la crearea thread-ului care masoara timpul de conectare");
        exit(1);
    }

    //astept sa se termine conectarea
    pthread_mutex_lock(&mutex);
    pthread_cond_wait(&cond, &mutex);
    pthread_mutex_unlock(&mutex);

    pthread_mutex_lock(&mutex);
    get_questions();
    pthread_mutex_unlock(&mutex);

    //prethreading
    creeaza_threadpool(nrthreads);

    //astept sa se termine asocierea numelor
    pthread_mutex_lock(&mutex);
    pthread_cond_wait(&cond, &mutex);
    pthread_mutex_unlock(&mutex);

    //astept sa se raspunda la intrebari
    pthread_mutex_lock(&mutex);
    if(nrclienti!=0)
    {
        pthread_cond_wait(&cond1, &mutex);
    }
    pthread_mutex_unlock(&mutex);

    pthread_mutex_lock(&mutex);
    sort(jucatori.begin(), jucatori.end(), compara_punctaje);
    pthread_mutex_unlock(&mutex);
    int written=snprintf(clasament, BUFFERSIZE, "Clasamentul acestei sesiuni este:\n");
    for(int i=nrclienti-1; i>=0; i--)
    {
        if(jucatori[i]->GetStatus()==true)
        {
            if(jucatori[i]->GetPunctaj()==jucatori[nrclienti-1]->GetPunctaj())
            {
                written+=snprintf(clasament+written, BUFFERSIZE-written, "%d. %s : %d puncte - castigator\n", i+1, jucatori[i]->GetNume(), jucatori[i]->GetPunctaj());
            }
            else
            {
                written+=snprintf(clasament+written, BUFFERSIZE-written, "%d. %s : %d puncte\n", i+1, jucatori[i]->GetNume(), jucatori[i]->GetPunctaj());
            }
        }
        else
        {
            written+=snprintf(clasament+written, BUFFERSIZE-written, "%d. %s : deconectat\n", i+1, jucatori[i]->GetNume());
        }
    }
    if(nrclienti!=0)
    {
        printf("%s\n", clasament);
    }
    fflush(stdout);
    bool astept=true;
    for(int i=nrclienti-1; i>=0; i--)
    {
        if(jucatori[i]->GetStatus()==true)
        {
            pair<Client*, string> sarcina;
            sarcina.first=jucatori[i];
            sarcina.second="clasament";
            pthread_mutex_lock(&mutex);
            sarcini.push(sarcina);
            pthread_mutex_unlock(&mutex);
        }
        else
        {
            pthread_mutex_lock(&mutex);
            gata++;
            if(gata==nrclienti)
            {
                printf("Toti clientii s-au deconectat pe parcursul jocului\n");
                astept=false;
            }
            pthread_mutex_unlock(&mutex);
        }
    }

    //astept trimiterea clasamentului la toti jucatorii activi
    if(astept==true)
    {
        pthread_mutex_lock(&mutex);
        if(nrclienti!=0)
        {
            pthread_cond_wait(&cond2, &mutex);
        }
        else
        {
            printf("Toti clientii s-au deconectat pe parcursul jocului\n");
        }
        pthread_mutex_unlock(&mutex);
    }

    for(int i=0; i<nrthreads; i++)
    {
        pair<Client*, string> sarcina;
        sarcina.first=nullptr;
        sarcina.second="inchide";
        pthread_mutex_lock(&mutex);
        sarcini.push(sarcina);
        pthread_mutex_unlock(&mutex);
    }

    for(int i=0; i<nrthreads; i++)
    {
        pthread_join(threadpool[i],NULL);
    }

    if(-1==close(sd))
    {
        perror("Eroare la inchiderea sd");
    }
    pthread_cancel(accept);
    conectare_expirata=false;
    jucatori.clear();
    nrclienti=0;
    gata=0;
    printf("Sesiunea curenta s-a incheiat!\n");
}

void setari_sesiune()
{
    char buffer[BUFFERSIZE];
    printf("Inainte de a incepe, introduceti urmatoarele date:\n");
    fflush(stdout);
    printf("Introduceti durata de timp (in secunde) alocata conectarii participantilor la joc: ");
    fflush(stdout);
    bzero(buffer, sizeof(buffer));
    if(read(STDIN_FILENO, &buffer, sizeof(buffer))<=0)
    {
        perror("[server]: Eroare la citirea timpului alocat conectarii");
        exit(1);
    }
    timp_de_conectare=atoi(buffer);
    bzero(buffer, sizeof(buffer));
    printf("Introduceti durata de timp (in secunde) in care participantii vor putea raspunde la o intrebare: ");
    fflush(stdout);
    if(read(STDIN_FILENO, &buffer, sizeof(buffer))<=0)
    {
        perror("[server]: Eroare la citirea timpului de raspuns");
        exit(1);
    }
    timp_de_raspuns=atoi(buffer);
    bzero(buffer, sizeof(buffer));
    printf("Introduceti numarul de thread-uri care sa fie create pentru deservirea jucatorilor: ");
    fflush(stdout);
    if(read(STDIN_FILENO, &buffer, sizeof(buffer))<=0)
    {
        perror("[server]: Eroare la citirea numarului de thread-uri");
        exit(1);
    }
    nrthreads=atoi(buffer);
    bzero(buffer, sizeof(buffer));
    printf("Doriti ca sesiunea sa fie privata? Introduceti \"DA\" in cazul afirmativ. Orice alt raspuns va fi considerat \"NU\".\nIntroduceti raspunsul: ");
    fflush(stdout);
    if(read(STDIN_FILENO, &buffer, sizeof(buffer))<=0)
    {
        perror("[server]: Eroare la citirea DA/NU");
        exit(1);
    }
    if(strcmp(buffer,"DA\n")==0)
    {
        bzero(buffer, sizeof(buffer));
        printf("Introduceti parola: ");
        fflush(stdout);
        if(read(STDIN_FILENO, &buffer, sizeof(buffer))<=0)
        {
            perror("[server]: Eroare la citirea parolei");
            exit(1);
        }
        bzero(parola, sizeof(parola));
        strcpy(parola, buffer);
        printf("Parola de conectare este: %s", parola);
        parola[strlen(parola)-1]='\0';
    }
    else
    {
        bzero(parola, sizeof(parola));
        strcpy(parola, "");
    }
    printf("Jucatorii se vor putea conecta timp de %d secunde si vor avea %d secunde sa raspunda la fiecare intrebare. Pentru deservirea jucatorilor vor fi create %d thread-uri\n", timp_de_conectare, timp_de_raspuns, nrthreads);
}

int configureaza_server()
{
    struct sockaddr_in server;
    int sd;

    if(-1==(sd=socket(AF_INET, SOCK_STREAM, 0)))
    {
        perror("[server]: Eroare la socket()");
        exit(1);
    }

    int opt=1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    bzero(&server, sizeof(server));

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
    return sd;
}

void* accepta_conexiuni(void* arg)
{
    bool ok=false;
    while(conectare_expirata==false || ok==false)
    {
        int client;
        sockaddr_in from;
        bzero(&from, sizeof(from));
        unsigned int length=sizeof(from);
        if(-1==(client=accept(sd, (struct sockaddr*) &from, &length)))
        {
            perror("[server]: Eroare la accept()");
        }
        if(strcmp(parola,"")!=0)
        {
            trimite_mesaj(client, "Introduceti parola:");
            const char* p=primeste_mesaj(client);
            if(strcmp(p, parola)!=0)
            {
                trimite_mesaj(client, "Parola nu e corecta. Conexiunea se va inchide.");
                trimite_mesaj(client, "Jocul s-a incheiat");
                close(client);
                continue;
            }
        }
        trimite_mesaj(client, "Te-ai conectat la QuizzGame! Va trebui sa astepti pana toti participantii se conecteaza. Pana atunci, cum te numesti?");
        pair<Client*, string> sarcina;
        pthread_mutex_lock(&mutex);
        Client* c= new Client(client);
        sarcina.first=c;
        sarcina.second="nume";
        sarcini.push(sarcina);
        nrclienti++;
        if(nrclienti>1)
        {
            ok=true;
        }
        pthread_mutex_unlock(&mutex);
    }

    pthread_mutex_lock(&mutex);
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);

    while(1)
    {
        int client;
        sockaddr_in from;
        bzero(&from, sizeof(from));
        unsigned int length=sizeof(from);
        if(-1==(client=accept(sd, (struct sockaddr*) &from, &length)))
        {
            perror("[server]: Eroare la accept()");
        }
        trimite_mesaj(client, "Ne pare rau, dar timpul de conectare s-a incheiat.");
        close(client);
    }
    return NULL;
}

void* timer_thread(void* arg)
{
    int time=*(int*)arg;
    pthread_detach(pthread_self());
    sleep(time);
    pthread_mutex_lock(&mutex);
    conectare_expirata=true;
    pthread_mutex_unlock(&mutex);
    return NULL;
}

void creeaza_threadpool(int nr)
{
    threadpool=(pthread_t*)calloc(nr, sizeof(pthread_t));
    for(int i=0; i<nr; i++)
    {
        pthread_create(&threadpool[i], NULL, treat, NULL);
    }
}

void* treat(void* arg)
{
    while(1)
    {
        char buffer[BUFFERSIZE];
        string sarcina="";
        Client* c=nullptr;

        pthread_mutex_lock(&mutex);
        if(sarcini.empty()==false)
        {
            sarcina=sarcini.front().second;
            c=sarcini.front().first;
            sarcini.pop();
        }
        pthread_mutex_unlock(&mutex);

        if(sarcina=="nume")
        {
            const char* aux=primeste_mesaj(c->GetDescriptor());
            if(strcmp(aux, "d")==0)
            {
                c->Inactive();
                pthread_mutex_lock(&mutex);
                nrclienti--;
                if(nrclienti==0)
                {
                    pthread_cond_signal(&cond);
                }
                pthread_mutex_unlock(&mutex);
                continue;
            }
            c->SetNume(aux);
            bzero(buffer, sizeof(buffer));
            sprintf(buffer, "Buna %s, jocul va incepe curand!", c->GetNume());
            trimite_mesaj(c->GetDescriptor(), buffer);
            printf("S-a conectat: %s\n", c->GetNume());
            pthread_mutex_lock(&mutex);
            jucatori.push_back(c);
            gata++;
            if(gata==nrclienti)
            {
                pthread_cond_signal(&cond);
                gata=0;
            }
            pair<Client*,string> s;
            s.first=c;
            s.second="intrebare";
            sarcini.push(s);
            pthread_mutex_unlock(&mutex);
        }
        else if(sarcina=="intrebare")
        {
            int i=c->GetRaspunsuri();
            bzero(buffer, sizeof(buffer));
            sprintf(buffer, "Intrebarea %d: %s\nVariante de raspuns:\nA: %s\nB: %s\nIntroduceti majuscula corespunzatoare raspunsului corect:", test[i].id, test[i].question, test[i].A, test[i].B);
            trimite_mesaj(c->GetDescriptor(), buffer);
            int puncte=timed_response(c->GetDescriptor(), i);
            c->ActualizarePunctaj(puncte);
            c->ActualizareRaspunsuri();
            if(c->GetRaspunsuri()<nrintrebari && puncte>=0)
            {
                pthread_mutex_lock(&mutex);
                pair<Client*,string> s;
                s.first=c;
                s.second="intrebare";
                sarcini.push(s);
                pthread_cond_signal(&cond);
                pthread_mutex_unlock(&mutex);
            }
            else
            {
                if(puncte<0)
                {
                    c->Inactive();
                }
                pthread_mutex_lock(&mutex);
                gata++;
                if(gata==nrclienti)
                {
                    pthread_cond_signal(&cond1);
                    gata=0;
                }
                pthread_mutex_unlock(&mutex);
            }
        }
        else if(sarcina=="clasament")
        {
            bzero(buffer, sizeof(buffer));
            sprintf(buffer, "Punctajul tau final este: %d\n", c->GetPunctaj());
            trimite_mesaj(c->GetDescriptor() ,buffer);
            trimite_mesaj(c->GetDescriptor(), clasament);
            if(c->GetPunctaj()==jucatori[nrclienti-1]->GetPunctaj())
            {
                trimite_mesaj(c->GetDescriptor(), "Felicitari, ai castigat!");
            }
            trimite_mesaj(c->GetDescriptor(), "Jocul s-a incheiat!");
            close(c->GetDescriptor());
            delete(c);
            pthread_mutex_lock(&mutex);
            gata++;
            if(gata==nrclienti)
            {
                pthread_cond_signal(&cond2);
            }
            pthread_mutex_unlock(&mutex);
        }
        else if(sarcina=="inchide")
        {
            break;
        }
    }
    return 0;
}

void trimite_mesaj(int cd, const char* msg)
{
    int lungime=strlen(msg);
    if(-1==send(cd, &lungime, sizeof(int), 0))
    {
        perror("[server]: Eroare la trimiterea lungimii mesajului");
    }
    if(-1==send(cd, msg, lungime, 0))
    {
        perror("[server]: Eroare la trimiterea mesajului");
    }
}

const char* primeste_mesaj(int cd)
{
    int length;
    if(-1==recv(cd, &length, sizeof(int), 0))
    {
        perror("[server]: Eroare la recv() de lungimea mesajului");
        exit(1);
    }
    char* mesaj=(char*)malloc((length+1)*sizeof(char));
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
            return "d";
        }
        bytes_received+=r;
    }
    mesaj[length-1]='\0';
    fflush(stdout);
    return mesaj;
}

void get_questions()
{
    rc=sqlite3_open("intrebari.db", &db);
    if(rc!=0)
    {
        fprintf(stderr, "[server]: Nu am putut deschide baza de date: %s\n", sqlite3_errmsg(db));
        exit(1);
    }
    char fraza_select[BUFFERSIZE];
    sprintf(fraza_select, "select * from test;");
    char* errmsg=NULL;
    rc=sqlite3_exec(db, fraza_select, callback, NULL, &errmsg);
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

int timed_response(int cd, int qid)
{
    int punctaj=0;
    struct timeval timeout;
    fd_set read_fds;
    timeout.tv_sec=timp_de_raspuns;
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
            const char* raspuns=primeste_mesaj(cd);
            if(strcmp(raspuns, test[qid].R)==0)
            {
                punctaj+=5;
                trimite_mesaj(cd,"Raspuns corect! Ai acumulat 5 puncte in plus!");
            }
            else if(strcmp(raspuns, "d")==0)
            {   
                punctaj=-5;
            }
            else
            {
                trimite_mesaj(cd,"Raspuns gresit!");
            }
        }
    }
    return punctaj;
}

bool compara_punctaje(Client* a, Client* b)
{
    return a->GetPunctaj()<b->GetPunctaj();
}