#include "lolmsg.h"

#include <stdio.h>
#include <string.h>
//#include "printer.h"

#define CHECKSUMINIT 0x00ff
#define MSGSTART 0xa0
#define MSGSTARTMASK 0xf0

#define MSGISVARIABLE 1
#define LOLVAROVERHEAD 9

// Serialized LOLMessage bytes
// byte 0:
//   bits 0-3 : MSGSTART (0xa)
//   bits 4-7 : Message type and reserved bits
// byte 1: address
// byte 2: command
// byte 3-6: data length
// byte 7: header checksum
// byte 8-x: data
// byte x+1: data checksum

//work round for 64bit machine
const static uint32_t dataOffsetPose = sizeof(LolMessage);

void lolmsgInit(LolMessage* msg, uint8_t address, uint8_t command, uint8_t* data, uint32_t length)
{
    msg->command = command;
    msg->address = address;
    msg->length = length;
    msg->data = data;
}

int lolmsgSerializedSize(LolMessage* msg)
{
    if (msg->length > 0)
        return LOLVAROVERHEAD + msg->length;
    else
        return LOLVAROVERHEAD - 1;
}

inline uint16_t checksumByte(uint16_t checksum, uint8_t b)
{
    return checksum ^= b;
}

inline uint16_t checksumBytes(uint16_t checksum, uint8_t* b, uint32_t count)
{
    while (count-- != 0)
        checksum ^= *b++;
    return checksum;
}

int lolmsgSerialize(LolMessage* msg, uint8_t* outbytes)
{
    uint8_t* bytes = outbytes;
    uint16_t checksum;
    uint8_t b = MSGSTART | MSGISVARIABLE;
    *bytes++ = b;
    checksum = checksumByte(CHECKSUMINIT, b);
    b = msg->address;
    *bytes++ = b;
    checksum = checksumByte(checksum, b);
    b = msg->command;
    *bytes++ = b;
    checksum = checksumByte(checksum, b);
    b = (msg->length >> 24) & 0xFF;
    *bytes++ = b;
    checksum = checksumByte(checksum, b);
    b = (msg->length >> 16) & 0xFF;
    *bytes++ = b;
    checksum = checksumByte(checksum, b);
    b = (msg->length >> 8) & 0xFF;
    *bytes++ = b;
    checksum = checksumByte(checksum, b);
    b = msg->length;
    *bytes++ = b;
    checksum = checksumByte(checksum, b);
    *bytes++ = (uint8_t)(checksum & 0xff);
    if (msg->length > 0)
    {
        checksum = CHECKSUMINIT;
        const uint8_t* data = msg->data;
        uint32_t i;
        for (i = 0; i < msg->length; i++)
        {
            b = *data++;
            *bytes++ = b;
            checksum = checksumByte(checksum, b);
        }
        *bytes++ = (uint8_t)(checksum & 0xff);
    }
    return bytes - outbytes;
}

void lolmsgParseInit(LolParseContext* ctx, uint8_t* buf, uint32_t bufLength)
{
    ctx->buf = buf;
    ctx->bufLength = bufLength;
    ctx->state = LOLPARSE_HEADER;
}

void lolmsgParseByte(LolParseContext* ctx, uint8_t byte)
{
    lolmsgParse(ctx, &byte, 1);
}

