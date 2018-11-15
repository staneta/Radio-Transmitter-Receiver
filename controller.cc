#include <ctime>
#include <poll.h>
#include <cstring>
#include <arpa/inet.h>
#include <algorithm>
#include <sys/time.h>
#include "controller.h"
#include "err.h"

Controller::Controller(const string &NAME, uint64_t session_id, uint16_t DATA_PORT, uint16_t CTRL_PORT, int PSIZE,
                       int FSIZE, int RTIME, int data_sock, char *mcast_dotted_addr, const sockaddr_in &MCAST_ADDR,
                       deque<pair<uint64_t, vector<char>>> &last_bytes, bool& transmitterRunning)
        : NAME(NAME), session_id(session_id), DATA_PORT(DATA_PORT), CTRL_PORT(CTRL_PORT), PSIZE(PSIZE), FSIZE(FSIZE),
          RTIME(RTIME), data_sock(data_sock), mcast_dotted_addr(mcast_dotted_addr), MCAST_ADDR(MCAST_ADDR),
          last_bytes(last_bytes), transmitterRunning(transmitterRunning) {}


unsigned long long Controller::getTime()
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return ((((unsigned long long)tv.tv_sec) * 1000) +
            (((unsigned long long)tv.tv_usec) / 1000));
}


void Controller::setupSocket()
{
    control_sock = socket(AF_INET, SOCK_DGRAM, 0); // creating IPv4 UDP socket
    if (control_sock < 0)
        syserr("socket");

    control_address.sin_family = AF_INET; // IPv4
    control_address.sin_addr.s_addr = htonl(INADDR_ANY); // listening on all interfaces
    control_address.sin_port = htons(CTRL_PORT);


    if (setsockopt(control_sock,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes)) < 0) // TODO: czy to działa czy jest deterministyczne?
        syserr("Reusing ADDR failed");


    // bind the socket to a concrete address
    if (bind(control_sock, (struct sockaddr*) &control_address, (socklen_t) sizeof(control_address)) < 0)
        syserr("bind");
}


void Controller::sendLookup(sockaddr_in client_address)
{
    auto snda_len = (socklen_t) sizeof(client_address);
    char com[] = "BOREWICZ_HERE";
    vector<char> data;

    for(unsigned i=0; i<strlen(com); i++)
        data.push_back(com[i]);
    data.push_back(' ');

    for(unsigned i=0; i<strlen(mcast_dotted_addr); i++)
        data.push_back(mcast_dotted_addr[i]);
    data.push_back(' ');

    auto port_string = to_string(DATA_PORT);
    for(unsigned i=0; i<port_string.length(); i++)
        data.push_back(port_string[i]);
    data.push_back(' ');

    for(unsigned i=0; i<NAME.length(); i++)
        data.push_back(NAME[i]);
    data.push_back('\n');

    auto buffer = reinterpret_cast<char*>(data.data());
    ssize_t snd_bytes = sendto(control_sock, buffer, data.size(), 0, (struct sockaddr*) &client_address, snda_len);
    if (snd_bytes != data.size())
        syserr("error on sending datagram to client socket (sendlookup)");
}


struct CompareFirst
{
    CompareFirst(uint64_t val) : val_(val) {}
    bool operator()(const std::pair<uint64_t, vector<char>>& elem) const {
        return val_ == elem.first;
    }
private:
    uint64_t val_;
};


void Controller::gatherRexmits(char *buffer, int rcv_bytes)
{
    char com2[] = "LOUDER_PLEASE ";
    auto i = strlen(com2);

    do
    {
        if(buffer[i] == ',') i++;

        string num_string;
        uint64_t num;
        while(i < rcv_bytes && buffer[i] != ',' && buffer[i] != '\n')
        {
            num_string += buffer[i];
            i++;
        }
        num = stoull(num_string);

        reexmits.push(num);

    } while (buffer[i] == ',');
}


void Controller::sendRexmit(char* buffer, int rcv_bytes)
{
    while(!reexmits.empty())
    {
        auto num = reexmits.front();
        reexmits.pop();

        auto dequeue_it = std::find_if(last_bytes.begin(),last_bytes.end(), CompareFirst(num));
        if(dequeue_it != last_bytes.end())
        {
            auto data = dequeue_it->second;
            char* data_char = reinterpret_cast<char*>(data.data());

            auto len = data.size(); //(size_t) PSIZE + 16;
            auto sflags = 0;
            auto rcva_len = (socklen_t) sizeof(MCAST_ADDR);
            auto snd_len = sendto(data_sock, data_char, len, sflags, (struct sockaddr*) &MCAST_ADDR, rcva_len);
            if (snd_len != (ssize_t) len)
                syserr("partial / failed write");
        }
    }
}


void Controller::listen()
{
    setupSocket();

    char buffer[60000];
    struct sockaddr_in client_address;
    socklen_t rcv_addr_len;
    ssize_t rcv_bytes;


    struct pollfd desc;
    desc.fd = control_sock;
    desc.events = POLLIN;
    desc.revents = 0;

    auto start_time = getTime();

    auto snda_len = (socklen_t) sizeof(client_address);
    rcv_addr_len = (socklen_t) sizeof(client_address);

    // Receive LOOKUP and REXMIT
    while(transmitterRunning)
    {
        long long timeout = RTIME - (getTime() - start_time);
        auto ret_val = poll(&desc, 1, (int)timeout);

        if(ret_val < 0)
        {
            syserr("poll (control)");
        }
        else if(ret_val > 0)
        {
            rcv_bytes = recvfrom(control_sock, buffer, sizeof(buffer), 0, (struct sockaddr*) &client_address, &rcv_addr_len);
            //(void) printf("read from socket: %.*s\n", (int) rcv_bytes, buffer);

            if(rcv_bytes < 0)
            {
                syserr("error on datagram from client socket");
            }
            else
            {
                char com1[] = "ZERO_SEVEN_COME_IN";
                char com2[] = "LOUDER_PLEASE ";

                if((unsigned)rcv_bytes >= sizeof(com1) && strncmp(buffer, com1, strlen(com1)) == 0) // możesz zmienić na == 18
                {
                    sendLookup(client_address);
                }
                else if((unsigned)rcv_bytes >= sizeof(com2) && strncmp(buffer, com2, strlen(com2)) == 0)
                {
                    gatherRexmits(buffer, rcv_bytes);
                }
            }
        }
        else
        {
            sendRexmit(buffer, rcv_bytes);
            start_time = getTime();
        }
    }
}
