#ifndef RADIO_PLAYER_H
#define RADIO_PLAYER_H

#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include "buffer.h"

using namespace std;

class Player
{
    Buffer& data_buffer;
    bool& stop_playing;
    bool& stop_receiving;
    mutex& data_mutex;
    uint64_t last;
    bool first_one = true;

public:

    Player(Buffer &data_buffer, bool &stop_playing, bool &stop_receiving, mutex& data_mutex);

    void stopPlaying();

    void Play();
};


#endif //RADIO_PLAYER_H
