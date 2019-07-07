// @Author Liu Han
// @Email  hancurrent@foxmail.com
#pragma once
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>
#include<errno.h>
#include"../TCP/Buffer.h"
#include<sys/mman.h>
#include<string>
#include<memory>
#include"../base/Logger.h"
#include<sstream>
using namespace std;
class Http
{
public:
	Http() :isDir_(false), isFile_(false), is404_(false)
	{

	}
	~Http()
	{

	}
	// 读http请求数据，然后开始处理http请求
	void handle(Buffer & inputBuffer, Buffer & outputBuffer)
	{
		// 读请求行
		const char* pos = inputBuffer.findCRLF();
		if (pos == NULL)
			return;
		std::string URI = std::string(inputBuffer.peek(), pos);
		//cout << "请求行" << URI << endl;
		//剩下的报头 空行 正文 全部丢弃/inputBuffer虚假的置空
		inputBuffer.retrieveAllFake();
		//outputBuffer虚假置空
		outputBuffer.retrieveAllFake();
		//判断是否是get请求
		size_t pos2 = URI.find(' ');
		std::string method = std::string(URI.begin(), URI.begin() + pos2);
		//是get请求则开始处理，不是则忽略
		if (strncasecmp("get", URI.data(), 3) == 0)//strncasecmp中的n表示比较两个串的前n位，case表示比较的时候不区分大小写
		{
			// 处理http请求
			httpRequest(URI.data(), outputBuffer);
		}
		else
		{
			return;
		}
	}
private:
	bool isDir_;
	bool isFile_;
	bool is404_;
	// 处理http请求
	//request是请求行
	void httpRequest(const char* request, Buffer & outputBuffer)
	{
		// 拆分http请求行
		// get /xxx http/1.1
		char method[12], path[1024], protocol[12];
		sscanf(request, "%s %s %s", method, path, protocol);

		//printf("method = %s, path = %s, protocol = %s\n", method, path, protocol);

		// 解码 将不能识别的中文乱码 - > 能识别的中文
		// 解码 %23 %34 %5f
		decodeStr(path, path);

		// 处理path  /xx
		// 去掉path中的/
		const char* file = path + 1;
		// 如果没有指定访问的资源,则http协议传过来的path会被设置成"/"。我们在设计程序处理这种情况时默认显示资源目录中的内容
		if (strcmp(path, "/") == 0)
		{
			// 设置file的值为资源目录的当前位置
			file = "./";
		}

		// 获取文件属性
		struct stat st;
		int ret = stat(file, &st);
		if (ret == -1)
		{
			// show 404
			is404_ = true;
			sendRespondHead(404, "File Not Found", getFileType(".html"), getFileSize("../404.html"), outputBuffer);
			sendFile("../404.html", getFileSize("../404.html"), outputBuffer);
			return;
		}
		else
		{
			is404_ = false;
		}

		// 判断是目录还是文件
		// 如果是目录
		if (S_ISDIR(st.st_mode))
		{
			isDir_ = true;
			isFile_ = false;
			//  发送响应头（状态行+消息报头+空行）
			//sendRespondHead(200, "OK", getFileType(".html"), 300, outputBuffer);
			// 发送目录信息
			sendDir(file, outputBuffer);
		}//如果是文件
		else if (S_ISREG(st.st_mode))
		{
			isFile_ = true;
			isDir_ = false;
			// 发送响应头（状态行+消息报头+空行）
			sendRespondHead(200, "OK", getFileType(file), st.st_size, outputBuffer);
			// 发送文件内容
			sendFile(file, st.st_size, outputBuffer);
		}
	}

	// 发送响应头（状态行+消息报头+空行）
	void sendRespondHead(int no, const char* desp, const char* type, __off_t len, Buffer & outputBuffer)
	{		
		char buf[1024] = { 0 };
		// 状态行+消息报头+空行
		sprintf(buf, "http/1.1 %d %s\r\nConnection: Keep-Alive\r\nContent-Type: %s\r\nContent-Length: %ld\r\nServer: LiuHan's WebServer\r\n\r\n", no, desp, type, len);
		outputBuffer.append(buf, strlen(buf));
		//cout << buf << endl;
	}

