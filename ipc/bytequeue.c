#include "bytequeue.h"

#include <string.h>
#include <stdio.h>

void BQInit(ByteQueue* bq, uint8_t* buffer, uint32_t bufferSize)
{
	bq->read = buffer;
	bq->write = buffer;
	bq->end = buffer + bufferSize;
	bq->count = 0;
	bq->size = bufferSize;
	bq->buffer = buffer;
}

bool BQPush(ByteQueue* bq, uint8_t byte)
{
	uint8_t* write = bq->write;
	*write = byte;
	if (write == bq->read && bq->count > 0)
	{	// Something was overwritten
		write++;
		if (write >= bq->end)
			write = bq->buffer;
		bq->read = write;
		bq->write = write;
		return true;
	}
	else
	{
		write++;
		if (write >= bq->end)
			write = bq->buffer;
		bq->write = write;
		bq->count++;
		return false;
	}
}

uint8_t BQPop(ByteQueue* bq)
{
	if (bq->count == 0)
		return 0;	// FIXME: Maybe another behavior could be better...
	uint8_t result = *bq->read++;
	if (bq->read >= bq->end)
		bq->read = bq->buffer;
	bq->count--;
	return result;
}

uint32_t BQPushBytes(ByteQueue* bq, const uint8_t* data, uint32_t count)
{
	register int32_t bqcount = bq->count;
	register uint8_t* bqwrite = bq->write;
	register int32_t write = bq->end - bqwrite;
	int32_t overwrite = count - bq->size + bqcount;
	if (overwrite <= 0)
	{
		if (count < write)
		{	// We can write without wraparound
			memcpy(bqwrite, data, count);
			bq->write = bqwrite + count;
			bq->count = bqcount + count;
		}
		else
		{
			memcpy(bqwrite, data, write);
			data += write;
			write = count - write;
			memcpy(bq->buffer, data, write);
			bq->count = bqcount + count;
			bq->write = bq->buffer + write;
		}
		return 0;
	}
	else
	{	// We overwrite something and the queue is full afterwards
		if (count < write)
		{	// We can write without wraparound
			memcpy(bqwrite, data, count);
			bq->write = bqwrite + count;
		}	
		else if (count < bq->size)
		{	// Wrap around and a bit of overwrite
			memcpy(bqwrite, data, write);
			data += write;
			write = count - write;		
			memcpy(bq->buffer, data, write);
			bq->write = bq->buffer + write;
		}
		else
		{	// Complete overwrite, we can aswell just take the last bytes
			memcpy(bq->buffer, data + count - bq->size, bq->size);
			bq->write = bq->buffer;
		}
		bq->count = bq->size;
		bq->read = bq->write;
		return overwrite;
	}
	/*
	uint16_t overwritten = 0;
	uint8_t* indata = data;
	for (uint16_t i = 0; i < count; i++)
		if (BQPush(bq, *indata++))
			overwritten++;
	return overwritten;*/
}

uint32_t BQPopBytes(ByteQueue* bq, uint8_t* buf, uint32_t count)
{

	register uint8_t* bqread = bq->read;
	register uint32_t bqcount = bq->count;
	uint32_t partread = bq->end - bqread;
	if (count > bqcount)
		count = bqcount;

	if (partread > count)
	{	// no wrap around
		memcpy(buf, bqread, count);
		bq->count = bqcount - count;
		bq->read = bqread + count;
	}
	else
	{	// with wrap around
		memcpy(buf, bqread, partread);
		buf += partread;
		partread = count - partread;
		memcpy(buf, bq->buffer, partread);
		bq->count = bqcount - count;
		bq->read = bq->buffer + partread;
	}
	return count;
/*
	uint16_t read = bq->count;
	uint8_t* outdata = buf;
	if (count < read)
		read = count;
	for (uint16_t i = 0; i < read; i++)
		*outdata++ = BQPop(bq);
	return read;*/
}

uint32_t BQPeekBytes(ByteQueue* bq, uint8_t* buf, uint32_t count)
{
	uint32_t read = bq->count;
	if (count < read)
		read = count;
	uint8_t* outdata = buf;
	uint8_t* temp = bq->read;
        uint32_t i;
	for (i = 0; i < read; i++)
	{
		*outdata++ = *temp++;
		if (temp >= bq->end)
			temp = bq->buffer;
	}
	return read;
}

void BQRemove(ByteQueue* bq, uint32_t count)
{
	if (count < bq->count)
	{
		bq->read += count;
		if (bq->read >= bq->end)
			bq->read -= bq->size;
		bq->count -= count;
	}
	else
	{
		bq->read = bq->write;
		bq->count = 0;
	}
}
