#include<winsock2.h>//winsock2的头文件
#include<iostream>
#include<time.h> 
#include <sstream>
using  namespace std;
#pragma comment(lib, "ws2_32.lib")
string TimeNow() {
	time_t now = time(NULL);
	tm* tm_t = localtime(&now);
	std::stringstream ss; //将时间转化为字符串放进ss中
	ss << "[";
	if (tm_t->tm_hour < 10)
		ss << "0";
	ss << tm_t->tm_hour << ":";
	if (tm_t->tm_min < 10)
		ss << "0";
	ss << tm_t->tm_min << ":";
	if (tm_t->tm_sec < 10)
		ss << "0";
	ss << tm_t->tm_sec << "]";
	string str;
	ss >> str; //将stringstream放进str中
	//char* p = const_cast<char*>(str.c_str()); //将str转化为char*类型
	return str;
}
int  main(){
	//加载winsock2的环境
	WSADATA  wd;
	if (WSAStartup(MAKEWORD(2, 2), &wd) != 0){
		cout << TimeNow();
		cout << "WSAStartup错误：" << GetLastError() << endl;
		return 0;
	}

	//创建流式套接字
	SOCKET  s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s == INVALID_SOCKET){
		cout << TimeNow();
		cout << "socket错误：" << GetLastError() << endl;
		return 0;
	}

	//链接服务器
	sockaddr_in   addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(8000);
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");

	int len = sizeof(sockaddr_in);
	if (connect(s, (SOCKADDR*)&addr, len) == SOCKET_ERROR){//向服务端发送建连请求
		cout << TimeNow();
		cout << "连接错误：" << GetLastError() << endl;
		return 0;
	}

	//接收服务端的消息
	char buf[1000] = { 0 };
	recv(s, buf, 1000, 0);
	cout << buf << endl;

	//随时给服务端发消息
	int  ret = 0;
	int flag = 1; //是否退出的标记
	do{
		char client_buf[1000] = { 0 };
		cout << "请输入消息内容:";
		cin.getline(client_buf,1000);
		if (client_buf[0] == 'q') {
			flag = 0; //如果输入q则跳出循环
		}
		if(flag==1)
		ret = send(s, client_buf, 1000, 0);
	} while (ret != SOCKET_ERROR && ret != 0 && flag==1);

	//关闭监听套接字并清理winsock2的环境
	closesocket(s);
	WSACleanup();
	return 0;
}
