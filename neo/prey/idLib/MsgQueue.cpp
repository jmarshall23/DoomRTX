#include "precompiled.h"
#pragma hdrstop

#include "preylib.h"

/*
===============
idMsgQueue::idMsgQueue
===============
*/
idMsgQueue::idMsgQueue(void) {
	Init(0);
}

/*
===============
idMsgQueue::Init
===============
*/
void idMsgQueue::Init(int sequence) {
	first = last = sequence;
	startIndex = endIndex = 0;
}

/*
===============
idMsgQueue::Add
===============
*/
bool idMsgQueue::Add(const byte* data, const int size) {
	if (GetSpaceLeft() < size + 8) {
		return false;
	}
	int sequence = last;
	WriteShort(size);
	WriteLong(sequence);
	WriteData(data, size);
	last++;
	return true;
}

/*
===============
idMsgQueue::Get
===============
*/
bool idMsgQueue::Get(byte* data, int& size) {
	if (first == last) {
		size = 0;
		return false;
	}
	int sequence;
	size = ReadShort();
	sequence = ReadLong();
	ReadData(data, size);
	assert(sequence == first);
	first++;
	return true;
}

/*
===============
idMsgQueue::GetTotalSize
===============
*/
int idMsgQueue::GetTotalSize(void) const {
	if (startIndex <= endIndex) {
		return (endIndex - startIndex);
	}
	else {
		return (sizeof(buffer) - startIndex + endIndex);
	}
}

/*
===============
idMsgQueue::GetSpaceLeft
===============
*/
int idMsgQueue::GetSpaceLeft(void) const {
	if (startIndex <= endIndex) {
		return sizeof(buffer) - (endIndex - startIndex) - 1;
	}
	else {
		return (startIndex - endIndex) - 1;
	}
}

/*
===============
idMsgQueue::CopyToBuffer
===============
*/
void idMsgQueue::CopyToBuffer(byte* buf) const {
	if (startIndex <= endIndex) {
		memcpy(buf, buffer + startIndex, endIndex - startIndex);
	}
	else {
		memcpy(buf, buffer + startIndex, sizeof(buffer) - startIndex);
		memcpy(buf + sizeof(buffer) - startIndex, buffer, endIndex);
	}
}

/*
===============
idMsgQueue::WriteToMsg
HUMANHEAD rww - write the queue to a bitmsg
===============
*/
void idMsgQueue::WriteToMsg(idBitMsg& msg) const {
	msg.WriteShort(GetTotalSize());
	assert(startIndex <= endIndex); //only support writing from an unread queue
	msg.WriteData(buffer + startIndex, endIndex - startIndex);
}

/*
===============
idMsgQueue::WriteToMsg
HUMANHEAD rww - read the queue from a bitmsg
===============
*/
void idMsgQueue::ReadFromMsg(const idBitMsg& msg) {
	Init(0); //ensure a flushed buffer
	endIndex = msg.ReadShort();
	msg.ReadData(buffer, endIndex);
}

/*
===============
idMsgQueue::GetDirect
HUMANHEAD rww - doesn't care about sequence
===============
*/
bool idMsgQueue::GetDirect(byte* data, int& size) {
	if (startIndex == endIndex) {
		size = 0;
		return false;
	}
	size = ReadShort();
	ReadLong(); //read over sequence
	ReadData(data, size);
	first++;
	return true;
}

/*
===============
idMsgQueue::WriteByte
===============
*/
void idMsgQueue::WriteByte(byte b) {
	buffer[endIndex] = b;
	endIndex = (endIndex + 1) & (MAX_MSG_QUEUE_SIZE - 1);
}

/*
===============
idMsgQueue::ReadByte
===============
*/
byte idMsgQueue::ReadByte(void) {
	byte b = buffer[startIndex];
	startIndex = (startIndex + 1) & (MAX_MSG_QUEUE_SIZE - 1);
	return b;
}

/*
===============
idMsgQueue::WriteShort
===============
*/
void idMsgQueue::WriteShort(int s) {
	WriteByte((s >> 0) & 255);
	WriteByte((s >> 8) & 255);
}

/*
===============
idMsgQueue::ReadShort
===============
*/
int idMsgQueue::ReadShort(void) {
	return ReadByte() | (ReadByte() << 8);
}

/*
===============
idMsgQueue::WriteLong
===============
*/
void idMsgQueue::WriteLong(int l) {
	WriteByte((l >> 0) & 255);
	WriteByte((l >> 8) & 255);
	WriteByte((l >> 16) & 255);
	WriteByte((l >> 24) & 255);
}

/*
===============
idMsgQueue::ReadLong
===============
*/
int idMsgQueue::ReadLong(void) {
	return ReadByte() | (ReadByte() << 8) | (ReadByte() << 16) | (ReadByte() << 24);
}

/*
===============
idMsgQueue::WriteData
===============
*/
void idMsgQueue::WriteData(const byte* data, const int size) {
	for (int i = 0; i < size; i++) {
		WriteByte(data[i]);
	}
}

/*
===============
idMsgQueue::ReadData
===============
*/
void idMsgQueue::ReadData(byte* data, const int size) {
	if (data) {
		for (int i = 0; i < size; i++) {
			data[i] = ReadByte();
		}
	}
	else {
		for (int i = 0; i < size; i++) {
			ReadByte();
		}
	}
}