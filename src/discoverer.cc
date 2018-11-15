#include <poll.h>
#include <arpa/inet.h>
#include <ctime>
#include <cstring>
#include <vector>
#include <string>
#include <iostream>
#include <sys/time.h>
#include <thread>
#include "discoverer.h"
#include "err.h"
#include "reexmiter.h"

Discoverer::Discoverer(map<string,pair<unsigned long long,pair<string,uint16_t>>>& stations,
                       const char *discover_dotted_addr, const sockaddr_in &DISCOVER_ADDR, uint16_t CTRL_PORT,
                       int BSIZE, int RTIME, int data_sock, string& currently_played, string& should_be_playing,
                       bool& station_changed, bool& station_list_changed, mutex& station_mutex,
                       list<pair<uint64_t, unsigned long long>>& missing, mutex& rexmit_mutex)
        : stations(stations), discover_dotted_addr(discover_dotted_addr), DISCOVER_ADDR(DISCOVER_ADDR),
          CTRL_PORT(CTRL_PORT), BSIZE(BSIZE), RTIME(RTIME), data_sock(data_sock),
          currently_played(currently_played), should_be_playing(should_be_playing), station_changed(station_changed),
          station_list_changed(station_list_changed), station_mutex(station_mutex), missing(missing), rexmit_mutex(rexmit_mutex) {}


unsigned long long Discoverer::getTime()
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return ((((unsigned long long)tv.tv_sec) * 1000) +
            (((unsigned long long)tv.tv_usec) / 1000));
}


void Discoverer::setupSocket()
{
    control_sock = socket(AF_INET, SOCK_DGRAM, 0); // creating IPv4 UDP socket
    if (control_sock < 0)
        syserr("socket");

    DISCOVER_ADDR.sin_family = AF_INET; // IPv4
    DISCOVER_ADDR.sin_port = htons(CTRL_PORT);
    if(inet_aton(discover_dotted_addr, &DISCOVER_ADDR.sin_addr) == 0)
        syserr("inet_aton");

    if(setsockopt(control_sock, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes)) < 0)
        syserr("Broadcast option failed");

    struct sockaddr_in myaddress;
    myaddress.sin_family = AF_INET; // IPv4
    myaddress.sin_port = htons(12234);
    myaddress.sin_addr.s_addr = htonl(INADDR_ANY);

    // bind the socket to a concrete address
    if(bind(control_sock, (struct sockaddr*) &myaddress, (socklen_t) sizeof(myaddress)) < 0)
        syserr("bind");
}


void Discoverer::receiveLookup()
{
    char lookup_rcv_com[] = "BOREWICZ_HERE";
    struct sockaddr_in client_address;

    char buffer[BUFFER_SIZE];

    ssize_t rcv_bytes;

    socklen_t rcv_addr_len = sizeof(client_address);
    rcv_bytes = recvfrom(control_sock, buffer, sizeof(buffer), 0, (struct sockaddr *) &client_address, &rcv_addr_len);

    if(rcv_bytes < 0)
    {
        syserr("error on datagram from client socket in discoverer");
    }
    else
    {
        if((unsigned)rcv_bytes > strlen(lookup_rcv_com) && strncmp(buffer, lookup_rcv_com, strlen(lookup_rcv_com)) == 0)
        {
            size_t i=strlen(lookup_rcv_com);
            if(buffer[i] == ' ') i++;
            else return;

            string mcast_addr;
            while(i<rcv_bytes-1 && buffer[i] != ' ')
            {
                mcast_addr += buffer[i];
                i++;
            }

            if(buffer[i] == ' ') i++;
            else return;


            string port_string;
            uint16_t data_port;
            while(i<rcv_bytes-1 && buffer[i] != ' ')
            {
                port_string += buffer[i];
                i++;
            }
            data_port = (uint16_t) stoi(port_string);


            if(buffer[i] == ' ') i++;
            else return;

            string name;
            while(i<rcv_bytes-1 && buffer[i] != '\n')
            {
                name += buffer[i];
                i++;
            }

            if(buffer[i] != '\n') return;

            station_mutex.lock();
            auto& station = stations[name];
            station.first = getTime();
            station.second.first = mcast_addr;
            station.second.second = data_port;
            station_mutex.unlock();
        }
    }
}


void Discoverer::Lookup()
{
    setupSocket();

    Reexmiter reexmiter(missing, rexmit_mutex, control_sock, RTIME, DISCOVER_ADDR);
    std::thread t4(&Reexmiter::Reexmit, reexmiter);

    char lookup_com[] = "ZERO_SEVEN_COME_IN\n";
    char buffer[BUFFER_SIZE];
    socklen_t snd_addr_len;
    ssize_t snd_bytes;

    struct pollfd desc;
    desc.fd = control_sock;
    desc.events = POLLIN;
    desc.revents = 0;

    snd_addr_len = (socklen_t) sizeof(DISCOVER_ADDR);
    unsigned long long start_time = getTime();

    snd_bytes = sendto(control_sock, lookup_com, sizeof(lookup_com), 0, (struct sockaddr*) &DISCOVER_ADDR, snd_addr_len);
    if (snd_bytes != sizeof(lookup_com))
        syserr("partial / failed write on send (discover.cc)");

    while(true)
    {
        long long timeout = 5000 - (getTime() - start_time);
        auto ret_val = poll(&desc, 1, (int)timeout);

        if(ret_val < 0)
        {
            syserr("control_poll");
        }
        else if(ret_val > 0)
        {
            receiveLookup();
        }
        else
        {
            start_time = getTime();

            // sprawdz czy jakieś się przeterminowały
            station_mutex.lock();
            for(auto it=stations.begin(); it!=stations.end();)
            {
                if(start_time - it->second.first >= 20000)
                {
                    if(it->first == currently_played)
                    {
                        should_be_playing = "";
                        currently_played = "";
                        station_changed = true;
                    }
                    it = stations.erase(it);
                }
                else
                    it++;
            }
            station_list_changed = true;
            station_mutex.unlock();

            snd_bytes = sendto(control_sock, lookup_com, sizeof(lookup_com), 0, (struct sockaddr*) &DISCOVER_ADDR, snd_addr_len);
            if (snd_bytes != sizeof(lookup_com))
                syserr("partial / failed write on send (discover.cc)");
        }
    }
}
