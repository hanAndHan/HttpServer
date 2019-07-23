// @Author Liu Han
// @Email  hancurrent@foxmail.com
#pragma once
#include <algorithm>
#include <vector>
#include <assert.h>
#include <string.h>
//        kCheapPrepend                                  readerIndex_            writerIndex_
//   |         |	prependableBytes()-kCheapPrepend 	|   readableBytes()   |   writableBytes()  |

class Buffer
{
public:

	static const size_t kCheapPrepend = 8;
	static const size_t kInitialSize = 1024;
	//头部预留了8字节以备不时之需
	explicit Buffer(size_t iniSize = kInitialSize)
		:buffer_(kCheapPrepend + iniSize),
		readerIndex_(kCheapPrepend),
		writerIndex_(kCheapPrepend)
	{
	}

	size_t readableBytes() const   //可读空间大小
	{
		return writerIndex_ - readerIndex_;
	}
	size_t writableBytes() const   //可写空间大小
	{
		return buffer_.size() - writerIndex_;
	}
	size_t prependableBytes() const  //readerIndex前面空间的大小
	{
		return readerIndex_;
	}

	void prepend(const void* data, size_t len)   //在/readerIndex前面添加
	{
		assert(len <= prependableBytes());
		readerIndex_ -= len;
		const char* d = static_cast<const char*>(data);
		std::copy(d, d + len, begin() + readerIndex_);
	}

	const char* findCRLF() const     //查找换行符\r\n
	{
		const char* crlf = std::search(peek(), beginWrite(), RN, RN + 2);
		return crlf == beginWrite() ? NULL : crlf;
	}
	const char* findCRLF(const char* start) const  //从指定位置开始查找换行符\r\n

	{
		assert(peek() <= start);
		assert(start <= beginWrite());
		const char* crlf = std::search(start, beginWrite(), RN, RN + 2);
		return crlf == beginWrite() ? NULL : crlf;
	}

	void append(const char* data, size_t len)   //追加数据到缓冲
	{
		ensureWritableBytes(len);
		std::copy(data, data + len, beginWrite());
		hasWritten(len);
	}

	void append(const void* data, size_t len)
	{
		append(static_cast<const char*>(data), len);
	}

	void ensureWritableBytes(size_t len)  //确认缓冲区大小是否大于等于len，长度不够则扩容
	{
		if (writableBytes() < len)
		{
			makeSpace(len);
		}
		assert(writableBytes() >= len);
	}
	
	const char* peek() const         // 可读的位置的指针
	{
		return begin() + readerIndex_;
	}

	char* beginWrite()                //可写的位置的指针
	{
		return begin() + writerIndex_;
	}

	const char* beginWrite() const    //可写的位置的指针
	{
		return begin() + writerIndex_;
	}

	void hasWritten(size_t len)      //可写部分减少
	{
		assert(len <= writableBytes());
		writerIndex_ += len;
	}

	void unwrite(size_t len)        //可写部分增加
	{
		assert(len <= readableBytes());
		writerIndex_ -= len;
	}
	void retrieve(size_t len)	   //释放已读部分
	{
		assert(len <= readableBytes());   
		if (len < readableBytes())
		{
			readerIndex_ += len;
		}
		else
		{
			retrieveAll();
		}
	}
	void retrieveAll()   //复原
	{
		readerIndex_ = kCheapPrepend;
		writerIndex_ = kCheapPrepend;
		buffer_.resize(kInitialSize + kCheapPrepend);
	}
	void retrieveAllFake()   //虚假复原
	{
		readerIndex_ = kCheapPrepend;
		writerIndex_ = kCheapPrepend;
	}
	void retrieveUntil(const char* end) //释放直到end的可读数据
	{
		assert(peek() <= end);
		assert(end <= beginWrite());
		retrieve(end - peek());
	}

	// 直接读入buffer
	ssize_t readFd(int fd, int* savedErrno);


private:

	char* begin()
	{
		return &*buffer_.begin();
	}
	const char* begin() const
	{
		return &*buffer_.begin();
	}

	void makeSpace(size_t len)
	{ 
	//先看可写的部分够不够，不够才扩容
		//         kCheapPrepend                       readerIndex_            writerIndex_
		//  |         |	  prependableBytes()-kCheapPrepend 	|   readableBytes()   |   writableBytes()  |
		if (writableBytes() + prependableBytes() - kCheapPrepend < len )  //保留了8个字节的头部
		{
			buffer_.resize(writerIndex_ + len);
		}
		else
		{
			assert(kCheapPrepend < readerIndex_);
			size_t readable = readableBytes();
			std::copy(begin() + readerIndex_,begin() + writerIndex_,begin() + kCheapPrepend);
			readerIndex_ = kCheapPrepend;
			writerIndex_ = readerIndex_ + readable;
			assert(readable == readableBytes());
		}
	}

	std::vector<char> buffer_;
	size_t readerIndex_;
	size_t writerIndex_;
	static const char RN[];
};

