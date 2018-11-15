#ifndef RADIO_TRANSMITTER_H
#define RADIO_TRANSMITTER_H

#include <iostream>
#include <netinet/in.h>

using namespace std;

class Transmitter
{
    string NAME;
    uint64_t session_id;
    uint16_t DATA_PORT;
    uint16_t CTRL_PORT;
    int PSIZE;
    int FSIZE;
    int RTIME;
    int data_sock;
    char* mcast_dotted_addr;
    struct sockaddr_in MCAST_ADDR;
    deque<pair<uint64_t, vector<char>>>& last_bytes;
    bool& transmitterRunning;

public:
    Transmitter(const string &NAME, uint64_t session_id, uint16_t DATA_PORT, uint16_t CTRL_PORT, int PSIZE, int FSIZE,
                int RTIME, int data_sock, char *mcast_dotted_addr, const sockaddr_in &MCAST_ADDR,
                deque<pair<uint64_t,vector<char>>> &last_bytes, bool& transmitterRunning);

    void sendSong();

private:

    void resetData(vector<char> &data, uint64_t first_byte_num);
};


#endif //RADIO_TRANSMITTER_H
