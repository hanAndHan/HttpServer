// @Author Liu Han
// @Email  hancurrent@foxmail.com
#pragma once
#include<functional>
#include<iostream>
#include <memory>

#include"../base/noncopyable.h"
#include"../base/TimeSinceGMT.h"
#include"../base/Logger.h"
using namespace std;

class EventLoop;

class Channel : noncopyable
{
 public:
  typedef function<void()> EventCallback;//事件的回调处理
  typedef function<void(int64_t)> ReadEventCallback;//读事件的回调处理

  Channel(EventLoop* loop, int fd);//构造函数，一个EventLoop可以包含多个channel，但是一个channel只能属于一个EventLoop
  ~Channel();

  void handleEvent(int64_t receiveTime);

  //一些回调函数的注册
  void setReadCallback(const ReadEventCallback& cb)//读的回调函数
  { readCallback_ = cb; }
  void setWriteCallback(const EventCallback& cb)//写的回调函数
  { writeCallback_ = cb; }
  void setCloseCallback(const EventCallback& cb)//关闭的回调函数
  { closeCallback_ = cb; }
  void setErrorCallback(const EventCallback& cb)//错误的回调函数
  { errorCallback_ = cb; }


  int fd() const { return fd_; }//channel对应的文件描述符
  int events() const { return events_; }//该channel的fd上注册了哪一些事件，保存在events_里面
  void set_revents(int revt) { revents_ = revt; } //设置就绪事件
   bool isNoneEvent() const { return events_ == kNoneEvent; }//没有事件

  //************    ！！！！！！！！重要！！！！！！！  *************//
  void enableReading() { events_ |= kReadEvent; update(); }//关注可读事件，调用update把该channel注册到eventloop，然后再注册到eventloop所持有的epoll对象中
  void disableReading() { events_ &= ~kReadEvent; update(); }
  void enableWriting() { events_ |= kWriteEvent; update(); }
  void disableWriting() { events_ &= ~kWriteEvent; update(); }
  void disableAll() { events_ = kNoneEvent; update(); }//不关注事件了
  bool isWriting() const { return events_ & kWriteEvent; }
  bool isReading() const { return events_ & kReadEvent; }
  // 下面两个函数由epoll调用
  int index() { return index_; }//channel当前在epoll上处于已添加1，还是已删除2，还是未添加-1
  void set_index(int idx) { index_ = idx; }//设置该channel在epoll上的状态



  void doNotLogHup() { logHup_ = false; }

  EventLoop* ownerLoop() { return loop_; }
  void remove();

 private:
  void update();
  

  static const int kNoneEvent;
  static const int kReadEvent;
  static const int kWriteEvent;

  EventLoop* loop_;			// 所属EventLoop
  const int  fd_;			// 文件描述符，但不负责关闭该文件描述符
  int        events_;		// 关注的事件
  int        revents_;		// epoll返回的事件，即就绪的事件
  int        index_;		// channel当前在epoll上处于已添加1，还是已删除2，还是未添加-1
  bool       logHup_;		// for POLLHUP  高性能服务器编程151页上方
  bool eventHandling_;		// 是否处于处理事件中
  ReadEventCallback readCallback_;
  EventCallback writeCallback_;
  EventCallback closeCallback_;
  EventCallback errorCallback_;
};
