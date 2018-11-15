#ifndef RADIO_MENU_H
#define RADIO_MENU_H

#include <iostream>
#include <map>
#include <netinet/in.h>
#include <zconf.h>
#include <poll.h>
#include <mutex>

using namespace std;

#define BUFFER_SIZE 1000

class Menu
{

    map<string,pair<unsigned long long,pair<string,uint16_t>>>& stations;
    const char *discover_dotted_addr;
    struct sockaddr_in DISCOVER_ADDR;
    struct pollfd client[_POSIX_OPEN_MAX];
    uint16_t CTRL_PORT;
    uint16_t UI_PORT;
    int BSIZE;
    int RTIME;
    int data_sock;
    int active_clients;
    string& currently_played;
    string& should_be_playing;
    bool& station_changed;
    bool& station_list_changed;
    mutex& station_mutex;
    u_int yes=1;

public:
    Menu(map<string, pair<unsigned long long int, pair<string, uint16_t>>> &stations, const char *discover_dotted_addr,
         const sockaddr_in &DISCOVER_ADDR, uint16_t CTRL_PORT, uint16_t UI_PORT, int BSIZE, int RTIME, int data_sock,
         string& currently_played, string& should_be_playing, bool& station_changed, bool& station_list_changed,
         mutex& station_mutex);

    void sendToClient(int msg_sock, std::string msg);

    void drawMenu();

    void initializeClients();

    void acceptNewClients();

    void handleClients();

    void ShowMenus();
};

#endif //RADIO_MENU_H
