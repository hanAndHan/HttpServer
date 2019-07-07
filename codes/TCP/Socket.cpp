// @Author Liu Han
// @Email  hancurrent@foxmail.com
#include "Socket.h"
#include <fcntl.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/uio.h>
#include <signal.h>
#include <string.h>
#include <netinet/tcp.h>
#include <iostream>
ssize_t Socket::read(void *buf, size_t count)
{
	return ::read(sockfd_, buf, count);
}
ssize_t Socket::readv(const struct iovec *iov, int iovcnt)
{
	return ::readv(sockfd_, iov, iovcnt);
}
ssize_t Socket::write(const void *buf, size_t count)
{
	return ::write(sockfd_, buf, count);
}

void Socket::close()
{
	if (::close(sockfd_) < 0)
	{
		LogFatal("socket close error");
	}
}
void Socket::shutdownWrite()
{
	if (::shutdown(sockfd_, SHUT_WR) < 0)
	{
		LogFatal("sockets::shutdownWrite");
	}
}

int Socket::setSocketNonBlocking()
{
	int flag = fcntl(sockfd_, F_GETFL, 0);
	if (flag == -1)
		return -1;

	flag |= O_NONBLOCK;
	if (fcntl(sockfd_, F_SETFL, flag) == -1)
		return -1;
	return 0;
}
void Socket::setSocketNodelay(bool on)
{
	/*int enable = 1;
	setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, (void*)&enable, sizeof(enable));*/
	int optval = on ? 1 : 0;
	::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY,
		&optval, sizeof optval);
}
void Socket::setSocketNoLinger()
{
	struct linger linger_;
	linger_.l_onoff = 1;
	linger_.l_linger = 30;
	setsockopt(sockfd_, SOL_SOCKET, SO_LINGER, (const char *)&linger_, sizeof(linger_));
}

void Socket::setReuseAddr(bool on)
{
	int optval = on ? 1 : 0;
	::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR,
		&optval, sizeof optval);
}

void Socket::setKeepAlive(bool on)
{
	int optval = on ? 1 : 0;
	::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE,
		&optval, sizeof optval);
}