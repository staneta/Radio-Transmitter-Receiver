#include <zconf.h>
#include "player.h"

Player::Player(Buffer &data_buffer, bool &stop_playing, bool &stop_receiving, mutex& data_mutex)
        : data_buffer(data_buffer), stop_playing(stop_playing), stop_receiving(stop_receiving),
          data_mutex(data_mutex) {}


void Player::stopPlaying()
{
    stop_receiving = true;
}


void Player::Play()
{
    while(!stop_playing)
    {
        int i;
        if(data_buffer.getSize() != 0)
        {
            i = data_buffer.getBeg();

            auto package = data_buffer.buf[i];
            if(package.second != nullptr)
            {
                if(!first_one && package.first != last + data_buffer.getPSIZE())
                {
                    stopPlaying();
                    break;
                }
                last = package.first;
                if(first_one)
                    first_one = false;

                for(int j = 16; j < data_buffer.getPSIZE() + 16; j++)
                    printf("%c", package.second[j]);

                if(data_buffer.buf[i].first == package.first)
                {
                    data_buffer.buf[i].first = 0;
                    data_buffer.buf[i].second = nullptr;
                }

                data_buffer.sub();
            }
            else
            {
                stopPlaying();
                break;
            }
        }
        else
        {
            stopPlaying();
            break;
        }
        //data_mutex.unlock();
    }
}



