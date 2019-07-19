// @Author Liu Han
// @Email  hancurrent@foxmail.com
#include "Server.h"
#include <string>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <functional>
#include <fcntl.h> 
#include"../base/Logger.h"
extern unordered_map<EventLoop*, TimerHeapManager*>loopToManager;
int ini_listenfd(const std::string& IP, const std::string& Port)
{
	int port = stoi(Port);
	if (port < 0 || port > 65535)
	{
		LogFatal("listen port error");
		return -1;
	}
	int listen_fd = 0;
	if ((listen_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP)) == -1)
	{
		LogFatal("socket create error");
		return -1;
	}
	//设置端口复用
	int optval = 1;
	if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1)
	{
		LogFatal("socket set error");
		return -1;
	}

	// 设置服务器IP和Port，和监听文件描述符绑定
	struct sockaddr_in server_addr;
	bzero((char*)&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(IP.c_str());
	server_addr.sin_port = htons((unsigned short)port);
	if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
	{
		LogFatal("socket bind error");
		return -1;
	}

	// 开始监听，最大等待队列长为SOMAXCONN
	if (listen(listen_fd, SOMAXCONN) == -1)
	{
		LogFatal("socket listen error");
		return -1;
	}
	return listen_fd;
}

void handle_for_sigpipe()//向读端关闭的socket写数据将会引发sigpipe信号，这里的操作是忽略sigpipe
{
	struct sigaction sa;
	memset(&sa, '\0', sizeof(sa));
	sa.sa_handler = SIG_IGN;
	sa.sa_flags = 0;
	if (sigaction(SIGPIPE, &sa, NULL))
		return;
}

Server::Server(EventLoop* loop, const std::string& IP, const std::string& Port, int threadNums, const std::string& nameArg)
	:listenfd_(ini_listenfd(IP, Port)),
	nextConnId_(1),
	loop_(loop),
	IP_(IP),
	Port_(Port),
	name_(nameArg),
	threadPool_(make_shared<EventLoopThreadPool>(loop, name_)),
	ListenChannel_(make_shared<Channel>(loop, listenfd_)),
	started_(false),
	threadNums_(threadNums)
{
	handle_for_sigpipe();
}
Server::~Server() { }

void Server::start(bool starTimer)
{
	threadPool_->setThreadNum(threadNums_);
	threadPool_->start(starTimer);

	ListenChannel_->setReadCallback(std::bind(&Server::handleConn, this,_1));//监听文件描述符所属channel设置回调
	ListenChannel_->enableReading();
	started_ = true;
}
void Server::handleConn(int64_t receiveTime)
{
	struct sockaddr_in client_addr;
	memset(&client_addr, 0, sizeof(struct sockaddr_in));
	socklen_t client_addr_len = sizeof(client_addr);

	int accept_fd = 0;
	//循环的把监听队列中所有就绪的连接全部接受
	while ((accept_fd = accept(listenfd_, (struct sockaddr*)&client_addr, &client_addr_len)) > 0)
	{

		EventLoop *loop = threadPool_->getNextLoop();//每个新到的连接均匀分配给一个loop，按照轮叫的方式来选择
		//std::cout << "New connection from " << inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port)<< " fd = " << accept_fd << std::endl;

		if (accept_fd >= 10000) // 最大连接数量
		{
			close(accept_fd);
			continue;
		}
		//设为非阻塞模式
		Socket(accept_fd).setSocketNonBlocking();
		//关闭nagle
		Socket(accept_fd).setSocketNodelay();
		

		std::string con_name(std::string("socket_"));
		//con_name += to_string(TimeSinceGMT().microSecondsSinceEpochOld());
		con_name += to_string(accept_fd);
		ConnectionPtr con(make_shared<Connection>(loop, con_name, accept_fd));						//创建一个新的连接，分配到新的loop上
		connections_[con_name] = con;//保存连接

		//for (auto it : connections_)
		//{
		//	std::cout <<"server目前保存的连接"<<it.first << endl;
		//}

		con->setCloseCallback(std::bind(&Server::removeConnection, this, std::placeholders::_1));	//设置连接关闭时的回调函数
		//*下面两句顺序不能反*//
		loop->runInLoop(std::bind(&TimerHeapManager::addConnection, loopToManager[loop], con, 15 * 1000 * 1000));	//将连接添加到红黑树,单位是微妙,15s后超时
		loop->runInLoop(std::bind(&Connection::connectEstablished, con));

		
		/*for (auto it : loop->getTimerHeap())
		{
			std::cout << "heap目前保存的连接" << endl;
			cout << it.first << "|" << it.second->name() << endl;
		}*/
	}
}
void Server::removeConnection(const ConnectionPtr& conn)
{
	loop_->assertInLoopThread();
	//std::cout <<"removeConnectionInLoop"<< "conn: " << conn->name() << "  fd " << conn->getFd() << endl;
	size_t n = connections_.erase(conn->name());
	(void)n;
	assert(n == 1);
	EventLoop* ioLoop = conn->getLoop();
	// 注册的是 Connection::connectDestroyed
	ioLoop->queueInLoop(std::bind(&Connection::connectDestroyed, conn));  
}
