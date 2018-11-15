#ifndef RADIO_DISCOVERER_H
#define RADIO_DISCOVERER_H

#include <cstdint>
#include <netinet/in.h>
#include <map>
#include <mutex>
#include <list>

using namespace std;

#define BUFFER_SIZE 1000
#define TTL_VALUE     4

class Discoverer
{
    map<string,pair<unsigned long long,pair<string,uint16_t>>>& stations;
    const char *discover_dotted_addr;
    struct sockaddr_in DISCOVER_ADDR;
    uint16_t CTRL_PORT;
    int BSIZE;
    int RTIME;
    int data_sock;
    int control_sock;
    u_int yes=1;
    string& currently_played;
    string& should_be_playing;
    bool& station_changed;
    bool& station_list_changed;
    mutex& station_mutex;
    list<pair<uint64_t, unsigned long long>>& missing;
    mutex& rexmit_mutex;

public:
    Discoverer(map<string,pair<unsigned long long,pair<string,uint16_t>>>& stations, const char *discover_dotted_addr,
               const sockaddr_in &DISCOVER_ADDR, uint16_t CTRL_PORT,
               int BSIZE, int RTIME, int data_sock, string& currently_played, string& should_be_playing,
               bool& station_changed, bool& station_list_changed, mutex& station_mutex,
               list<pair<uint64_t, unsigned long long>>& missing, mutex& rexmit_mutex);

    unsigned long long getTime();

    void setupSocket();

    void receiveLookup();

    void Lookup();
};


#endif //RADIO_DISCOVERER_H
