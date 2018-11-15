#ifndef RADIO_BUFFER_H
#define RADIO_BUFFER_H

#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <atomic>

using namespace std;

class Buffer
{
    atomic<int> capacity;
    atomic<int> size;
    atomic<int> beg;
    atomic<int> end;
    atomic<int> PSIZE;
    mutex var_mutex;

public:
    vector<pair<uint64_t, shared_ptr<char[]>>> buf;

    explicit Buffer() {}
    explicit Buffer(int cap, int PSZ)
    {
        buf.resize(capacity, make_pair(0, nullptr));
        capacity = cap;
        PSIZE = PSZ;
        size = 0;
        beg = 0;
        end = 0;
    }

    void clear(int cap, int PSZ)
    {
        capacity = cap;
        buf.resize(capacity, make_pair(0, nullptr));
        for(int i=0; i<capacity; i++)
        {
            buf[i].first = 0;
            buf[i].second = nullptr;
        }
        size = 0;
        beg = 0;
        end = 0;
        PSIZE = PSZ;
    }

    int add()
    {
        var_mutex.lock();
        if(size != 0)
            end = (end.load()+1) % capacity;

        if(size == capacity)
        {
            beg = (beg.load()+1) % capacity;
        }
        else
            size++;

        var_mutex.unlock();
        return end;
    }

    void sub()
    {
        var_mutex.lock();
        if(size != 0)
        {
            beg = (beg.load() + 1) % capacity;
            size--;
        }
        var_mutex.unlock();
    }

    int findPos(int pos_to_sub)
    {
        var_mutex.lock();
        if(pos_to_sub >= size) return -1;

        if(pos_to_sub <= end) return end - pos_to_sub;

        int new_to_sub = pos_to_sub - end;
        var_mutex.unlock();
        return capacity - new_to_sub;
    }

    int add1(int i)
    {
        return (i + 1) % capacity;
    }

    int getEnd()
    {
        return end;
    }

    int getBeg()
    {
        return beg;
    }

    int getSize()
    {
        return size;
    }

    int getPSIZE()
    {
        return PSIZE;
    }
};


#endif //RADIO_BUFFER_H
