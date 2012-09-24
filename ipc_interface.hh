/*
 *      Created on: 06/2012
 *          Author: Wenguo Liu (wenguo.liu@brl.ac.uk)
 *
*/
#ifndef IPC_INTERFACE_HH
#define IPC_INTERFACE_HH

namespace IPC{

    enum IPC_msg_type
    {
        LINK_INFO_REQ = 0X1,
        LINK_INFO = 0X2,
    };
}

#endif
