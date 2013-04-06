#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ipc.hh>

#include "ipc_interface.hh"
#define MAX_CLIENT 4

static void ServerConnection(const LolMessage*msg, IPC::Connection *conn,  void *ptr);
static void ClientConnection(const LolMessage*msg, IPC::Connection *conn, void *ptr);

int main(int argc, const char**argv)
{
    bool is_client = false;
    IPC::IPC client_ipc;
    IPC::IPC monitor_ipc;

    if(argc == 3 && strcmp(argv[1], "-c")==0)
    {
        is_client = true;
        printf("I am client tring to connect to %s:%d\n", argv[2], 10000);
        client_ipc.SetCallback(ClientConnection, (void*)&client_ipc);
        client_ipc.Start(argv[2], 10000, false);

    }
    else if(argc == 2 && strcmp(argv[1], "-h")==0)
    {
        printf("I am a server\n");

            monitor_ipc.SetCallback(ServerConnection, (void*)&monitor_ipc);
            monitor_ipc.Start("localhost", 10000, true);
    }
    else
    {
        printf("help:\n");
        printf("main -c host  :start as client\n");
        printf("main -h  :start as server\n");
        exit(1);
    }

    //wait for ever
    while(1)
    {
        usleep(1000000);
        if(is_client)
        {
            printf("Request Link_info\n");
            unsigned char data[2]={0};
            client_ipc.SendData(IPC::LINK_INFO_REQ, data, 2);
        }

    }
    return 0;
}


void ServerConnection(const LolMessage* msg, IPC::Connection* conn, void *ptr)
{
    printf("received something\n");
    IPC::IPC *monitor_ipc = (IPC::IPC*)ptr;
    switch(msg->command)
    {
        case IPC::LINK_INFO_REQ:
            {
                unsigned char data[10] = {0,1,2,3,4,5,6,7,8,9};
                conn->SendData(IPC::LINK_INFO, data, 10);
            }
            break;
        default:
            break;
    }
}

void ClientConnection(const LolMessage*msg, IPC::Connection * conn, void *ptr)
{
    IPC::IPC *client_ipc = (IPC::IPC*)ptr;
    switch(msg->command)
    {
        case IPC::LINK_INFO:
            {
                for(int i=0;i<msg->length;i++)
                    printf("%d\t", msg->data[i]);
                printf("\n");
            }
            break;
        default:
            break;
    }
}
