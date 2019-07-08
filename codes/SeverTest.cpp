// @Author Liu Han
// @Email  hancurrent@foxmail.com
#include<iostream>
#include"./TCP/Server.h"
#include"./net/EventLoop.h"
#include"./base/Logger.h"
#include<string>
using namespace std;
int main(int argc, const char* argv[])
{
	if (argc != 4)
	{
		printf("eg: ./a.out ip port path\n");
		exit(1);
	}
	cout << "启动" << endl;
	Logger * log = Logger::getLogger();
	log->start(true);

	// ip
	string ip = argv[1];
	//端口
	string port = argv[2];
	// 修改进程的工作目录, 方便后续操作
	int ret = chdir(argv[3]);
	if (ret == -1)
	{
		perror("chdir error");
		exit(1);
	}
	EventLoop loop;
	Server sever(&loop, ip, port, 4);
	sever.start(true);
	loop.loop();
	log->stop();
	return 0;
}
