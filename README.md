# HttpServer
| Part Ⅰ | Part Ⅱ | Part Ⅲ | Part Ⅳ | Part Ⅴ | Part Ⅵ |
| :--------: | :---------: | :---------: | :---------: | :---------: | :---------: |
| [Inroduce](#介绍) | [Timer](#Timer) | [Log](#日志类) | [Buffer](#Buffer) |[Close](#连接关闭) |[Test](#压力测试)

<a name="介绍"></a>
## 1. 介绍
本项目为c++11实现的Http下载服务器，支持在浏览器页面播放音频和视频，以及文件下载。开发环境为Visual Studio 2017/unbuntu 16.04 VMware Workstation 14 Player，使用vs2017远程调试在linux环境下的程序。
### 1.1 技术要点
* 基于one loop per thread的思想实现主框架，同时使用一种one timer per loop的方式踢掉空闲连接
* event loop使用epoll LT模式加非阻塞I/O实现
* timer使用STL的multi_map实现，支持心搏时间动态变化，超时时间动态变化 
* 使用timerfd将定时事件融入epoll系统调用，即统一事件源
* 实现高效的多缓冲异步日志系统
* 实现应用层buffer，支持延迟关闭连接
* 实现线程池充分利用多核CPU，并避免线程频繁创建销毁的开销
### 1.2 总体框架
![](https://github.com/hanAndHan/HttpServer/blob/master/imge/framework.png)
如图所示，前端工作线程负责处理业务，后端日志线程负责将日志信息写入文件。程序启动时，先创建好SubLoop，MainLoop，日志线程。MainLoop只负责监听是否有连接到来。当连接到来后MainLoop以一种轮叫的方式(round robin)一次性将所有处于ESTABLISHED状态的连接分发给SubLoop，同时每个SubLoop中运行着一个Timer，负责踢掉空闲连接。当前端日志buffer被写满时会与后端buffer空闲交换，由后端日志线程负责将buffer中的内容写入日志文件。
<a name="Timer"></a>
## 2. Timer实现要点
在服务器和客户端没有协商好心跳协议的情况下，使用timer踢掉空闲连接只是一种权益之计。踢掉空闲连接的方式有很多种，比如升序链表，小根堆，时间轮等，我尝试了两种方式，第一种是小根堆，第二种是红黑树。
小根堆直接使用的STL的priority_queue，红黑树使用的是multi_map。主体思想是one timer per loop，即每个loop都用一个timer管理连接。
* 先来讲讲使用小根堆实现时遇到的bug：
我的每个连接用shared_ptr进行管理，当shared_ptr引用计数减为0后，连接会被关闭，为了不影响引用计数，小根堆这边使用的是weak_ptr。假设有这么一种情况：客户端主动关闭了连接，服务器read到0，服务器也应该关闭连接，并把连接从小根堆上摘下来。但是priority_queue不支持随机访问，因此连接不能被及时的被摘下来(等到之后访问到它时再摘下来)，但是由于使用的是weak_ptr，因此连接的及时关闭不会受到影响。这里看起来似乎还没有什么问题。但是在进行插入操作的时候会出现问题，因为priority_queue是自动排序的，在进行插入时，某些weak_ptr指向的资源已经被释放，这将导致在排序时出现coredump(这个隐藏bug困扰了我半天，最后也是gdb core文件定位到出现段错误的地方分析出来的)。
* 使用红黑树的实现如下：
  * 时间复杂度：插入O(logn), 删除O(logn)，修改2O(logn)
  * 特点：心搏时间动态变化，超时时间动态修改，使用timerfd_create统一事件源。
  * 思路：
 	* 每个连接用shared_ptr管理，红黑树这边也用shared_ptr指向一个连接，因为红黑树支持随机访问，所以删除一个连接时，先把该连接从红黑树上摘下来，再关闭该连接。
	* 每次accept一个连接，给连接设置一个超时时间15s，将连接插入红黑树中。
	* 每次调用心搏函数时候，循环遍历红黑树上的连接一次，将超时的连接关闭(若连接此时还在进行写操做的话，则只是把它从红黑树上摘下来，连接完成写操作后自己关闭连接)，直到遇到没有超时的连接跳出循环。将心搏间隔改为下次最早超时的连接离超时所需的时间。这样能减少心搏函数触发频率，尽量保证，下一次心搏函数执行时有连接超时。
	* 每当有连接完成一次读操作时，重置该连接的超时时间。
<a name="日志类"></a>
## 3. 日志类实现要点
* 一个包含多个线程的进程最好只写一个文件，这样在分析日志时可以避免在多个文件中跳来跳去。
* 工作线程：干事的线程。  
  日志线程：负责收集日志，并写入日志文件。  
  异步日志：日志线程负责从缓冲区收集日志，并写入日志文件。工作线程除了干事之外只管往缓冲区中写日志。  
* 为什么需要异步日志？  
  因为若是由工作线程直接写入日志文件时，会造成工作线程在进行I/O操作时陷入阻塞状态。这可能造成请求方超时，或者耽误发送心跳消息等。<br>
* 多缓冲技术:我参考了muduo使用前后端buffer的思想，即前端一块buffer，后端多块buffer，前端buffer满后，和后端一块空闲的buffer交换，然后由日志线程将这块满了的buffer写入日志文件。<br>
* 为什么需要多缓冲？
    * 前端不是将日志一条一条的传给后端，而是将多条日志拼成一个大buffer传给后端，减少了后端被唤醒的频率，降低了开销。<br>
    * 前端写日志时，当buffer满时，不必等待写磁盘操作。<br>
* 为了及时将日志消息写入文件，即便前端buffer没满，每隔一段时间也会进行交换操作。
* 日志打印的消息格式如下：[日期 时间.微秒][日志级别][线程id][源文件名:行号][正文]<br>
  为了加快日志打印速度，其中线程id的打印，和时间戳的的打印需要使用一点小技巧。   
  * 打印线程id前会先查看线程id是否已经缓存过，若缓存过则直接打印，没有缓存过才使用系统调用syscall(SYS_gettid)获取全局唯一的线程id。  
  * 时间戳长这个样子20190511 12:43:05.787868，每次打印时间戳前，会查看当前时刻和上一次打印时间戳的时刻是否处于同一秒内，若处于同一秒内，则只格式化微秒部分；不处于同一秒内才调用gmtime_r格式化时间部分。至于如何判断和上一次打印时间戳是否处于同一秒内，是通过gettimeofday做到的，这个函数可以求得距离1970年0时0分0秒的微妙数。由于gettimeofday不是系统调用，不会陷入内核，所以调用时间相当快。
  
`注：本日志类仅实现高效的多缓冲异步日志系统，不支持日志文件的滚动功能。`  
### 3.1 日志类总体流程
前端提供一块buffer供各工作线程写入，后端预先分配两块空闲buffer，使用STL中的双向链表维护。当前端buffer满时，和后端的一块空闲buffer交换(交换的是指针，所以速度很快)。若后端没有空闲buffer则会像系统申请一块新的buffer加入双向链表中，所以，该双向链表是会动态增长的，会自己动态增长到合适的长度；当后端日志线程发现有满的后端buffer时，就开始将该满的后端buffer写入日志文件。  
### 3.2 日志类测试环境
* 阿里云轻量级服务器
* 内存: 2G
* CPU: 单核
* 硬盘: 40GB SSD云盘
### 3.3 日志类测试方式
受测试环境限制，因此和muduo的日志库进行横向对比测试，写入100w条日志，每条日志长度100字节，统计写入速度。
* mudo
测试代码：  
```cpp  
#include <stdio.h>
#include<iostream>
#include<muduo/base/Logging.h>
#include<ctime>
using namespace std;
using namespace muduo;

FILE* gFile;

void dummyOutput(const char* msg, int len)
{
	if (gFile)
	{
		fwrite(msg, 1, len, gFile);
	}
}

void dummyFlush()
{
	fflush(gFile);
}

int main()
{
	gFile = fopen("./test.txt", "ae");
	
	Logger::setOutput(dummyOutput);
	Logger::setFlush(dummyFlush);
	cout << "len:"<< strlen("20190510 12:09:47.579382Z 100056 INFO  xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx - main.cpp:143")<<endl;
	clock_t start, end;
	start = clock();
	for (int i = 0; i < 500000; i++)
	{
		LOG_INFO << "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
	}
	end = clock();
	double endTime = (double)(end - start) / CLOCKS_PER_SEC;
	double totaltime = endTime;
	double mBytes = 500000 * 100 / 1000 / 1000;
	double mBytesEachSecond = mBytes / totaltime;
	cout <<"muduo - "<< "Rate:" << mBytesEachSecond << "MB/s" << endl;
	fclose(gFile);
	return 0;
}
```
* HttpServer 
测试代码：  
```cpp
#include<iostream>
#include"base/Logger.h"
#include<ctime>
int main()
{
	Logger::getLogger()->start(true);

	clock_t start, end;
	start = clock();
	cout << "len:" << strlen("[20190511 12:43:05.787868][INFO][108616][/home/hanliu/projects/HttpSever/main.cpp:87][xxxxxxxxxxxxx]") << endl;
	for (int i = 0; i < 500000; i++)
	{
		LogInfo("xxxxxxxxxxxxx");
	}
	end = clock();
	double endTime	 = (double)(end - start) / CLOCKS_PER_SEC;
	double totaltime = endTime;
	double mBytes    = 500000 * 100 / 1000 / 1000;
	double mBytesEachSecond = mBytes / totaltime;
	cout << "mine - "<< "Rate:" << mBytesEachSecond << "MB/s" << endl;

	Logger::stop();
	return 0;
}
```  
### 3.4 日志类测试结果
muduo:   
![](https://github.com/hanAndHan/HttpServer/blob/master/imge/muduoLogTest.png)  
HttpServer:   
![](https://github.com/hanAndHan/HttpServer/blob/master/imge/httpServerLogTest.png)  <br>
由上述结果可见HttpServer中的日志库和muduo中的日志库性能差距不大。
<a name="Buffer"></a>
## 4. Buffer
需要自定义buffer的两个原因：
* 读数据的时候，不知道要接收的数据有多少，如果把缓冲区设计得太大会造成浪费。所以一个Buffer带有一个栈上的缓冲和堆上的缓冲，每次使用readv读取数据，先读到堆上那块缓冲再读到栈上那块缓冲，若栈上的缓冲有数据，则将其append到堆上的缓冲。栈上缓冲的大小为64KB，在一个不繁忙的系统上，程序一般等待在epoll()系统调用上，一有数据到达就会立刻唤醒应用程序来读取数据，那么每次read的数据不会超过几KB(一两个以太网frame)，陈硕在书中写到64KB缓冲足够容纳千兆网在500us内全速发送的数据。
* 写数据的时候，若已连接套接字对应的写缓冲区装不下了，剩下的没写的数据保存在自定义buffer中，然后监听已连接套接字上面的写事件，当写事件就绪时，继续将数据写入写缓冲区。若还写不完，继续保持监听写事件，若写完了，停止监听写事件，防止出现busyloop。
<a name="连接关闭"></a>
## 5. 连接关闭
这部分简要说明一个连接对象关闭的过程，每个Connection对象使用shared_ptr进行管理。
* 连接到来时，创建一个Connection对象，使用shared_ptr管理，存入unodered_map中，引用计数为1。
* 当连接关闭时，Channel上注册的读事件就绪，会调用Channel的handleEvent执行读回调函数<br>void Connection::handleRead(int64_t receiveTime)。
* 在handleRead中调用handleClose，handleClose中使用了shared_from_this()，引用计数加1变为2。
* 在handleClose中调用Server::removeConnection，然后再erase，这时候引用计数变为1，然后使用bind，引用计数又加1变为2，ioLoop->queueInLoop(std::bind(&Connection::connectDestroyed, conn))。
* handleClose调用结束，之前shared_from_this()得到的对象析构，引用计数减1变为1。
* handleRead调用结束。
* connectDestroyed调用结束，引用计数减1变为0。
* connection对象析构。
<a name="压力测试"></a>
## 6. 压力测试
## 6.1 测试环境
* unbuntu 16.04 VMware Workstation 14 Player
* 内存：4G
* CPU：I5-8300H
## 6.2 测试方法
* 为了不受带宽限制，选择本地环境进行测试
* 使用工具Webbench，开启1000客户端进程，时间为60s
* 分别测试短连接和长连接的情况
* 关闭日志打印功能，关闭定时器剔除空闲连接功能
* 为避免磁盘IO对测试结果的影响，测试响应为内存中的"HTTP/1.1 200 OK\r\nContent-type: text/plain\r\n\r\nHello World"
* 我的服务器在实现上参考了muduo，Linya学长去年也参考muduo实现了一个WebSever:https://github.com/linyacool/WebServer
因此我将与Linya学长的WebSever进行一个小小的对比，测试过程中关闭WebSever的所有输出及日志打印功能
* 线程池开启4线程
* 因为发送的内容很少，为避免发送可能的延迟，关闭Nagle算法
## 6.3 测试结果及分析
| 服务器 | 短连接QPS | 长连接QPS | 
| - | :-: | -: | 
| HttpServer | 65304 | 184162 | 
| WebServer | 61387| 174518 |

测试截图：
* HttpServer短连接测试  
![shortHttp](https://github.com/hanAndHan/HttpServer/blob/master/imge/shortConHttpSever_1.png)
* WebSever短连接测试  
![shortWeb](https://github.com/hanAndHan/HttpServer/blob/master/imge/shortConWebSever_1.png)
* HttpServer长连接测试  
![keepHttp](https://github.com/hanAndHan/HttpServer/blob/master/imge/longConHttpSever_1.png)
* Web长连接测试  
![keepWeb](https://github.com/hanAndHan/HttpServer/blob/master/imge/longConWebSever_1.png)
## 6.4 测试结果分析
* 由于长连接省去了频繁创建关闭连接的开销，所以长连接的qps明显高于短连接，大概3倍左右的水平。
* HttpSever无论是长连接还是短连接都略高于WebSever，究其原因有两点：
	* HttpSever实现了自定义buffer，每个连接新建时就初始好了一块1K大小的buffer，在写入少量数据时，避免了动态扩容的开销；而WebSever是直接使用的string作为buffer。
	* HttpSever采用的EPOLL LT模式， 读写的时候不必等候出现EAGAIN，可以节省系统调用次数，降低延迟。而WebSever采用的是EOLL ET模式每次读写必须等到EAGAIN为止，否则会出现漏掉事件没处理的bug。


