#ifndef MYSERVER_H//定义这个头文件
#define MYSERVER_H
#include <iostream>
#include <WINSOCK2.h>
#include <time.h>
#include <sstream>
#include <windows.h>
#include <fstream>
#include <string>
#include <vector>
#include <io.h>
#pragma comment(lib, "ws2_32.lib")

using namespace std;

const int MAXSIZE = 2048;
SOCKADDR_IN server_addr;
SOCKET server;
int SEQ = 0;
int ACK = 0;
double TIMEOUT = 2.0;

class Header {
public:
    u_short datasize = 0;
    u_short sum = 0;
    u_char flag = 0;
    u_char ack = 0;//seq+1
    u_short seq = 0;

    Header() { flag = 0; }
    void setHeader(u_short d, u_char f, u_short se) {
        this->datasize = d;
        this->seq = se;
        this->flag = f;

    }

    void show_header() {
        cout << " sum: " << (int)this->sum << " seq: " << (int)this->seq << " flag: " << (int)this->flag << endl;
    }

};

struct Packet {
    Header header;//UDP首部
    char* Buffer;//数据区
    Packet() {
        Buffer = new char[MAXSIZE + sizeof(header)]();
    }
};

void printsplit();
u_short cksum(u_short* buff, int size);
long long time(long long head);
void setSum(Header& header);
bool check_sign(Header header, u_char sign);


void printsplit() {
    cout << "--------------------------------------------------------------------------" << endl;
}
u_short cksum(u_short* buff, int size) {
    int count = (size + 1) / 2;
    u_short* buf = new u_short[size + 1];
    memset(buf, 0, size + 1);
    memcpy(buf, buff, size);
    u_short sum = 0;
    while (count--) {//二进制反码求和
        sum += *buf++;
        if (sum & 0xFFFF0000) {
            sum &= 0xFFFF;
            sum++;
        }
    }
    return ~(sum & 0xFFFF);//结果取反
}
long long time(long long head) {
    long long tail, freq;
    QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
    QueryPerformanceCounter((LARGE_INTEGER*)&tail);
    return (tail - head) / freq;
}
void setSum(Header& header) {
    //header.sum = 0;
    u_short s = cksum((u_short*)&header, sizeof(header));
    header.sum = s;
}
bool check_sign(Header header, u_char sign) {
    if (header.flag == sign && cksum((u_short*)&header, sizeof(header)) == 0)
        return true;
    else
        return false;
}


#endif

#pragma once
