// @Author Liu Han
// @Email  hancurrent@foxmail.com
#include "Connection.h"
#include "../net/EventLoop.h"
#include "Socket.h"
#include "../net/Channel.h"
#include <memory>
#include <iostream>
#include"../timerHeap/TimerHeapManager.h"
extern const int timeslot;
extern unordered_map<EventLoop*, TimerHeapManager*>loopToManager;
Connection::Connection(EventLoop* loop,const std::string& Con_name,int sockfd)
	:sockfd_(sockfd),
	loop_(loop),
	state_(kConnecting),
	Con_name_(Con_name),
	socket_(new Socket(sockfd)),
	reading_(false),
	channel_(new Channel(loop, sockfd)),
	overTimeSinceGMT_(0),
	overTime_(0),
	lastReadTimeSinceGMT_(0)
{
	channel_->setReadCallback(std::bind(&Connection::handleRead, this,_1));
	channel_->setWriteCallback(std::bind(&Connection::handleWrite, this));
	channel_->setCloseCallback(std::bind(&Connection::handleClose, this));
	channel_->setErrorCallback(std::bind(&Connection::handleError, this));
	socket_->setReuseAddr(true);
	socket_->setSocketNodelay(true);
}

Connection::~Connection()
{
	//std::cout << "  ~Connection  fd " << channel_->fd() << endl;
	//std::cout <<"heap size:" << loop_->getTimerHeap().size() << endl;
	assert(state_ == kDisconnected);
}

void Connection::connectEstablished()
{
	loop_->assertInLoopThread();
	assert(state_ == kConnecting);
	setState(kConnected);
	channel_->tie(shared_from_this());			//将当前的tcpconnection对象安全转换成this指针供shared_ptr使用
	channel_->enableReading();					//注册channel的读事件到epoll，读事件就绪时，epoll会调用handleRead
}


void Connection::send()
{
	if (state_ == kConnected)
	{
		loop_->assertInLoopThread();
		ssize_t  nwrote = 0;
		ssize_t remaining = outputBuffer_.readableBytes();
		if (!channel_->isWriting())
		{
			nwrote = socket_->write(outputBuffer_.peek(), outputBuffer_.readableBytes());
			if (nwrote >= 0)
			{
				remaining -= nwrote;
				outputBuffer_.retrieve(nwrote);
			}
			else
			{
				if (errno != EWOULDBLOCK)
				{
					if (errno == EPIPE || errno == ECONNRESET) 
					{
						LogFatal("TcpConnection::send Error");
					}
				}
			}
			if (remaining > 0 && !channel_->isWriting())
			{
				channel_->enableWriting();
			}
			else if (remaining == 0)
			{
				//if (http.isDir()||http.isShow404())//发送的是目录的话，直接把连接关了
				//{
				//	loopToManager[loop_]->removeConnection(overTimeSinceGMT_, shared_from_this());
				//	shutdown();
				//}

				//else//发送的是文件的话，延长关闭
				//{
				//	//写完了不主动关闭连接，延长超时时间
				//	cout << endl << "延长" << name() << endl;
				//	loopToManager[loop_]->modifyConnection(overTimeSinceGMT_, shared_from_this());
				//}
			}
		}
	}
}
//shutdown与handleClose的区别：
//shutdown用来实现延迟关闭，handleClose用于回调，或者用来实现强制关闭
void Connection::shutdown()
{
	if (state_ == kConnected)
	{
		setState(kDisconnecting);
	
		loop_->runInLoop(std::bind(&Connection::shutdownInLoop, this));
	}
}

void Connection::shutdownInLoop()  
{
	loop_->assertInLoopThread();
	if (!channel_->isWriting())// 没有监听写事件才关闭
	{
		setState(kDisconnected);
		channel_->disableAll();
		ConnectionPtr guardThis(shared_from_this());
		closeCallback_(guardThis); //调用Server::removeConnection
		// std::cout << "-----shutdownInloop------" << endl;
	}
}

