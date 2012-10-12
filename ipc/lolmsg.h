#ifndef LOLMSGH
#define LOLMSGH

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif 

///
///	Low Level Message implementation in C for use on MSP and Blackfin.
/// Struct LolMessage is the container for the message and can be serialized using
/// function lolmsgSerialize(). For iterative deserialization from a byte stream, 
/// a parse context is provided with struct LolParseContext.
///
typedef struct LolMessage
{
	union
	{
		uint16_t meta;
		struct
		{
			uint8_t command;
			uint8_t address;
		} __attribute__((packed));
	};
	uint32_t length;
	uint8_t* data;
} __attribute__((packed)) LolMessage;

/// Conveniently initialize a LolMessage struct using these functions
void lolmsgInit(LolMessage* msg, uint8_t address, uint8_t command, uint8_t* data, uint32_t length);

/// 
/// Serialization routine.
/// 
int lolmsgSerializedSize(LolMessage* msg);
int lolmsgSerialize(LolMessage* msg, uint8_t* outbytes);

typedef enum ParseState
{ 
	LOLPARSE_COMPLETEBIT = 1,
	LOLPARSE_HEADER = 2,
	LOLPARSE_VARIABLE = 6,
	LOLPARSE_VARIABLE_PL = 8,
	LOLPARSE_VARIABLE_COMPLETE = 7,
	LOLPARSE_ERR_NOSTART = 16,
	LOLPARSE_ERR_BUFTOOSMALL = 18,
	LOLPARSE_ERR_CHECKSUM = 20
} ParseState;

typedef struct LolParseContext
{
	uint32_t pos;
	ParseState state;
	uint16_t checksum;
	uint32_t bufLength;
	uint8_t* buf; 
} LolParseContext;

/// 
/// buf must be minimum of 8 bytes, a buf larger than 8 allows receiving of variable payloads
///
void lolmsgParseInit(LolParseContext* ctx, uint8_t* buf, uint32_t bufLength);
void lolmsgParseByte(LolParseContext* ctx, uint8_t byte);
int lolmsgParse(LolParseContext* ctx, uint8_t* bytes, uint32_t length);

void printLolMessage(const LolMessage *msg);

static inline LolMessage* lolmsgParseDone(LolParseContext* ctx)
{
	if (ctx->state & LOLPARSE_COMPLETEBIT)
		return (LolMessage*)ctx->buf;
	else
		return NULL;
}

#ifdef __cplusplus
}
#endif 

#endif
