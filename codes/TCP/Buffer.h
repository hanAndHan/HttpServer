// @Author Liu Han
// @Email  hancurrent@foxmail.com
#pragma once
#include <algorithm>
#include <vector>
#include <assert.h>
#include <string.h>

class Buffer
{
public:

	static const size_t kCheapPrepend = 8;
	static const size_t kInitialSize = 1024;

	explicit Buffer(size_t iniSize = kInitialSize)
		:buffer_(kCheapPrepend + iniSize),
		readerIndex_(kCheapPrepend),
		writerIndex_(kCheapPrepend)
	{
	}

	void swap(Buffer& tmp)
	{
		buffer_.swap(tmp.buffer_);
		std::swap(readerIndex_, tmp.readerIndex_);
		std::swap(writerIndex_, tmp.writerIndex_);
	}

	size_t readableBytes() const   //可读的数据量
	{
		return writerIndex_ - readerIndex_;
	}
	size_t writableBytes() const   //可写的空间
	{
		return buffer_.size() - writerIndex_;
	}
	size_t prependableBytes() const  //前面空闲空间
	{
		return readerIndex_;
	}

	void prepend(const void* data, size_t len)   //在头部添加
	{
		assert(len <= prependableBytes());
		readerIndex_ -= len;
		const char* d = static_cast<const char*>(data);
		std::copy(d, d + len, begin() + readerIndex_);
	}
	const char* peek() const         // 可读的位置下标
	{
		return begin() + readerIndex_;
	}

	const char* findCRLF() const     //查找换行符\r\n
	{
		const char* crlf = std::search(peek(), beginWrite(), RN, RN + 2);
		return crlf == beginWrite() ? NULL : crlf;
	}
	const char* findCRLF(const char* start) const
	{
		assert(peek() <= start);
		assert(start <= beginWrite());
		// FIXME: replace with memmem()?
		const char* crlf = std::search(start, beginWrite(), RN, RN + 2);
		return crlf == beginWrite() ? NULL : crlf;
	}

	const char* findEOL() const          //查找文件结尾符
	{
		const void* eol = memchr(peek(), '\n', readableBytes());
		return static_cast<const char*>(eol);
	}
	const char* findEOL(const char* start) const
	{
		assert(peek() <= start);
		assert(start <= beginWrite());
		const void* eol = memchr(start, '\n', beginWrite() - start);
		return static_cast<const char*>(eol);
	}

	void append(const char* data, size_t len)
	{
		ensureWritableBytes(len);
		std::copy(data, data + len, beginWrite());
		hasWritten(len);
	}
	void ensureWritableBytes(size_t len)
	{
		if (writableBytes() < len)
		{
			makeSpace(len);
		}
		assert(writableBytes() >= len);
	}
	void append(const void* data, size_t len)
	{
		append(static_cast<const char*>(data), len);
	}

	char* beginWrite()                //开始可写的位置
	{
		return begin() + writerIndex_;
	}

	const char* beginWrite() const    //开始可写的位置
	{
		return begin() + writerIndex_;
	}

	void hasWritten(size_t len)
	{
		assert(len <= writableBytes());
		writerIndex_ += len;
	}

	void unwrite(size_t len)       // 回写len长度字段
	{
		assert(len <= readableBytes());
		writerIndex_ -= len;
	}
	void retrieve(size_t len) // 释放可读部分
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
	void retrieveUntil(const char* end) //释放直到end的数据
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
		if (writableBytes() + prependableBytes() < len + kCheapPrepend)  //保留了8个字节的头部
		{
			buffer_.resize(writerIndex_ + len);
		}
		else
		{
			assert(kCheapPrepend < readerIndex_);
			size_t readable = readableBytes();
			std::copy(begin() + readerIndex_,
				begin() + writerIndex_,
				begin() + kCheapPrepend);
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

