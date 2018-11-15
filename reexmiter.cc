#include <sys/time.h>
#include <netinet/in.h>
#include "reexmiter.h"
#include "err.h"

Reexmiter::Reexmiter(list<pair<uint64_t, unsigned long long int>> &missing, mutex& rexmit_mutex, int control_sock,
                     int RTIME, struct sockaddr_in& DISCOVER_ADDR)
        : missing(missing), rexmit_mutex(rexmit_mutex), control_sock(control_sock), RTIME(RTIME),
          DISCOVER_ADDR(DISCOVER_ADDR) {}


unsigned long long Reexmiter::getTime()
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return ((((unsigned long long)tv.tv_sec) * 1000) +
            (((unsigned long long)tv.tv_usec) / 1000));
}


void Reexmiter::Reexmit()
{
    int milisec = RTIME; // length of time to sleep, in miliseconds

    string com = "LOUDER_PLEASE ";
    vector<char> package;

    while(true)
    {
        // clear vector
        package.clear();
        for(char i : com)
            package.push_back(i);

        rexmit_mutex.lock();
        if(!missing.empty())
        {
            bool first = true;
            auto curr_time = getTime();
            for(auto elem : missing)
            {
                if(elem.second + RTIME < curr_time)
                {
                    if(first) first = false;
                    else package.push_back(',');

                    uint64_t num = elem.first;
                    auto num_string = to_string(num);
                    for(unsigned i=0; i<8; i++)
                        package.push_back(num_string[i]);
                }
                if(package.size() > 50000)
                    break;
            }

            if(!first)
            {
                package.push_back('\n');

                auto snd_addr_len = (socklen_t) sizeof(DISCOVER_ADDR);
                auto buffer = reinterpret_cast<char*>(package.data());
                auto snd_bytes = sendto(control_sock, buffer, package.size(), 0, (struct sockaddr*) &DISCOVER_ADDR, snd_addr_len);
                if (snd_bytes != package.size())
                    syserr("partial / failed write on send (reexmiter.cc)");
            }
        }
        rexmit_mutex.unlock();

        // sleep
        struct timespec req = {0};
        req.tv_sec = 0;
        req.tv_nsec = milisec * 1000000L;
        nanosleep(&req, (struct timespec *) nullptr);
    }
}
