// @Author Liu Han
// @Email  hancurrent@foxmail.com
#include"EPoll.h"
#include"Channel.h"
#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <sys/epoll.h>
#include"EventLoop.h"
namespace
{
const int kNew = -1;//channel还未添加
const int kAdded = 1;//channel现在是已经被添加的状态
const int kDeleted = 2;//channel现在是已经被删除的状态
}

EPoll::EPoll(EventLoop* loop): 
	ownerLoop_(loop),
	epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
    events_(kInitEventListSize)//初始化就绪事件数组的大小
{
  if (epollfd_ < 0)
	  LogFatal("EPollPoller::EPollPoller");
}

EPoll::~EPoll()
{
  ::close(epollfd_);
}

int64_t EPoll::poll(int timeoutMs, ChannelList* activeChannels)
{
  int numEvents = ::epoll_wait(epollfd_,
                               &*events_.begin(),//就绪事件
                               static_cast<int>(events_.size()),//最多监听文件描述符的个数
                               timeoutMs);
  if (numEvents > 0)
  {
    //cout << numEvents << " events happended";
    fillActiveChannels(numEvents, activeChannels);
    if (static_cast<size_t>(numEvents) == events_.size())//动态扩容
    {
      events_.resize(events_.size()*2);
    }
  }
  else if (numEvents == 0)
  {
    //cout << " nothing happended";
  }
  else
  {
	  LogFatal("EPollPoller::poll()");
  }
  return static_cast<int64_t>(TimeSinceGMT().microSecondsSinceEpoch());
}

void EPoll::fillActiveChannels(int numEvents,ChannelList* activeChannels) const
{
  for (int i = 0; i < numEvents; ++i)
  {
    Channel* channel = static_cast<Channel*>(events_[i].data.ptr);//获取就绪的channel
    channel->set_revents(events_[i].events);
    activeChannels->push_back(channel);
  }
}

void EPoll::updateChannel(Channel* channel)
{
  ownerLoop_->assertInLoopThread();
  const int index = channel->index();
  if (index == kNew || index == kDeleted)//channel是还未添加的/或者已经被删除的
  {
    channel->set_index(kAdded);    //将channel的状态重新设置为添加
    update(EPOLL_CTL_ADD, channel);//将channel添加到epoll上
  }
  else//channel是已经添加的，想对其删除或者修改
  {
    int fd = channel->fd();
    (void)fd;
    assert(index == kAdded);
    if (channel->isNoneEvent())//从epoll上删除channel
    {
      update(EPOLL_CTL_DEL, channel);
      channel->set_index(kDeleted);
    }
    else//在epoll上修改channel
    {
      update(EPOLL_CTL_MOD, channel);
    }
  }
}

void EPoll::removeChannel(Channel* channel)
{
  ownerLoop_->assertInLoopThread();
  assert(channel->isNoneEvent());
  int index = channel->index();
  assert(index == kAdded || index == kDeleted);
   if (index == kAdded)
  {
    update(EPOLL_CTL_DEL, channel);
  }
  //调用了updateChannel和removeChannel,后状态才会变为kNew，直接调用removeChannel会断言失败
  channel->set_index(kNew);//将该channel设置成未添加
}

void EPoll::update(int operation, Channel* channel)
{
  struct epoll_event event;
  bzero(&event, sizeof event);
  event.events = channel->events();//文件描述符关注的事件
  event.data.ptr = channel;//文件描述符对应的用户数据，这里的用户数据就保存的channel
  int fd = channel->fd();
  if (::epoll_ctl(epollfd_, operation, fd, &event) < 0)
  {
	  char errMessage[32]; 
	  sprintf(errMessage,"epoll_ctl op=%d,fd=%d", operation, fd);
	  LogFatal(errMessage);
  }
}