void Connection::handleRead(int64_t receiveTime)
{
	loop_->assertInLoopThread();
	int savedErrno = 0;
	ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);

	if (n > 0)
	{
		reading_ = true;
		//每执行一次读操作就延长关闭
		loopToManager[loop_]->modifyConnection(overTimeSinceGMT_, shared_from_this());
		//lastReadTimeSinceGMT_ = TimeSinceGMT().secondsSinceEpochOld();//上次读操作的时间
		//*************处理 buffer***************// 
		http.handle(inputBuffer_, outputBuffer_);
		send();
		reading_ = false;
	}
	else if (n == 0)
	{
		//cout << "客户端关闭" << endl;
		loopToManager[loop_]->removeConnection(overTimeSinceGMT_, shared_from_this());
		handleClose();//无论对方是半关闭还是全关闭，都认为对方是全关闭，本端强制关闭连接，
					  //数据还没写完也关闭/若不想强制关闭可调用shutdown，它会等到本端把数据写完才关闭，但是这会造成busy loop,
					  //因为采用的是epoll lt模式，本端一直没有关闭文件描述符，那么文件描述符将一直就绪，因为这个关闭事件我们一直没有处理
	}
	else
	{
		errno = savedErrno;
		char msg[32];
		sprintf(msg, "TcpConnection::handleRead %d", savedErrno);
		LogError(msg);
		loopToManager[loop_]->removeConnection(overTimeSinceGMT_, shared_from_this());
		handleError();
	}
}
void Connection::handleWrite()
{
	loop_->assertInLoopThread();
	if (channel_->isWriting())
	{
		//cout << "=======================写操作就绪====================" << endl;
		ssize_t n = socket_->write(outputBuffer_.peek(), outputBuffer_.readableBytes());
		if (n > 0)
		{
			outputBuffer_.retrieve(n);
			if (outputBuffer_.readableBytes() == 0)
			{
				channel_->disableWriting();	   // 停止关注POLLOUT事件，以免出现busy loop
				if (state_ == kDisconnecting)	// 发送缓冲区已清空并且连接状态是kDisconnecting, 要关闭连接
				{
					shutdownInLoop();		// 关闭连接
				}
				//else//写完后延长超时时间
				//{
				//	loopToManager[loop_]->modifyConnection(overTimeSinceGMT_, shared_from_this());
				//}
			}
		}
	}

}
//改一下，打印下日志错误**********************************
void Connection::handleError()
{
	/* 出错后强制关闭 */
	forceClose();
}

void Connection::forceClose()
{
	if (state_ == kConnected || state_ == kDisconnecting)
	{
		setState(kDisconnecting);
		loop_->queueInLoop(std::bind(&Connection::forceCloseInLoop, shared_from_this()));
	}
}
void Connection::forceCloseInLoop()
{
	loop_->assertInLoopThread();
	if (state_ == kConnected || state_ == kDisconnecting)
	{
			handleClose();
	}
}

//这个函数执行完Connection就被析构了
void Connection::connectDestroyed()   // 在server中使用
{
	loop_->assertInLoopThread();
	channel_->remove(); // 本连接对应的描述符不再需要监控，从epoll中删除。
	socket_->close();
}

//handleClose=>Server::removeConnection=>Connection::connectDestroyed()放进loop中调用
void Connection::handleClose()
{
	//std::cout << "----handleclose----" << endl;
	loop_->assertInLoopThread();
	assert(state_ == kConnected || state_ == kDisconnecting);   

	setState(kDisconnected);
	channel_->disableAll();
	ConnectionPtr guardThis(shared_from_this());
	closeCallback_(guardThis); //调用Server::removeConnection
}

bool Connection::readingOrWriting()
{
	return channel_->isWriting() || isReading();
}

bool Connection::writing()
{
	return channel_->isWriting();
}

int64_t Connection::getOverTimeSinceGMT()
{
	return overTimeSinceGMT_;
}

void Connection::setOverTimeSinceGMT(int64_t overTimeSinceGMT)
{
	overTimeSinceGMT_ = overTimeSinceGMT;
}

int64_t Connection::getOverTime()
{
	return overTime_;
}

int64_t Connection::getLastReadTimeSinceGMT()
{
	return lastReadTimeSinceGMT_;
}

void Connection::setOverTime(int64_t overTime)
{
	overTime_ = overTime;
}
