#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ipc.hh>
#include <signal.h>

#include "ipc_interface.hh"

bool userQuit = false;
int count=0;
void signalHandler(int dummy);
static void ServerConnection(const LolMessage*msg,void *conn, void *ptr);
static void ClientConnection(const LolMessage*msg, void *conn, void *ptr);

int main(int argc, const char**argv)
{
    if (signal(SIGINT, signalHandler) == SIG_ERR)
    {
        printf("signal(2) failed while setting up for SIGINT");
        return -1;
    }

    if (signal(SIGTERM, signalHandler) == SIG_ERR)
    {
        printf("signal(2) failed while setting up for SIGTERM");
        return -1;
    }

    bool is_client = false;
    IPC::IPC client_ipc;
    IPC::IPC monitor_ipc;
    int port=10000;

    while(count<3)
    {
        if(argc == 3 && strcmp(argv[1], "-c")==0)
        {
            is_client = true;
            printf("I am client tring to connect to %s:%d\n", argv[2], port);
            client_ipc.SetCallback(ClientConnection, (void*)&client_ipc);
            client_ipc.Start(argv[2], port, false);
        }
        else if(argc == 2 && strcmp(argv[1], "-h")==0)
        {
            printf("I am a server\n");

            monitor_ipc.SetCallback(ServerConnection, (void*)&monitor_ipc);
            monitor_ipc.Start("localhost", port, true);
        }
        else
        {
            printf("help:\n");
            printf("main -c [portid: 0 - 3]  :start as client\n");
            printf("main -h  :start as server\n");
            exit(1);
        }
        usleep(1000000);

        //wait for ever
        while(!userQuit)
        {
            if(is_client && client_ipc.Connections()->size()>0)
            {
                //printf("Request Link_info\n");
                unsigned char data[2]={0};
                client_ipc.SendData(IPC::LINK_INFO_REQ, data, 2);
            }
            usleep(1000000);
        }

        userQuit = false;

        if(is_client)
        {
            client_ipc.Stop();
        }
        else
            monitor_ipc.Stop();
        
        usleep(1000000);

    }
    printf("this is normal exit\n");


    return 0;
}


void ServerConnection(const LolMessage*msg,void *conn, void *ptr)
{
  //  printf("received something\n");
    //IPC::IPC *monitor_ipc = (IPC::IPC*)ptr;
    switch(msg->command)
    {
        case IPC::LINK_INFO_REQ:
            {
                unsigned char data[10] = {0,1,2,3,4,5,6,7,8,9};
               (( IPC::Connection*)conn)->SendData(IPC::LINK_INFO, data, 10);
            }
            break;
        default:
            break;
    }
}

void ClientConnection(const LolMessage*msg, void *conn, void *ptr)
{
    //IPC::IPC *client_ipc = (IPC::IPC*)ptr;
    switch(msg->command)
    {
        case IPC::LINK_INFO:
            {
                printf("\t");
                for(int i=0;i<msg->length;i++)
                    printf("%d\t", msg->data[i]);
                printf("\n");
            }
            break;
        default:
            break;
    }
}
void signalHandler(int dummy)
{
 //   printf("user quit signals captured: ctrl + c \n");
    userQuit = true;
    count++;
}