	// 发送目录内容(即将目录里的文件或目录的名字拼成一张表发给浏览器)
	void sendDir(const string & dirname, Buffer & outputBuffer)
	{
		// 拼一个html页面<table></table>
		string buf;
		//sprintf(buf, "<html><head><title>目录名: %s</title><link rel = \"shortcut icon\" href = \"/favicon.ico\" type=\"image/x-icon\" /></head>", dirname);
		//sprintf(buf, "<html><head><title>目录名: %s</title><link rel = \"icon\" href = \"data:;base64,=\"></head>", dirname);
		buf = "<html><head><title>目录名:" + dirname + "</title></head><body><h1>当前目录:" + dirname + "</h1><table>";


		char enstr[1024] = { 0 };
		char path[1024] = { 0 };
		// 目录项二级指针
		struct dirent** ptr;
		int num = scandir(dirname.data(), &ptr, NULL, alphasort);
		// 遍历
		for (int i = 0; i < num; ++i)
		{
			char* name = ptr[i]->d_name;

			// 拼接文件的完整路径
			sprintf(path, "%s/%s", dirname.data(), name);
			struct stat st;
			stat(path, &st);
			//将超链接编码成16进制,免得浏览器下次发送超链接看见是中文又去转码一次
			encodeStr(enstr, sizeof(enstr), name);
			// 如果是文件
			if (S_ISREG(st.st_mode))
			{
				buf = buf + "<tr><td><a href=\"" + enstr + "\">" + name + "</a></td><td>" + DoubleToString((double)st.st_size/(1024*1024),2) + "MB" + "</td></tr>";
			}
			// 如果是目录
			else if (S_ISDIR(st.st_mode))
			{
				buf = buf + "<tr><td><a href=\"" + enstr + "/" + "\">" + name + "/" + "</a></td><td>" + "</td></tr>";
			}
			// 字符串拼接
		}
		string blank = "  <br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br><br>";
		string records = "<div class=\"footer\"><div style = \"width:300px;margin:0 auto; padding:5px 0;\"><a target = \"_blank\" href = \"http://www.beian.gov.cn/portal/registerSystemInfo?recordcode=50022702000404\" style = \"display:inline-block;text-decoration:none;height:20px;line-height:20px;\"><img src = \"http://www.liuhan.store/img/%e5%a4%87%e6%a1%88%e5%9b%be%e6%a0%87.png\" style = \"float:left;\" / ><p style = \"float:left;height:20px;line-height:20px;margin: 0px 0px 0px 5px; color:#939393;\">渝公网安备 50022702000404号</p></a></div></div>";
		buf = buf + "</table>"+blank+records+"</body></html>";
		sendRespondHead(200, "OK", getFileType(".html"), buf.size(), outputBuffer);
		outputBuffer.append(buf.data(), buf.size());	
	}

