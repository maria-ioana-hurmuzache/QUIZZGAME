#include <iostream>
#include <string>
using namespace std;

class Client
{
    private:
    char* nume;
    int descriptor;
    int raspunsuri;
    int punctaj;
    bool active;
    public:
    Client(int descriptor);
    int GetDescriptor();
    void SetNume(const char*);
    const char* GetNume();
    int GetPunctaj();
    int GetRaspunsuri();
    void ActualizarePunctaj(int puncte);
    void ActualizareRaspunsuri();
    bool GetStatus();
    void Inactive();
    
};