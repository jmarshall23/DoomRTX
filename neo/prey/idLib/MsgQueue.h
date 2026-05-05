#pragma once

#define MAX_MSG_QUEUE_SIZE				16384		// must be a power of 2

class idMsgQueue {
public:
	idMsgQueue();

	void			Init(int sequence);

	bool			Add(const byte* data, const int size);
	bool			Get(byte* data, int& size);
	int				GetTotalSize(void) const;
	int				GetSpaceLeft(void) const;
	int				GetFirst(void) const { return first; }
	int				GetLast(void) const { return last; }
	void			CopyToBuffer(byte* buf) const;

	void			WriteToMsg(idBitMsg& msg) const; //HUMANHEAD rww - write the queue to a bitmsg
	void			ReadFromMsg(const idBitMsg& msg); //HUMANHEAD rww - read the queue from a bitmsg
	bool			GetDirect(byte* data, int& size); //HUMANHEAD rww - doesn't care about sequence

private:
	byte			buffer[MAX_MSG_QUEUE_SIZE];
	int				first;			// sequence number of first message in queue
	int				last;			// sequence number of last message in queue
	int				startIndex;		// index pointing to the first byte of the first message
	int				endIndex;		// index pointing to the first byte after the last message

	void			WriteByte(byte b);
	byte			ReadByte(void);
	void			WriteShort(int s);
	int				ReadShort(void);
	void			WriteLong(int l);
	int				ReadLong(void);
	void			WriteData(const byte* data, const int size);
	void			ReadData(byte* data, const int size);
};