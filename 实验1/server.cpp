#include <winsock2.h> // winsock2的头文件
#include <iostream>
#include<time.h> 
#include<string>
#include <sstream>
#pragma comment(lib, "ws2_32.lib")
using namespace std; 
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

DWORD WINAPI ThreadFun(LPVOID lpThreadParameter) {
	// 与客户端通讯，发送或者接受数据
	SOCKET c = (SOCKET)lpThreadParameter;//接受传来的线程参数，实际是主函数中的client这个socket
	// 发送与客户端连接成功的信息
	char buf[1000] = { 0 };
	sprintf_s(buf, "连接成功！欢迎用户 %d 进入聊天室！（退出聊天请按q）", c);
	send(c, buf, 1000, 0);

	// 循环接收客户端数据
	int ret = 0;
	do {
		char server_buf[1000] = { 0 };
		ret = recv(c, server_buf, 1000, 0);//接受客户端发来的字符消息

		cout << "================================================" << endl;
		cout << TimeNow();
		cout << "用户" << c << ":    " << server_buf << endl;//打印消息内容

	} while (ret != SOCKET_ERROR && ret != 0);
	cout << "================================================" << endl;
	cout << endl;
	cout << endl;
	cout << endl;
	cout << TimeNow();
	cout << "用户" << c << "离开了聊天室！" << endl;;

	return 0;
}

int main(){
	WSADATA wsaData;//初始化Socket DLL，协商使用的Socket版本
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0){ //MAKEWORD(2, 2)生成 WSAStartup 期望的版本控制字。
		cout << TimeNow();
		cout << "WSAStartup Error:" << WSAGetLastError() << endl;
		return 0;
	}
	else{
		cout << TimeNow();
		cout << "初始化SocketDLL成功!" << endl;
	}

	// 创建流式套接字
	cout << TimeNow();
	cout << "正在创建流式套接字......" << endl;
	SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);//指定创建的socket的地址类型、服务类型及TCP协议
	
	if (s == INVALID_SOCKET){
		cout << TimeNow();
		cout << "socket错误:" << WSAGetLastError() << endl;
		return 0;
	}
	else {
		cout << TimeNow();
		cout << "创建成功" << endl;
	}

	// 绑定端口和ip
	cout << TimeNow();
	cout << "正在绑定端口和IP" << endl;
	sockaddr_in addr;
	memset(&addr, 0, sizeof(sockaddr_in));
	addr.sin_family = AF_INET;//地址类型
	addr.sin_port = htons(8000);//端口号
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");//IP地址

	int len = sizeof(sockaddr_in);
	if (bind(s, (SOCKADDR*)&addr, len) == SOCKET_ERROR){//将本地地址绑定到s这个socket
		cout << TimeNow();
		cout << "绑定错误:" << WSAGetLastError() << endl;
		return 0;
	}
	else{
		cout << TimeNow();
		cout << "端口号：8000 IP：127.0.0.1绑定成功！" << endl;

	}
	
	// 监听远程连接是否到来
	listen(s, 5);
	cout << TimeNow();
	cout << "等待用户进入聊天室......" << endl;
	// 主线程循环接收客户端的连接
	while (true){
		sockaddr_in addrClient; //远程端的地址
		len = sizeof(sockaddr_in);
		// 接受成功返回与client通讯的Socket
		SOCKET client = accept(s, (SOCKADDR*)&addrClient, &len); //接受server请求等待队列中的连接请求即client
		if (client != INVALID_SOCKET)
		{
			cout << TimeNow();
			cout << "用户" << client << "连接成功！" << endl;
			// 创建线程，并且传入与client通讯的套接字
			HANDLE hThread = CreateThread(NULL, 0, ThreadFun, (LPVOID)client, 0, NULL);
			if(!hThread)
				CloseHandle(hThread); // 关闭对线程的引用
		}

	}

	// 关闭监听套接字并清理winsock2的环境
	closesocket(s);
	WSACleanup();
	return 0;
}


