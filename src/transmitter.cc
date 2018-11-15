#include <vector>
#include <deque>
#include "transmitter.h"
#include "err.h"

Transmitter::Transmitter(const string &NAME, uint64_t session_id, uint16_t DATA_PORT, uint16_t CTRL_PORT, int PSIZE,
                         int FSIZE, int RTIME, int data_sock, char *mcast_dotted_addr, const sockaddr_in &MCAST_ADDR,
                         deque<pair<uint64_t, vector<char>>> &last_bytes, bool& transmitterRunning)
        : NAME(NAME), session_id(session_id), DATA_PORT(DATA_PORT), CTRL_PORT(CTRL_PORT), PSIZE(PSIZE), FSIZE(FSIZE),
          RTIME(RTIME), data_sock(data_sock), mcast_dotted_addr(mcast_dotted_addr), MCAST_ADDR(MCAST_ADDR),
          last_bytes(last_bytes), transmitterRunning(transmitterRunning) {}


void Transmitter::resetData(vector<char> &data, uint64_t first_byte_num)
{
    data.clear();
    uint64_t session_id_big_end = htobe64(session_id);
    uint64_t first_byte_num_big_end = htobe64(first_byte_num);

    auto id_bytes = reinterpret_cast<char*>(&session_id_big_end);
    auto first_bytes = reinterpret_cast<char*>(&first_byte_num_big_end);

    for(unsigned i=0; i<8; i++)
        data.push_back(id_bytes[i]);
    for(unsigned i=0; i<8; i++)
        data.push_back(first_bytes[i]);
}


void Transmitter::sendSong()
{
    vector<char> data;
    uint64_t num=0, last_first_num=0;
    char byte;

    while(scanf("%c", &byte) == 1)
    {
        if(num % PSIZE == 0)
        {
            resetData(data, num);
        }

        data.push_back(byte);
        num++;

        if(num % PSIZE == 0 && num != 0)
        {
            if(last_bytes.size() == (unsigned)(FSIZE/PSIZE))
                last_bytes.pop_front();

            char* data_char = reinterpret_cast<char*>(data.data());
            last_bytes.emplace_back(last_first_num, data);
            last_first_num = num;

            // send song
            auto len = data.size(); //(size_t) PSIZE + 16;
            auto sflags = 0;
            auto rcva_len = (socklen_t) sizeof(MCAST_ADDR);
            auto snd_len = sendto(data_sock, data_char, len, sflags, (struct sockaddr*) &MCAST_ADDR, rcva_len);
            if (snd_len != (ssize_t) len)
                syserr("partial / failed write");
        }
    }
    transmitterRunning = false;
}