// The parse function FROM HELL!
// Beware all kinds of evil performance hacks.
int lolmsgParse(LolParseContext* ctx, uint8_t* bytes, uint32_t inlength)
{
    uint8_t* outbytes = ctx->buf;
    LolMessage* outmsg = (LolMessage*) ctx->buf;
    uint8_t* inbytes = (uint8_t*)bytes;

    // Local copies of object variables
    // Must be written back on return!
    uint32_t pos = ctx->pos;
    uint16_t checksum = ctx->checksum;
    ParseState state = ctx->state;

    uint32_t parsed = 0;
    switch (state)
    {
        default:
            state = LOLPARSE_HEADER;
            // intentional fall through!
        case LOLPARSE_HEADER:
            {
                // Parse the first byte. This tells us which message type we have.
                if (inlength <= 0)
                    goto end;

                uint8_t firstbyte = *inbytes++;
                checksum = checksumByte(CHECKSUMINIT, firstbyte);
                parsed++;
                pos = 0;

                if ((firstbyte & MSGSTARTMASK) != MSGSTART)
                {	// Some unused bits are used as start flag, helps us to 
                    // to get synchronized quicker if we get frame shifted.
                    state = LOLPARSE_ERR_NOSTART;
                    goto end;
                }
                
                if (firstbyte & MSGISVARIABLE)
                {
                    state = LOLPARSE_VARIABLE;
                    goto case_variable;
                }

            }
            break;
        case LOLPARSE_VARIABLE: case_variable:
            // Here we parse the header of a variable type message
                                while (parsed < inlength)
                                {
                                    uint8_t inbyte = *inbytes++;
                                    parsed++;
                                    //    printf("parsed: %#x -- %#x\n", parsed, inbyte);
                                    switch (pos)
                                    {
                                        case 0:
                                            outmsg->address = inbyte;
                                            break;
                                        case 1:
                                            outmsg->command = inbyte;
                                            break;
                                        case 2:
                                            outmsg->length = inbyte;
                                            break;
                                        case 3:
                                            outmsg->length = outmsg->length<<8 | inbyte;
                                            break;
                                        case 4:
                                            outmsg->length = outmsg->length<<8 | inbyte;
                                            break;
                                        case 5:
                                            outmsg->length = outmsg->length<<8 | inbyte;
                                            break;
                                        case 6:
                                            if (inbyte != checksum)
                                            {
                                                state = LOLPARSE_ERR_CHECKSUM;
                                                goto end;
                                            }
                                            else if (outmsg->length > ctx->bufLength - 10)
                                            {
                                                state = LOLPARSE_ERR_BUFTOOSMALL;
                                                goto end;
                                            }
                                            else if (outmsg->length == 0)
                                            {
                                                state = LOLPARSE_VARIABLE_COMPLETE;
                                                goto end;
                                            }
                                            else
                                            {
                                                outmsg->data = outbytes + dataOffsetPose; 
                                                pos = dataOffsetPose;
                                                checksum = CHECKSUMINIT;
                                                state = LOLPARSE_VARIABLE_PL;
                                                goto case_variable_pl;
                                            }
                                    }
                                    checksum = checksumByte(checksum, inbyte);
                                    pos++;
                                }
                                break;
        case LOLPARSE_VARIABLE_PL : case_variable_pl:
                                    // Here we parse the payload of a variable type message
                                    {
                                        uint32_t end = dataOffsetPose + outmsg->length;
                                        while (parsed < inlength)
                                        {
                                            uint8_t inbyte = *inbytes++;
                                            parsed++;

                                            //printf("pos: %d parsed: %#x -- %#x\n", pos, parsed, inbyte);
                                            if (pos >= end)
                                            {
                                                if (inbyte != checksum)
                                                    state = LOLPARSE_ERR_CHECKSUM;
                                                else
                                                {
                                                    state = LOLPARSE_VARIABLE_COMPLETE;
                                                }
                                                goto end;
                                            }

                                            outbytes[pos++] = inbyte;
                                            checksum = checksumByte(checksum, inbyte);
                                        }
                                    }
                                    break;
    }

end:
    ctx->pos = pos;
    ctx->checksum = checksum;
    ctx->state = state;
    return parsed;
}


void printLolMessage(const LolMessage *msg)
{
    printf("command: %#x\n", msg->command);
    printf("address: %#x\n", msg->address);
    printf("data: %d bytes (", msg->length);
    uint32_t i;
    for(i=0;i<msg->length;i++)
        printf("%#x\t", msg->data[i]);
    printf(")\n");
}
