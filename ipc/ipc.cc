/*
 *      Created on: 01/2012
 *          Author: Wenguo Liu (wenguo.liu@brl.ac.uk)
 *
 */
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros
#include <netdb.h>
#include <unistd.h>
#include <sstream>
#include <iostream>
#include "ipc.hh"

namespace IPC{

#define Close(fd) {\
    printf("\tclose socket %d from line %d\n",fd, __LINE__);\
    close(fd);\
}

#define Shutdown(fd, val){\
    printf("\tshutdown socket %d from line %d\n", fd, __LINE__);\
    int ret=shutdown(fd,SHUT_RDWR);\
    if(ret<0){\
    perror("error");\
    close(fd);}\
}


Connection::Connection()
{
    ipc = NULL;
    callback = NULL;
    connected = true;
    BQInit(&txq, txbuffer, IPCTXBUFFERSIZE);
    pthread_mutex_init(&mutex_txq, NULL);
    user_data = NULL;
    transmiting_thread_running = false;
    receiving_thread_running = false;
}
Connection::~Connection()
{
    //clean up
    printf("connection removed %s:%d\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
    connected = false;
    if(sockfds>=0)
    {
        printf("connection shutdown %d\n", sockfds);
        Close(sockfds);
    }
}

bool Connection::Start()
{
    pthread_create(&receiving_thread, 0, Receiving, this);
    pthread_create(&transmiting_thread, 0, Transmiting, this);
    return true;
}

IPC::IPC()
{
    sockfd = -1;
    port = 10000;
    host = NULL;
    server = true;
    callback = NULL;
    user_data = NULL;
    monitoring_thread_running = false;
}

IPC::~IPC()
{
    //cleanup
}

bool IPC::Start(const char* h, int p, bool s)
{
    port = p;
    server = s;
    if(h)
        host=strdup(h);
    else
        host=NULL;

    //create monitoring thread
    pthread_create(&monitor_thread, 0, Monitoring, this);
    return true;
}

void * IPC::Monitoring(void * ptr)
{
    IPC* ipc = (IPC*)ptr;

    ipc->monitoring_thread_running = true;

    bool ret=false;
    if(ipc->server)
        ret = ipc->StartServer(ipc->port);
    else
        ret = ipc->ConnectToServer(ipc->host, ipc->port);

    ipc->monitoring_thread_running = false;

    return NULL;
}

void * Connection::Receiving(void * p)
{

    Connection * ptr = (Connection*)p;
    printf("create receiving thread for %s:%d\n",inet_ntoa(ptr->addr.sin_addr), ntohs(ptr->addr.sin_port));

    //main loop, keep reading
    unsigned char rx_buffer[IPCBLOCKSIZE];

    lolmsgParseInit(&ptr->parseContext, new uint8_t[IPCLOLBUFFERSIZE], IPCLOLBUFFERSIZE);
    ptr->receiving_thread_running = true;

    while(ptr->connected)
    {
        //reading
        memset(rx_buffer, 0, IPCBLOCKSIZE);
        int received = read(ptr->sockfds,rx_buffer, IPCBLOCKSIZE);
        if (received <= 0) 
        {
            printf("ERROR read to socket %d : %d -- it seems connection lost\n",ptr->sockfds, received);
            ptr->connected = false;
            break;
        }
        else
        {
            int parsed = 0;
            while (parsed < received)
            {
                parsed += lolmsgParse(&ptr->parseContext, rx_buffer + parsed, received - parsed);
                LolMessage* msg = lolmsgParseDone(&ptr->parseContext);
                if(msg!=NULL && ptr->callback)
                {
                    // printf("received data from %s : %d\n",inet_ntoa(ptr->addr.sin_addr),ntohs(ptr->addr.sin_port));
                    ptr->callback(msg, ptr, ptr->user_data);
                }
            }
        }
    }

    printf("exit receiving thread for %s:%d\n",inet_ntoa(ptr->addr.sin_addr), ntohs(ptr->addr.sin_port));
    ptr->receiving_thread_running = false;
    pthread_exit(NULL);
}

void * Connection::Transmiting(void *p)
{
    Connection * ptr = (Connection*)p;
    printf("create transmiting thread for %s:%d\n", inet_ntoa(ptr->addr.sin_addr), ntohs(ptr->addr.sin_port));
    uint8_t txBuf[IPCBLOCKSIZE];

    ptr->transmiting_thread_running = true;

    while(ptr->connected)
    {
        pthread_mutex_lock(&ptr->mutex_txq);
        if(BQCount(&ptr->txq) > 0 )
        {
            register int byteCount = BQPopBytes(&ptr->txq, txBuf, IPCBLOCKSIZE);

            int n = write(ptr->sockfds, txBuf, byteCount);
            if(n<0)
            {
                ptr->connected = false;
                printf("write error %d\n", n);
                break;
            }
        }
        pthread_mutex_unlock(&ptr->mutex_txq);

        usleep(100000);
    }

    printf("exit transmiting thread for %s:%d\n", inet_ntoa(ptr->addr.sin_addr), ntohs(ptr->addr.sin_port));
    ptr->transmiting_thread_running = false;
    pthread_exit(NULL);
}

bool IPC::StartServer(int port)
{
    printf("Start Server @ port: %d\n", port);
    struct sockaddr_in serv_addr;

    bool binded = false;
    //start server
    while(!binded)
    {
        sockfd = socket(AF_INET, SOCK_STREAM,0);
        if(sockfd <0)
        {
            binded =false;
            continue;
        }

        //set sockfd options
        int flag = 1;
        if (setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&flag,sizeof(int)) == -1)
            exit(1);
        printf("flag: %d\n", flag);

        memset((char*) &serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = INADDR_ANY;
        serv_addr.sin_port = htons(port);
        if(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
        {
            printf("ERROR on binding, try again in 1 second\n");
            binded = false;
            Close(sockfd);
            usleep(1000000);
        }
        else
            binded  = true;

    }

    if(sockfd<0)
        return false;

    printf("%d Listening on port %d\n",sockfd, port);

    socklen_t clilen;
    //listening for connection
    listen(sockfd,10);

    while(monitoring_thread_running)
    {
        struct sockaddr_in client;
        clilen = sizeof(client);
        int clientsockfd = accept(sockfd, (struct sockaddr *) &client, &clilen);

        if (clientsockfd < 0) 
        {
            printf("ERROR on accept %d\n", clientsockfd);
        }
        else
        {
            printf("%d accept %d\n", sockfd, clientsockfd);
            Connection *conn = new Connection;
            conn->sockfds = clientsockfd;
            conn->addr = client;
            conn->connected = true;
            conn->SetCallback(callback, user_data);
            connections.push_back(conn);
            conn->Start();
        }

    }

    Close(sockfd);

    return true;
}

bool IPC::ConnectToServer(const char * host, int port)
{
    struct sockaddr_in serv_addr;
    struct hostent *server;
    server = gethostbyname(host);

    printf("Trying to connect to Server [%s:%d]\n", host, port);

    int clientsockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (clientsockfd < 0) 
    {
        printf("ERROR opening socket, exit monitor thread\n");
        return false;
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(port);
    if (connect(clientsockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
    {
        printf("ERROR connecting, exit monitor thread\n");
        return false;
    }
    printf("Success to connect to Server [%s:%d] @ %d\n", host, port, clientsockfd);

    Connection *conn = new Connection;
    conn->sockfds = clientsockfd;
    conn->addr = serv_addr;
    conn->connected = true;
    conn->SetCallback(callback, user_data);
    connections.push_back(conn);
    conn->Start();
    conn->transmiting_thread_running = true;
    conn->receiving_thread_running = true;

    //wait until user quit
    while(monitoring_thread_running)
        usleep(10000);

    Shutdown(conn->sockfds, 0);

    //wait until all thread quit
    while(conn && !conn->transmiting_thread_running && !conn->receiving_thread_running)
        usleep(1000000);

    //  delete conn;
    Close(conn->sockfds);
    return true;
}

bool Connection::SendData(const uint8_t type, uint8_t *data, int data_size)
{
    //printf("Send data [%s] to %s:%d\n", message_names[type], inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
    LolMessage msg;
    lolmsgInit(&msg, 0, type, data, data_size);
    int len = lolmsgSerializedSize(&msg);
    uint8_t buf[len];
    lolmsgSerialize(&msg, buf);

    pthread_mutex_lock(&mutex_txq);
    int write = BQSize(&txq) - BQCount(&txq);
    if (len < write)
        write = len;
    BQPushBytes(&txq, buf, write);       

    pthread_mutex_unlock(&mutex_txq);
    return write == len;

}

bool IPC::SendData(const uint8_t type, uint8_t *data, int data_size)
{
    for(unsigned int i=0; i< connections.size(); i++)
    {
        if(connections[i]->connected)
            connections[i]->SendData(type, data, data_size);
    }
    return true;
}

bool IPC::SendData(const uint32_t dest, const uint8_t type, uint8_t * data, int data_size)
{
    bool ret = false;
    for(unsigned int i=0; i< connections.size(); i++)
    {
        if(connections[i]->addr.sin_addr.s_addr == dest && connections[i]->connected)
        {
            ret = connections[i]->SendData(type, data, data_size);
            break; // assumed only one connection from one address
        }
    }
    return ret;
}

int IPC::RemoveBrokenConnections()
{
    int count = 0;
    std::vector<Connection*>::iterator it = connections.begin();
    while(it != connections.end())
    {
        //remove broken connection
        if(!(*it)->connected &!(*it)->transmiting_thread_running && !(*it)->receiving_thread_running)
        { 
            printf("\tremove broken connection\n");
            delete *it;
            it = connections.erase(it);
            count++;
        }
        else
            it++;
    }
    return count;
}

void IPC::Stop()
{
    monitoring_thread_running = false;
    if(sockfd >0)
    {
        Shutdown(sockfd, 2); //this will stops all connections, so monitor thread will quit

        std::vector<Connection*>::iterator it = connections.begin();
        while(it != connections.end())
        {
            if((*it) !=NULL)
                Shutdown((*it)->sockfds, 2); //this will stops each connections, so their transmit and receiver thread will quit
            it++;
        }
    }

}

}//end of namespace
