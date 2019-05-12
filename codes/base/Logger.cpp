// @Author Lin Han
// @Email  hancurrent@foxmail.com
#include "Logger.h"
MutexLock Logger::mutex;
Condition Logger::readableBuf(Logger::mutex);
Thread    Logger::readThread(Logger::Threadfunc);
//std::shared_ptr<Logger> Logger::myLogger = nullptr;
std::shared_ptr<Buffer> Logger::curBuf = nullptr;
std::list<std::shared_ptr<Buffer>> Logger::bufList;
Logger *	 Logger::myLogger             = new Logger();
int			 Logger::readableNum          = 0;
bool		 Logger::isRunningFunc		  = true;
bool		 Logger::isRunningThreadFunc  = true;
bool		 Logger::startd				  = false;
int		   	 Logger::ioNumbers            = 0;
int			 Logger::fd                   = -1;
std::string  Logger::logFileName;
TimeSinceGMT Logger::timeNow;

__thread time_t t_lastSecond;
__thread char t_time[32];

Logger::Logger()
{
	/* 创建当前Buffer */
	curBuf = std::make_shared<Buffer>();
	/* 创建两块备用Buffer */
	bufList.resize(2);
	(*bufList.begin()) = std::make_shared<Buffer>();
	(*(++bufList.begin())) = std::make_shared<Buffer>();
}
Logger::~Logger()
{

}
Logger * Logger::getLogger()
{
	//myLogger = std::make_shared<Logger>();
	return myLogger;
}
std::shared_ptr<Buffer>& Logger::useFul()
{
	auto iter = bufList.begin();
	/* 查询是否存在可用的Buffer */
	for (; iter != bufList.end(); ++iter)
	{
		if ((*iter)->Size() == 0)
		{
			break;
		}
	}
	/* 不存在则创建一块新Buffer并返回 */
	if (iter == bufList.end())
	{
		std::shared_ptr<Buffer> p = std::make_shared<Buffer>();
		/* 统一使用右值来提高效率 */
		bufList.push_back(std::move(p));
		return bufList.back();
	}
	return *iter;
}
void Logger::logStream(const char* mesg, size_t len)
{
	/*日志类启动了才打印日志*/
	assert(startd);
	/* 上锁，使用lock和condition_variable条件变量结合 */
	MutexLockGuard lock(mutex);
	/* 判断当前缓冲是否还写得下，写不下了则与备用缓冲交换新的 */
	if (curBuf->avail() > len)
	{
		curBuf->append(mesg, int(len));
	}
	else
	{
		/* 得到一块备用缓冲 */
		auto & useBuf = useFul();
		/* 交换指针即可 */
		curBuf.swap(useBuf);
		/*写入之前写不下的数据*/
		curBuf->append(mesg, int(len));
		/* 可读缓冲数量增加 */
		++readableNum;
		/* 唤醒阻塞后台线程 */
		readableBuf.notifyAll();
	}
}
					             /*日志级别              文件名       行号                可变参数的宏*/
