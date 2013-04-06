/*
 *      Created on: 01/2012
 *          Author: Wenguo Liu (wenguo.liu@brl.ac.uk)
 *
*/
#ifndef IPC_HH
#define IPC_HH

#include <pthread.h>
#include <vector>
#include "lolmsg.h"
#include "bytequeue.h"

#define IPCLOLBUFFERSIZE 320*240*3  
#define IPCTXBUFFERSIZE  10240
#define IPCBLOCKSIZE  10240

namespace IPC{
    typedef struct
    {
        void * ipc;
        sockaddr_in addr; 
        int sock;
    }user_pointer;

class IPC
{

    typedef void (*Callback)(const LolMessage *, void *);

    public:
        IPC():sockfd(-1),clientsockfd(-1),callback(NULL),port(10000),host(NULL)
        {
            BQInit(&txq, txbuffer, IPCTXBUFFERSIZE);
            server = true;
            user_data = NULL;
        }
        ~IPC(){};

        bool Start(const char *host,int port, bool server);
        bool SendData(const uint8_t type, uint8_t *data, int len);
        inline bool SetCallback(Callback c, void * u) {callback = c; user_data = u;}

    private:
        static void * Monitoring(void *ptr);
        static void * Receiving(void *ptr);
        static void * Transmiting(void *ptr);
        static void * Listening(void *ptr);
        bool StartServer(int port);
        bool ConnectToServer(const char * host, int port);
        bool ListenForConnection(int sockfd);
        
        Callback callback;
        void * user_data;

        int sockfd;
        int clientsockfd;
        bool server;
        int port;
        char* host;
        bool connected;
        uint8_t txbuffer[IPCTXBUFFERSIZE];
        ByteQueue txq;
        pthread_t monitor_thread;
        pthread_t receiving_thread;
        pthread_t transmiting_thread;
        pthread_t listening_thread;
        pthread_mutex_t mutex_txq;

        std::vector<int> client_sockfds;

};

}//end of namespace
#endif
