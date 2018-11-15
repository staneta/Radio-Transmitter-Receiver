#ifndef RADIO_CONTROLLER_H
#define RADIO_CONTROLLER_H

#include <iostream>
#include <netinet/in.h>
#include <vector>
#include <deque>
#include <queue>

using namespace std;

#define BUFFER_SIZE 1000

class Controller
{
    string NAME;
    uint64_t session_id;
    uint16_t DATA_PORT;
    uint16_t CTRL_PORT;
    int PSIZE;
    int FSIZE;
    int RTIME;
    int data_sock;
    int control_sock;
    int rexmit_sock;
    char* mcast_dotted_addr;
    struct sockaddr_in MCAST_ADDR;
    struct sockaddr_in control_address;
    deque<pair<uint64_t, vector<char>>>& last_bytes;
    bool& transmitterRunning;
    u_int yes=1;
    queue<uint64_t> reexmits;

public:
    Controller(const string &NAME, uint64_t session_id, uint16_t DATA_PORT, uint16_t CTRL_PORT, int PSIZE, int FSIZE,
                int RTIME, int data_sock, char *mcast_dotted_addr, const sockaddr_in &MCAST_ADDR,
                deque<pair<uint64_t,vector<char>>> &last_bytes, bool& transmitterRunning);

    unsigned long long getTime();

    void setupSocket();

    void sendLookup(struct sockaddr_in client_address);

    void gatherRexmits(char* buffer, int rcv_bytes);

    void sendRexmit(char* buffer, int rcv_bytes);

    void listen();
};


#endif //RADIO_CONTROLLER_H