	// 发送文件内容
	void sendFile(const char* filename, __off_t len, Buffer & outputBuffer)
	{
		// 打开文件
		int fd = open(filename, O_RDONLY);

		if (fd == -1)
		{
			//打印下日志，open失败
			LogFatal("open failed");
		}
		char * memp = (char *)mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0);
		if (memp == MAP_FAILED)//检测映射时是否出错
		{
			LogFatal("mmap error");
		}
		//int bytes = read(fd, buf, 30 * 1024 * 1024);
		//outputBuffer.append(buf, bytes);
		outputBuffer.append(memp, len);
		close(fd);
		int flag = munmap(memp, len);//释放映射的内存空间
		if (flag == -1)
		{
			//打印下日志，munmap失败
			LogFatal("munmap error");
		}
	}


	// 单个数16进制数转化为10进制
	int hexit(char c)
	{
		if (c >= '0' && c <= '9')
			return c - '0';
		if (c >= 'a' && c <= 'f')
			return c - 'a' + 10;
		if (c >= 'A' && c <= 'F')
			return c - 'A' + 10;

		return 0;
	}
	/*
	*  这里的内容是处理%20之类的东西！是"解码"过程。
	*  %20 URL编码中的‘ ’(space)
	*  %21 '!' %22 '"' %23 '#' %24 '$'
	*  %25 '%' %26 '&' %27 ''' %28 '('......
	*  相关知识html中的‘ ’(space)是&nbsp
	*/
	//16进制转成了10进制
	void decodeStr(char *to, char *from)
	{
		for (; *from != '\0'; ++to, ++from)
		{
			if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2]))
			{

				*to = static_cast<char>(hexit(from[1]) * 16 + hexit(from[2]));

				from += 2;
			}
			else
			{
				*to = *from;

			}

		}
		*to = '\0';
	}
	//"编码"，用作回写浏览器的时候，将除字母数字及/_.-~以外的字符转义后回写。
	//10进制转16进制
	void encodeStr(char* to, int tosize, const char* from)
	{
		int tolen;

		for (tolen = 0; *from != '\0' && tolen + 4 < tosize; ++from)
		{
			if (isalnum(*from) || strchr("/_.-~", *from) != (char*)0)
			{
				*to = *from;
				++to;
				++tolen;
			}
			else
			{
				sprintf(to, "%%%02x", (int)*from & 0xff);
				to += 3;
				tolen += 3;
			}

		}
		*to = '\0';
	}
	// 通过文件名获取文件的类型
	const char *getFileType(const char *name)
	{
		const char* dot;

		// 自右向左查找‘.’字符, 如不存在返回NULL
		dot = strrchr(name, '.');
		if (dot == NULL)
			return "text/plain; charset=utf-8";
		if (strcmp(dot, ".pdf") == 0)
			return "application/pdf";
		if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
			return "text/html; charset=utf-8";
		if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
			return "image/jpeg";
		if (strcmp(dot, ".gif") == 0)
			return "image/gif";
		if (strcmp(dot, ".png") == 0)
			return "image/png";
		if (strcmp(dot, ".css") == 0)
			return "text/css";
		if (strcmp(dot, ".au") == 0)
			return "audio/basic";
		if (strcmp(dot, ".wav") == 0)
			return "audio/wav";
		if (strcmp(dot, ".avi") == 0)
			return "video/x-msvideo";
		if (strcmp(dot, ".mov") == 0 || strcmp(dot, ".qt") == 0)
			return "video/quicktime";
		if (strcmp(dot, ".mpeg") == 0 || strcmp(dot, ".mpe") == 0)
			return "video/mpeg";
		if (strcmp(dot, ".vrml") == 0 || strcmp(dot, ".wrl") == 0)
			return "model/vrml";
		if (strcmp(dot, ".midi") == 0 || strcmp(dot, ".mid") == 0)
			return "audio/midi";
		if (strcmp(dot, ".mp3") == 0)
			return "audio/mpeg";
		if (strcmp(dot, ".mp4") == 0)
			return "audio/mp4";
		if (strcmp(dot, ".ogg") == 0)
			return "application/ogg";
		if (strcmp(dot, ".pac") == 0)
			return "application/x-ns-proxy-autoconfig";

		return "text/plain; charset=utf-8";
	}
	//获取文件大小
	__off_t getFileSize(const char * filename)
	{
		struct stat st;
		int ret = stat(filename, &st);
		if (ret == -1)
		{
			//打印下日志，404html不存在
			LogError("404html not exit");
		}
		return st.st_size;
	}
	//double 转 string 精度控制
	string DoubleToString(const double value, unsigned int precisionAfterPoint)
	{
		ostringstream out;
		// 清除默认精度
		out.precision(numeric_limits<double>::digits10);
		out << value;

		string res = move(out.str());
		auto pos = res.find('.');
		if (pos == string::npos)
			return res;

		auto splitLen = pos + 1 + precisionAfterPoint;
		if (res.size() <= splitLen)
			return res;

		return res.substr(0, splitLen);
	}
public:
	bool isDir()
	{
		return isDir_;
	}

	bool isFile()
	{
		return isFile_;
	}
	bool isShow404()
	{
		return is404_;
	}
};