void Logger::logStream(const char* pszLevel, const char* pszFile, int lineNo, const char* pszFmt, ...)
{
	/*日志类启动了才打印日志*/
	assert(startd);
	/*拼接打印信息*/
	char msg[256] = { 0 };

	va_list vArgList;
	va_start(vArgList, pszFmt);
	vsnprintf(msg, 256, pszFmt, vArgList);
	va_end(vArgList);

	int microseconds = timeFormate();
	char content[512] = { 0 };

	//size_t len = static_cast<size_t>(sprintf(content, "[%s.%d][%s][%d][%s:%d %s]%s\n",
	//	t_time,
	//	microseconds,
	//	pszLevel,
	//	CurrentThread::tid(),
	//	pszFile,
	//	lineNo,
	//	pszFuncSig,
	//	msg)); 

	size_t len = static_cast<size_t>(sprintf(content, "[%s.%d][%s][%d][%s:%d][%s]\n",
		t_time,
		microseconds,
		pszLevel,
		CurrentThread::tid(),
		pszFile,
		lineNo,
		msg)); 

	/* 上锁，使用lock和condition_variable条件变量结合 */
	MutexLockGuard lock(mutex);
	//cout << curBuf->avail() << " " << len << endl;
	/* 判断当前缓冲是否还写得下，写不下了则与备用缓冲交换新的 */
	if (curBuf->avail() > len)
	{
		curBuf->append(content, len);
	}
	else
	{
		/* 得到一块备用缓冲 */
		auto & useBuf = useFul();
		/* 交换指针即可 */
		curBuf.swap(useBuf);
		/*写入之前写不下的数据*/
		curBuf->append(content, len);
		/* 可读缓冲数量增加 */
		++readableNum;
		/* 唤醒阻塞后台线程 */
		readableBuf.notifyAll();
	}
}
int Logger::timeFormate()
{
	/*距离GMT的毫秒数*/
	int64_t microSecondsSinceEpoch = timeNow.microSecondsSinceEpoch();
	/*将上述毫秒数转换成 秒与毫秒*/
	time_t seconds = static_cast<time_t>(microSecondsSinceEpoch / 1000000);
	int microseconds = static_cast<int>(microSecondsSinceEpoch % 1000000);
	/*和上一次打印时间戳不处于同一秒内才调用gmtime_r获取当前时间*/
	if (seconds != t_lastSecond)
	{
		t_lastSecond = seconds;
		struct tm tm_time;
		::gmtime_r(&seconds, &tm_time); // FIXME TimeZone::fromUtcTime，可以将GMT时间和UTC时间看作等同的
		int len = snprintf(t_time, sizeof(t_time), "%4d%02d%02d %02d:%02d:%02d",//若要使用北京时间，可将tm_time.tm_hour+8
			tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
			tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);
		assert(len == 17); (void)len;
	}
	return microseconds;
}
void Logger::func()
{

	MutexLockGuard lock(mutex);
	/* 如果备用缓冲并无数据可读，阻塞等待唤醒 */
	while (readableNum == 0 && isRunningFunc)
	{
		bool waitTimeSuccess = readableBuf.waitForSeconds(3);
		//bool waitTimeSuccess = readableBuf.waitForMilliseconds(30);
		if (waitTimeSuccess && curBuf->Size() > 0 && readableNum == 0)//等了3秒钟缓冲区中有数据但是缓冲区还没满,不等了，后台工作线程直接写文件
		{
			/* 得到一块备用缓冲 */
			auto & useBuf = useFul();
			/* 交换指针即可 */
			curBuf.swap(useBuf);
			/* 可读缓冲数量增加 */
			++readableNum;
		}
	}
	/* 找数据不为空的Buffer */
	auto iter = bufList.begin();
	for (; iter != bufList.end(); ++iter)
	{
		if ((*iter)->Size() != 0)
			break;
	}
	/* 如果到末尾没找到，没有数据可读 */
	if (iter == bufList.end())
	{
		return;
	}
	else
	{
		/* 将满的缓冲写到文件中 */
		++ioNumbers;
		write(fd, (*iter)->Data(), (*iter)->Size());
		/* 清空缓冲,这一步可以不用，只需要重置writable，下次写的时候直接覆盖之前的就可以 */
		//bzero((*iter)->Data(), (*iter)->Capacity());
		/* 归位readable和writable */
		(*iter)->setSize();
		/* 可读缓冲数量减1 */
		--readableNum;
	}
}
void Logger::Threadfunc()
{
	while (isRunningThreadFunc)
	{
		func();
	}
	/*结束日志类时，把curBuf中最后剩下的数据写入文件*/
	if (isRunningThreadFunc == false && curBuf->Size() > 0)
	{
		write(fd, curBuf->Data(), curBuf->Size());
		/* 清空缓冲 */
		bzero(curBuf->Data(), curBuf->Capacity());
		/* 归位readable和writable */
		curBuf->setSize();
	}
}
bool Logger::start()
{
	if (readThread.isStarted())
		return true;
	else
	{
		if (logFileName.empty())
		{
			char timestr[64] = { 0 };
			sprintf(timestr, "%s.httpServer.log", timeNow.toFormattedString().data());
			logFileName = timestr;
		}
		fd = open(logFileName.data(), O_RDWR | O_APPEND | O_CREAT , 0644);
		if (fd < 0)
		{
			perror("open error\n");
			return false;
		}
		readThread.start();
		startd = true;
		return true;
	}
}
void Logger::stop()
{
	isRunningThreadFunc = false;
	isRunningFunc = false;
	readableBuf.notify();
	readThread.join();
	if (myLogger != nullptr)
	{
		delete myLogger;
		myLogger = nullptr;
	}
}

void Logger::setLogFileName(const char * fileName)
{
	logFileName = fileName;
}
