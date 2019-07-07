// @Author Liu Han
// @Email  hancurrent@foxmail.com
// 缓冲区类，配合Logger类使用
#pragma once
#include<vector>
#include<string.h>
using namespace std;
class BufferLog
{
	

public:
	/* 构造函数，初始化buffer,并且设置可读可写的位置 */
	explicit BufferLog() :
		readable(0), writable(0), initializeSize(4000*1000)
	{
		/* 提前开辟好大小 */
		buffer.resize(initializeSize);
	}

	/* 返回缓冲区的容量 */
	size_t Capacity()
	{
		return buffer.capacity();
	}

	/* 返回缓冲区的大小 */
	size_t Size()
	{
		return writable;
	}

	/* set Size */
	void setSize(void)
	{
		readable = 0;
		writable = 0;
	}

	/* 向buffer中添加数据 */
	void append(const char* mesg, size_t len)
	{
		strncpy(WritePoint(), mesg, len);//复制长度为len的字符串
		writable += len;
	}

	/* 返回buffer可用大小 */
	size_t avail()
	{
		return Capacity() - writable;
	}

	/*返回buffer的首地址*/
	char *  Data()
	{
		return &buffer[0];
	}
private:
	/* 返回可读位置的指针 */
	char* ReadPoint()
	{
		return &buffer[readable];
	}

	/* 返回可写位置的指针 */
	char* WritePoint()
	{
		return &buffer[writable];
	}

	/* 返回可读位置 */
	size_t ReadAddr()
	{
		return readable;
	}

	/* 返回可写位置 */
	size_t WriteAddr()
	{
		return writable;
	}


private:	
	size_t readable;
	size_t writable;
	/* 初始化默认大小 */
	const size_t initializeSize;
	vector<char> buffer;
};