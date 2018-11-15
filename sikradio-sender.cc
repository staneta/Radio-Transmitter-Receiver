#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <string>
#include <ctime>
#include <vector>
#include <thread>
#include <deque>
#include <sys/time.h>
#include "err.h"
#include "transmitter.h"
#include "controller.h"

using namespace std;

string NAME = "Nienazwany Nadajnik";
uint64_t session_id;
uint16_t DATA_PORT = 20000 + (386032 % 10000);
uint16_t CTRL_PORT = 30000 + (386032 % 10000);
int PSIZE = 512;
int FSIZE = 131072;
int RTIME = 250;
char* mcast_dotted_addr = nullptr;
struct sockaddr_in MCAST_ADDR;

deque<pair<uint64_t, vector<char>>> last_bytes;

void setParameter(std::string param, char* value);
void checkParameters(int argc, char *argv[]);


int main(int argc, char *argv[])
{
    checkParameters(argc, argv);
    session_id = (uint64_t) std::time(nullptr);

    MCAST_ADDR.sin_family = AF_INET; // IPv4
    MCAST_ADDR.sin_port = htons(DATA_PORT);
    if(inet_aton(mcast_dotted_addr, &MCAST_ADDR.sin_addr) == 0)
        syserr("inet_aton");

    int data_sock = socket(AF_INET, SOCK_DGRAM, 0); // was pf
    if (data_sock < 0)
        syserr("socket");



    bool transmitterRunning = true;
    Transmitter transmitter(NAME, session_id, DATA_PORT, CTRL_PORT, PSIZE, FSIZE, RTIME, data_sock, mcast_dotted_addr, MCAST_ADDR, last_bytes, transmitterRunning);
    Controller controller(NAME, session_id, DATA_PORT, CTRL_PORT, PSIZE, FSIZE, RTIME, data_sock, mcast_dotted_addr, MCAST_ADDR, last_bytes, transmitterRunning);

    std::thread t1(&Transmitter::sendSong, transmitter);
    std::thread t2(&Controller::listen, controller);
    t1.join();
    t2.join();
}
















void checkParameters(int argc, char *argv[])
{
    if(argc%2 == 0)
        syserr("Wrong number of parameters");
    for(int i=1; i<argc; i+=2)
        setParameter(std::string(argv[i]), argv[i+1]);
    if(mcast_dotted_addr == nullptr)
        syserr("Parameter -a is obligatory");
}

void setParameter(string param, char* value)
{
    if(param == "-a") {
        mcast_dotted_addr = value;
    } else if(param == "-P") {
        DATA_PORT = atoi(value);
    } else if(param == "-C") {
        CTRL_PORT = atoi(value);
    } else if(param == "-p") {
        PSIZE = atoi(value);
    } else if(param == "-f") {
        FSIZE = atoi(value);
    } else if(param == "-R") {
        RTIME = atoi(value);
    } else if(param == "-n") {
        NAME = string(value);
        if(NAME.length() > 64)
            syserr("Station's name is too long");
    } else {
        syserr("Cannot recognize this parameter");
    }
}
