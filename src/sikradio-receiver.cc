#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <iostream>
#include <cstring>
#include <thread>
#include <mutex>
#include <vector>
#include <sys/time.h>

#include "err.h"
#include "discoverer.h"
#include "menu.h"
#include "buffer.h"
#include "player.h"

#define PORT_NUM     1234

using namespace std;

void setParameter(std::string param, char* value);
void checkParameters(int argc, char *argv[]);
unsigned long long getTime();
int createSocket(uint16_t port, struct ip_mreq ip_mreq);
int deleteSocket(int sock, struct ip_mreq ip_mreq);
void getNewStation(int&data_sock, string& currently_played, bool& station_changed, bool& first_station_pick, struct ip_mreq& ip_mreq, mutex& station_mutex);


string NAME;
map<string,pair<unsigned long long,pair<string,uint16_t>>> stations;
const char *discover_dotted_addr = "255.255.255.255";
uint16_t CTRL_PORT = 30000 + (386032 % 10000);
uint16_t UI_PORT = 10000 + (386032 % 10000);
struct sockaddr_in DISCOVER_ADDR;
int BSIZE = 65536;
int RTIME = 250;
int PSIZE = 0;


int main(int argc, char *argv[])
{
    checkParameters(argc, argv);

    string currently_played;
    bool first_station_pick = true;
    bool station_changed = true;
    bool station_list_changed = true;
    mutex station_mutex;
    mutex data_mutex;
    string should_be_playing;

    uint64_t last_byte = 0;
    uint64_t BYTE0 = 0;
    uint64_t current_session_id = 0;
    Buffer data_buffer;
    bool stop_playing = false;
    bool stop_receiving = false;
    int threads_running = 0;

    mutex rexmit_mutex;
    list<pair<uint64_t, unsigned long long>> missing;

    size_t buffer_size = 65600;
    auto buffer = new char[buffer_size];
    ssize_t len;

    struct ip_mreq ip_mreq;
    int data_sock = -1;
    struct sockaddr_in client_address;

    Discoverer discoverer(stations, discover_dotted_addr, DISCOVER_ADDR, CTRL_PORT, BSIZE, RTIME, 0, currently_played, should_be_playing, station_changed, station_list_changed, station_mutex,
                          missing, rexmit_mutex);
    Menu menu(stations, discover_dotted_addr, DISCOVER_ADDR, CTRL_PORT, UI_PORT, BSIZE, RTIME, 0, currently_played, should_be_playing, station_changed, station_list_changed, station_mutex);
    Player player(data_buffer, stop_playing, stop_receiving, data_mutex);

    std::thread t1(&Discoverer::Lookup, discoverer);
    std::thread t2(&Menu::ShowMenus, menu);
    std::thread t3;


    for(;;)
    {
        if(threads_running > 0)
        {
            t3.join();
            threads_running--;
        }

        getNewStation(data_sock, currently_played, station_changed, first_station_pick, ip_mreq, station_mutex);

        struct pollfd desc;
        desc.fd = data_sock;
        desc.events = POLLIN;
        desc.revents = 0;
        bool first_package = true;
        bool start_playing = true;
        stop_playing = false;
        stop_receiving = false;

        while(true)
        {
            auto ret_val = poll(&desc, 1, 5000);

            if(ret_val < 0)
                syserr("control_poll");
            else if(ret_val > 0)
            {
                auto rcva_len = (socklen_t) sizeof(client_address);
                len = recvfrom(data_sock, buffer, buffer_size, 0, (struct sockaddr*) &client_address, &rcva_len);

                if(len < 0)
                    syserr("error on datagram from client socket");
                else
                {
                    uint64_t session_id=0;
                    uint64_t first_byte=0;
                    std::memcpy(&session_id, buffer, 8);
                    std::memcpy(&first_byte, buffer+8, 8);
                    session_id = be64toh(session_id);
                    first_byte = be64toh(first_byte);

                    if(session_id > current_session_id)
                    {
                        current_session_id = session_id;
                        break;
                    }

                    if(first_package)
                    {
                        // przypisuje wartosci
                        current_session_id = session_id;
                        BYTE0 = first_byte;
                        PSIZE = (int)len-16;
                        last_byte = first_byte;
                        data_buffer.clear(BSIZE/PSIZE, PSIZE);

                        // zmieniam wielkosc bufferu do recvfrom
                        buffer_size = (size_t) len;
                        char* tmp_buffer = new char[buffer_size];
                        memcpy(tmp_buffer, buffer, buffer_size);
                        delete[] buffer;
                        buffer = new char[buffer_size];
                        memcpy(buffer,tmp_buffer, buffer_size);

                        first_package = false;
                    }

                    std::shared_ptr<char[]> ptr(buffer);
                    if(first_byte == last_byte + PSIZE || first_byte == BYTE0)
                    {
                        int pos = data_buffer.add();
                        data_buffer.buf[pos].first = first_byte;
                        data_buffer.buf[pos].second = ptr;
                        last_byte = first_byte;
                    }
                    else if(first_byte > last_byte + PSIZE)
                    {
                        int pos;
                        auto curr_time = getTime();

                        rexmit_mutex.lock();
                        while(first_byte > last_byte)
                        {
                            last_byte += PSIZE;
                            pos = data_buffer.add();
                            data_buffer.buf[pos].first = 0;
                            data_buffer.buf[pos].second = nullptr;
                            missing.emplace_back(last_byte, curr_time);
                        }
                        rexmit_mutex.unlock();

                        pos = data_buffer.add();
                        data_buffer.buf[pos].first = first_byte;
                        data_buffer.buf[pos].second = ptr;
                        last_byte = first_byte;
                    }
                    else // first_byte < last_byte + PSIZE
                    {
                        auto pos = data_buffer.findPos((int)(last_byte - first_byte)/PSIZE);
                        if(pos != -1)
                        {
                            data_buffer.buf[pos].first = first_byte;
                            data_buffer.buf[pos].second = ptr;
                        }

                        rexmit_mutex.lock();
                        auto oldest = data_buffer.buf[data_buffer.getBeg()];
                        for(auto it=missing.begin(); it!=missing.end(); ++it)
                        {
                            if(oldest.second != nullptr)
                            {
                                if(it->first < oldest.first)
                                    it = missing.erase(it);
                            }
                            else if(it->first == first_byte)
                            {
                                missing.erase(it);
                                break;
                            }
                        }
                        rexmit_mutex.unlock();
                    }
                    if(start_playing && (first_byte > BYTE0 + BSIZE*3/4))
                    {
                        t3 = thread(&Player::Play, player);
                        threads_running++;
                        start_playing = false;
                    }

                    buffer = new char[buffer_size];
                }

                if(station_changed || stop_receiving)
                {
                    stop_playing = true;
                    break;
                }
            }
            else break;
        }
    }
}

