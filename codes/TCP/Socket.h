// @Author Liu Han
// @Email  hancurrent@foxmail.com
#pragma once
#include <arpa/inet.h>
#include"../base/noncopyable.h"
#include"../base/Logger.h"
class Socket
{
public:
	explicit Socket(int sockfd)
		: sockfd_(sockfd)
	{ }

	ssize_t read(void *buf, size_t count);
	ssize_t readv(const struct iovec *iov, int iovcnt);
	ssize_t write(const void *buf, size_t count);
	void    close();
	void    shutdownWrite();
	int     setSocketNonBlocking();
	void    setSocketNodelay(bool on = true);
	void    setSocketNoLinger();
	void    setReuseAddr(bool on = true);
	void	setKeepAlive(bool on = true);
private:
	const int sockfd_;
};