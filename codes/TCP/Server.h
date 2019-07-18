// @Author Liu Han
// @Email  hancurrent@foxmail.com
#pragma once
#include "../base/noncopyable.h"
#include "../base/Condition.h"
#include "../net/EventLoop.h"
#include "../net/Channel.h"
#include "../net/EventLoopThreadPool.h"
#include"../timerHeap/TimerHeapManager.h"
#include "Socket.h"
#include "Connection.h"
#include <string>
#include <memory>
#include <unordered_map>

using namespace std::placeholders;

class Server :noncopyable
{
public:
	Server(EventLoop* loop,
		const std::string& IP,//服务器的IP
		const std::string& Port,//服务器的端口
		int threadNums_,
		const std::string& nameArg="EventLoopThreadPool"//threadPool_的名字
	);
	~Server();

	void start(bool starTimer = false);
private:
	typedef std::shared_ptr<Connection> ConnectionPtr;
	typedef std::unordered_map<std::string, ConnectionPtr> ConnectionMap;
	void handleConn(int64_t receiveTime);								 //监听套接字就绪对应的回调
	void removeConnection(const ConnectionPtr& conn);
	void removeConnectionInLoop(const ConnectionPtr& conn);


	int listenfd_;									  //监听套接字
	int nextConnId_;

	EventLoop* loop_;								  //主reactor
	const std::string IP_;							  //服务器的IP
	const std::string Port_;						  //服务的端口
	std::string name_;								  //threadPool_的名字
	std::shared_ptr<EventLoopThreadPool> threadPool_; //从reactor
	std::shared_ptr<Channel> ListenChannel_;		  //监听套接字使用的channel
	bool started_;									  //服务器是否启动
	int threadNums_;								  //EventLoopThread中loop的个数
	ConnectionMap connections_;						  //服务器管理的连接
};