void getNewStation(int&data_sock, string& currently_played, bool& station_changed, bool& first_station_pick, struct ip_mreq& ip_mreq, mutex& station_mutex)
{

    while(true)
    {
        while(true)
        {
            station_mutex.lock();
            if(!stations.empty())
            {
                break;
            }
            station_mutex.unlock();
        }

        string mcast;
        uint16_t port;

        if(!stations.empty())
        {
            // jeśli jest ustawiony parametr -n
            if(first_station_pick && !NAME.empty())
            {
                for(const auto &station : stations)
                {
                    if(station.first == NAME)
                    {
                        currently_played = NAME;
                        mcast = station.second.second.first;
                        port = station.second.second.second;
                        first_station_pick = false;
                    }
                }
                if(first_station_pick) continue;
            }
            if(currently_played.empty())
            {
                currently_played = stations.begin()->first;
                mcast = stations.begin()->second.second.first;
                port = stations.begin()->second.second.second;
            }
            else
            {
                for(const auto &station : stations)
                {
                    if(station.first == currently_played)
                    {
                        mcast = station.second.second.first;
                        port = station.second.second.second;
                    }
                }
            }
            station_changed = false;

            if(data_sock != -1)
                data_sock = deleteSocket(data_sock, ip_mreq);

            ip_mreq.imr_interface.s_addr = htonl(INADDR_ANY);
            if(inet_aton(mcast.c_str(), &ip_mreq.imr_multiaddr) == 0) //"239.10.11.12"
                syserr("inet_aton");
            data_sock = createSocket(port, ip_mreq);
        }
        station_mutex.unlock();
        break;
    }
}


int createSocket(uint16_t port, struct ip_mreq ip_mreq)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock < 0)
        syserr("socket");

    struct sockaddr_in data_address;
    data_address.sin_family = AF_INET; // IPv4
    data_address.sin_port = htons(port); //20000 + (386032 % 10000));
    data_address.sin_addr.s_addr = htonl(INADDR_ANY);

    u_int on = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    if(bind(sock, (struct sockaddr*) &data_address, (socklen_t) sizeof(data_address)) < 0)
        syserr("bind");

    if(setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void*)&ip_mreq, sizeof ip_mreq) < 0)
        syserr("setsockopt");

    return sock;
}


int deleteSocket(int sock, struct ip_mreq ip_mreq)
{
    if(setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, (void*)&ip_mreq, sizeof ip_mreq) < 0)
        syserr("setsockopt");

    close(sock);
    return -1;
}


unsigned long long getTime()
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return ((((unsigned long long)tv.tv_sec) * 1000) +
            (((unsigned long long)tv.tv_usec) / 1000));
}


void checkParameters(int argc, char *argv[])
{
    if(argc%2 == 0)
        syserr("Wrong number of parameters");
    for(int i=1; i<argc; i+=2)
        setParameter(std::string(argv[i]), argv[i+1]);
}


void setParameter(string param, char* value)
{
    if(param == "-d") {
        discover_dotted_addr = value;
    } else if(param == "-C") {
        CTRL_PORT = atoi(value);
    } else if(param == "-U") {
        UI_PORT = atoi(value);
    } else if(param == "-b") {
        BSIZE = atoi(value);
    } else if(param == "-R") {
        RTIME = atoi(value);
    } else if(param == "-n") {
        NAME = string(value);
    } else {
        syserr("Cannot recognize this parameter");
    }
}
