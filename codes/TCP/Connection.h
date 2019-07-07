// @Author Liu Han
// @Email  hancurrent@foxmail.com
#pragma once
#include "../base/noncopyable.h"
#include "Buffer.h"
#include <memory>
#include <string>
#include"../base/Logger.h"
#include"Socket.h"
#include"../Http/Http.h"
#include<unordered_map>

class EventLoop;
class Sockets;
class Channel;
class TimerHeapManager;

class Connection : noncopyable, public std::enable_shared_from_this<Connection>
{
	typedef std::shared_ptr<Connection> ConnectionPtr;
	typedef std::function<void(const ConnectionPtr&)> CloseCallback;

public:
	Connection(EventLoop* loop,
		const std::string& Con_name,
		int sockfd
	);
	~Connection();


	bool connected() const { return state_ == kConnected; }
	bool disconnected() const { return state_ == kDisconnected; }
	void send();  // this one will swap data
	void shutdown(); // NOT thread safe, no simultaneous calling
	bool isReading() const { return reading_; };
	void connectEstablished();
	EventLoop* getLoop() const { return loop_; }
	const string& name() const { return Con_name_; }
	void setCloseCallback(const CloseCallback& cb)	//由上层Server设置的回调
	{
		closeCallback_ = cb;
	}
	void connectDestroyed();						//回收该连接占用的资源

	int getFd()
	{
		return sockfd_;
	}
	int64_t getOverTimeSinceGMT();

	void setOverTimeSinceGMT(int64_t overTimeSinceGMT);

	int64_t getOverTime();

	int64_t getLastReadTimeSinceGMT();

	void setOverTime(int64_t overTime);

	bool readingOrWriting();//不能跨线程调用

	bool writing();


private:

	enum StateE { kDisconnected, kConnecting, kConnected, kDisconnecting };
	void handleRead(int64_t receiveTime);		//该连接的读回调
	void handleWrite();							//该连接的写回调
	void handleClose();							//该连接关闭时的回调
	void handleError();							//该连接出错时的回调


	void shutdownInLoop();

	

	void forceClose();		
	void forceCloseInLoop();
	void setState(StateE s) { state_ = s; }		//设置该连接的当前状态


	int sockfd_;								//该连接对应的文件描述符
	EventLoop* loop_;							//该连接所属的eventloop
	
	StateE state_;								//该连接当前的状态
	const std::string Con_name_;				//该连接的名字
	std::unique_ptr<Socket> socket_;			//已连接套接字
	bool reading_;
	CloseCallback closeCallback_;				//上层server设置的回调
	
	std::unique_ptr<Channel> channel_;			//已连接套接字所属的channel

	
	Buffer inputBuffer_;
	Buffer outputBuffer_;
	int64_t overTimeSinceGMT_;				   //该connection在该超时时间到时没有收发数据则关闭该connection
	int64_t overTime_;						   //超时时间间隔
	int64_t lastReadTimeSinceGMT_;			   //上次进行读操作的时间
	Http http;
};

