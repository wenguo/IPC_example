/*
 *      Created on: 01/2012
 *          Author: Wenguo Liu (wenguo.liu@brl.ac.uk)
 *
*/
#include <pthread.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <sstream>
#include <iostream>
#include "ipc.hh"

namespace IPC{

bool IPC::Start(const char* h, int p, bool s)
{
    port = p;
    server = s;
    if(h)
        host=strdup(h);
    else
        host=NULL;

    pthread_mutex_init(&mutex_txq, NULL);
    //create monitoring thread
    pthread_create(&monitor_thread, 0, Monitoring, this);

    return true;
}

void * IPC::Monitoring(void * ptr)
{
    IPC* ipc = (IPC*)ptr;

    bool ret=false;
    if(ipc->server)
        ret = ipc->StartServer(ipc->port);
    else
        ret = ipc->ConnectToServer(ipc->host, ipc->port);
}

void * IPC::Receiving(void * p)
{

    user_pointer * ptr = (user_pointer*)p;
    IPC * ipc = (IPC*)(ptr->ipc);
    printf("create receiving thread %d\n", ptr->sock);

    //main loop, keep reading
    unsigned char rx_buffer[IPCBLOCKSIZE];

    LolParseContext parseContext;
    lolmsgParseInit(&parseContext, new uint8_t[IPCLOLBUFFERSIZE], IPCLOLBUFFERSIZE);

    while(1)
    {
        //reading
        memset(rx_buffer, 0, IPCBLOCKSIZE);
        int received = read(ptr->sock,rx_buffer, IPCBLOCKSIZE);
        if (received <= 0) 
        {
            printf("ERROR read to socket : %d -- it seems connection lost\n", received);
            //ipc->connected = false;
            break;
        }
        else
        {
            
            int parsed = 0;
            while (parsed < received)
            {
                parsed += lolmsgParse(&parseContext, rx_buffer + parsed, received - parsed);
                LolMessage* msg = lolmsgParseDone(&parseContext);
                if(msg!=NULL && ipc->callback)
                {
                    printf("received data from %s : %d\n",inet_ntoa(ptr->addr.sin_addr),ntohs(ptr->addr.sin_port));
                    ipc->callback(msg, ipc->user_data);
                }
            }
        }
    }

    printf("Exit monitoring thread\n");

    pthread_exit(NULL);
}

void * IPC::Transmiting(void *p)
{
    user_pointer * ptr = (user_pointer*)p;
    IPC * ipc = (IPC*)ptr->ipc;
    printf("create transmiting thread %d\n", ptr->sock);
    uint8_t txBuf[IPCBLOCKSIZE];

    while(1)
    {
            pthread_mutex_lock(&ipc->mutex_txq);
        if(BQCount(&ipc->txq) > 0 )
        {
            register int byteCount = BQPopBytes(&ipc->txq, txBuf, IPCBLOCKSIZE);

            int n = write(ptr->sock, txBuf, byteCount);
            if(n<0)
                printf("write error %d\n", n);
        }
            pthread_mutex_unlock(&ipc->mutex_txq);

        usleep(100000);
    }
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

        memset((char*) &serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = INADDR_ANY;
        serv_addr.sin_port = htons(port);
        if(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
        {
            printf("ERROR on binding, try again in 1 second\n");
            binded = false;
            close(sockfd);
            usleep(1000000);
        }
        else
            binded  = true;
    }

    if(sockfd<0)
        return false;

    printf("Listening on port %d\n", port);

    socklen_t clilen;
    //listening for connection
    listen(sockfd,5);

    while(1)
    {
        struct sockaddr_in client;
        clilen = sizeof(client);
        clientsockfd = accept(sockfd, (struct sockaddr *) &client, &clilen);

        printf("accept\n");
        if (clientsockfd < 0) 
        {
            printf("ERROR on accept\n");
        }
        else
        {
            client_sockfds.push_back(clientsockfd);
            user_pointer *ptr = new user_pointer;
            ptr->ipc = this;
            ptr->sock = clientsockfd;
            ptr->addr = client;

            pthread_create(&(receiving_thread), 0, Receiving, ptr);
            pthread_create(&(transmiting_thread), 0, Transmiting, ptr);
        }

    }
}

bool IPC::ListenForConnection(int sockfd)
{
    if(sockfd<0)
        return false;

    printf("Listening on port %d\n", port);

    socklen_t clilen;
    //listening for connection
    listen(sockfd,5);

    struct sockaddr_in cli_addr;
    clilen = sizeof(cli_addr);
    clientsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
    printf("accept\n");
    if (clientsockfd < 0) 
    {
        printf("ERROR on accept\n");
        return false;
    }
    else
        return true;

}

bool IPC::ConnectToServer(const char * host, int port)
{
    struct sockaddr_in serv_addr;
    struct hostent *server;
    server = gethostbyname(host);
    
    printf("Trying to connect to Server [%s:%d]\n", host, port);

    clientsockfd = socket(AF_INET, SOCK_STREAM, 0);
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

    client_sockfds.push_back(clientsockfd);
    user_pointer *ptr = new user_pointer;
    ptr->ipc = this;
    ptr->sock = clientsockfd;
    ptr->addr = serv_addr;

    pthread_create(&receiving_thread, 0, Receiving, ptr);
    pthread_create(&transmiting_thread, 0, Transmiting, ptr);
    return true;
}

bool IPC::SendData(const uint8_t type, uint8_t *data, int data_size)
{
	printf("send data\n");
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

}//end of namespace
