#include "Client.h"
#include <string.h>

Client::Client(int descriptor)
{
    this->descriptor=descriptor;
    this->raspunsuri=0;
    this->punctaj=0;
    this->active=true;
}

int Client::GetDescriptor()
{
    return this->descriptor;
}

void Client::SetNume(const char* nume)
{
    this->nume=(char*)malloc(strlen(nume));
    strcpy(this->nume,nume);
}

const char* Client::GetNume()
{
    return this->nume;
}

int Client::GetPunctaj()
{
    return this->punctaj;
}

bool Client::GetStatus()
{
    return this->active;
}

void Client::Inactive()
{
    this->active=false;
}

void Client::ActualizarePunctaj(int puncte)
{
    this->punctaj+=puncte;
}

int Client::GetRaspunsuri()
{
    return this->raspunsuri;
}

void Client::ActualizareRaspunsuri()
{
    this->raspunsuri++;
}