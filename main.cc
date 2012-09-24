#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ipc.hh>

#include "ipc_interface.hh"
#define MAX_CLIENT 4

static void ServerConnection(const LolMessage*msg, void *ptr);
static void ClientConnection(const LolMessage*msg, void *ptr);

int main(int argc, const char**argv)
{
    bool is_client = false;
    IPC::IPC client_ipc;
    IPC::IPC monitor_ipc[MAX_CLIENT];
    int port[MAX_CLIENT]={10000,10001,10002,10003};

    if(argc == 4 && strcmp(argv[1], "-c")==0)
    {
        is_client = true;
        int port_index;
        sscanf(argv[3], "%d", &port_index);
        port_index = port_index % 4; //in case not [0-3] is given
        printf("I am client tring to connect to %s:%d\n", argv[2], port[port_index]);
        client_ipc.SetCallback(ClientConnection, (void*)&client_ipc);
        client_ipc.Start(argv[2], port[port_index], false);

    }
    else if(argc == 2 && strcmp(argv[1], "-h")==0)
    {
        printf("I am a server\n");

        for(int i=0;i<MAX_CLIENT;i++)
        {
            monitor_ipc[i].SetCallback(ServerConnection, (void*)&monitor_ipc[i]);
            monitor_ipc[i].Start("localhost", port[i], true);
        }
    }
    else
    {
        printf("help:\n");
        printf("main -c [portid: 0 - 3]  :start as client\n");
        printf("main -h  :start as server\n");
        exit(1);
    }

    //wait for ever
    while(1)
    {
        if(is_client)
        {
            printf("Request Link_info\n");
            unsigned char data[2]={0};
            client_ipc.SendData(IPC::LINK_INFO_REQ, data, 2);
        }

        usleep(1000000);
    }
    return 0;
}


void ServerConnection(const LolMessage*msg, void *ptr)
{
    printf("received something\n");
    IPC::IPC *monitor_ipc = (IPC::IPC*)ptr;
    switch(msg->command)
    {
        case IPC::LINK_INFO_REQ:
            {
                unsigned char data[10] = {0,1,2,3,4,5,6,7,8,9};
                monitor_ipc->SendData(IPC::LINK_INFO, data, 10);
            }
            break;
        default:
            break;
    }
}

void ClientConnection(const LolMessage*msg, void *ptr)
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
