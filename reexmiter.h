#ifndef RADIO_REEXMITER_H
#define RADIO_REEXMITER_H

#include <iostream>
#include <vector>
#include <mutex>
#include <list>

using namespace std;

class Reexmiter
{
    list<pair<uint64_t, unsigned long long>>& missing;
    mutex& rexmit_mutex;
    int control_sock;
    int RTIME;
    struct sockaddr_in& DISCOVER_ADDR;

public:

    Reexmiter(list<pair<uint64_t, unsigned long long int>> &missing, std::mutex &rexmit_mutex, int control_sock,
              int RTIME, struct sockaddr_in& DISCOVER_ADDR);

    void Reexmit();

    unsigned long long getTime();
};


#endif //RADIO_REEXMITER_H
