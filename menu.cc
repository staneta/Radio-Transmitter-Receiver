#include <zconf.h>
#include <poll.h>
#include "menu.h"
#include "err.h"

Menu::Menu(map<string, pair<unsigned long long int, pair<string, uint16_t>>> &stations,
           const char *discover_dotted_addr, const sockaddr_in &DISCOVER_ADDR, uint16_t CTRL_PORT, uint16_t UI_PORT,
           int BSIZE, int RTIME, int data_sock, string& currently_played, string& should_be_playing, bool& station_changed,
           bool& station_list_changed, mutex& station_mutex)
        : stations(stations), discover_dotted_addr(discover_dotted_addr), DISCOVER_ADDR(DISCOVER_ADDR),
          CTRL_PORT(CTRL_PORT), UI_PORT(UI_PORT), BSIZE(BSIZE), RTIME(RTIME), data_sock(data_sock),
          currently_played(currently_played), should_be_playing(should_be_playing), station_changed(station_changed), station_list_changed(station_list_changed),
          station_mutex(station_mutex) {}


void Menu::sendToClient(int msg_sock, std::string msg)
{
    ssize_t snd_len = write(msg_sock, msg.c_str(), msg.size());
    if (snd_len != (ssize_t)msg.size())
        syserr("writing to client socket");
}


void Menu::drawMenu()
{
    string menu;
    menu = "------------------------------------------------------------------------\r\n";
    menu += "SIK Radio\r\n------------------------------------------------------------------------\r\n";
    station_mutex.lock();
    for(const auto& station : stations)
    {
        if(station.first == currently_played) menu += "  > ";
        else menu += "    ";
        menu += station.first + "\r\n";
    }
    station_mutex.unlock();
    menu += "------------------------------------------------------------------------\r\n";

    for(int j = 1; j < _POSIX_OPEN_MAX; ++j)
    {
        if(client[j].fd != -1)
        {
            sendToClient(client[j].fd, "\u001B[2J"); // clear
            sendToClient(client[j].fd, "\e[f"); // cursor on top
            sendToClient(client[j].fd, menu);
        }
    }
}


void Menu::initializeClients()
{
    /* Inicjujemy tablicę z gniazdkami klientów, client[0] to gniazdko centrali */
    for(int i = 0; i < _POSIX_OPEN_MAX; ++i)
    {
        client[i].fd = -1;
        client[i].events = POLLIN;
        client[i].revents = 0;
    }
    active_clients = 0;


    /* Tworzymy gniazdko centrali */
    client[0].fd = socket(PF_INET, SOCK_STREAM, 0);
    if(client[0].fd < 0)
        syserr("Opening stream socket");


    /* Co do adresu nie jesteśmy wybredni */
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(UI_PORT);
    if(bind(client[0].fd, (struct sockaddr*)&server, (socklen_t)sizeof(server)) < 0)
        syserr("Binding stream socket");


    /* Zapraszamy klientów */
    if(listen(client[0].fd, 5) == -1)
        syserr("Starting to listen");
}


void Menu::acceptNewClients()
{
    int msg_sock;
    int i;

    if(client[0].revents & POLLIN)
    {
        msg_sock = accept(client[0].fd, (struct sockaddr*)0, (socklen_t*)0);
        if (msg_sock == -1)
            perror("accept");
        else
        {
            for(i=1; i<_POSIX_OPEN_MAX; ++i)
            {
                if(client[i].fd == -1)
                {
                    client[i].fd = msg_sock;
                    active_clients += 1;
                    sendToClient(msg_sock, "\377\375\042\377\373\001"); // character mode
                    break;
                }
            }
            if (i >= _POSIX_OPEN_MAX)
            {
                fprintf(stderr, "Too many clients\n");
                if (close(msg_sock) < 0)
                    perror("close");
            }
        }
    }
}


void Menu::handleClients()
{
    char buffer[3];

    for(int i=1; i<_POSIX_OPEN_MAX; ++i)
    {
        if(client[i].fd != -1 && (client[i].revents & (POLLIN | POLLERR)))
        {
            auto rval = read(client[i].fd, buffer, 3);
            if(rval < 0)
            {
                perror("Reading stream message");
                if (close(client[i].fd) < 0)
                    perror("close");
                client[i].fd = -1;
                active_clients -= 1;
            }
            else if(rval == 0)
            {
                fprintf(stderr, "Ending connection\n");
                if (close(client[i].fd) < 0)
                    perror("close");
                client[i].fd = -1;
                active_clients -= 1;
            }
            else
            {
                if(rval == 3 && buffer[0] == 27 && buffer[1] == 91 && buffer[2] == 66) // w dol
                {
                    station_mutex.lock();
                    bool next = false;
                    for(const auto& station : stations)
                    {
                        if(next)
                        {
                            currently_played = station.first;
                            break;
                        }
                        if(station.first == currently_played)
                            next = true;
                    }
                    station_changed = true;
                    station_list_changed = true;
                    station_mutex.unlock();
                }

                if(rval == 3 && buffer[0] == 27 && buffer[1] == 91 && buffer[2] == 65) // w gore
                {
                    station_mutex.lock();
                    string last = currently_played;
                    for(const auto& station : stations)
                    {
                        if(station.first == currently_played)
                        {
                            if(last != currently_played)
                                currently_played = last;
                            break;
                        }
                        last = station.first;
                    }
                    station_changed = true;
                    station_list_changed = true;
                    station_mutex.unlock();
                }
                drawMenu();
            }
        }
    }
}


void Menu::ShowMenus()
{
    int ret, i;

    initializeClients();

    /* Do pracy */
    while(true)
    {
        for(i = 0; i < _POSIX_OPEN_MAX; ++i)
            client[i].revents = 0;

        /* Czekamy przez 5000 ms */
        ret = poll(client, _POSIX_OPEN_MAX, 1000);
        if (ret < 0)
            perror("poll");
        else if (ret > 0)
        {
            acceptNewClients();
            handleClients();
        }
        else
        {
            // timeout
            if(station_list_changed)
            {
                drawMenu();
                station_list_changed = false;
            }
        }
    }

    if (client[0].fd >= 0)
        if (close(client[0].fd) < 0)
            perror("Closing main socket");
    exit(EXIT_SUCCESS);
}
