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
/*-----------------各个全局变量定义-----------------------------------*/
const int MAXSIZE = 2048;
SOCKADDR_IN server_addr;
SOCKET server;
int SEQ = 0;
int ACK = 0;
const int cwnd = 10;
int addrlen = sizeof(server_addr);
int exptdseqnum = 0;
/*-------------------类型帧首部定义-------------------------*/
class Header {
public:
    u_short datasize = 0;
    u_short sum = 0;
    u_char flag = 0;
    u_char ack = 0;//==seq
    u_char seq = 0;//seq==ack


    Header() { flag = 0; }
    void setHeader(u_short d, u_char f, u_short se) {
        this->datasize = d;
        this->seq = se;
        this->flag = f;

    }

    void show_header() {
        cout << "datasize: " << (int)this->datasize << " seq: " << (int)this->seq << " flag: " << (int)this->flag << endl;
    }

};

/*--------------------------------数据包结构定义-----------------------------------------------*/
struct Packet {
    Header header;//UDP首部
    char* Buffer;//数据区
    Packet() {
        Buffer = new char[MAXSIZE + sizeof(header)]();
    }
};
/*----------------------------所要用的函数--------------------------------------------------*/
void printsplit();
u_short cksum(u_short* buff, int size);
long long time(long long head);
void setSum(Header& header);
bool check_sign(Header header, u_char sign);
bool interact(SOCKET& sockServ, SOCKADDR_IN& ClientAddr, int& ClientAddrLen, string type);
int receive(SOCKET& servsocket, SOCKADDR_IN& clientaddr, int& len, char* message);
void start(int len);

/*-----------------------各函数定义----------------------------------------------------------*/
void init() {
    WSADATA wsadata;
    WSAStartup(MAKEWORD(2, 2), &wsadata);
    server_addr.sin_family = AF_INET;//使用IPV4
    server_addr.sin_port = htons(4000);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server = socket(AF_INET, SOCK_DGRAM, 0);
    if (bind(server, (SOCKADDR*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR)//绑定套接字，进入监听状态
        cout << "bind error!" << endl;
    else
        cout << "waiting client....." << endl;

}
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
    header.sum = 0;
    u_short s = cksum((u_short*)&header, sizeof(header));
    header.sum = s;
}
bool check_sign(Header header, u_char sign) {
    if (header.flag == sign && cksum((u_short*)&header, sizeof(header)) == 0)
        return true;
    else
        return false;
}

bool interact(SOCKET& sockServ, SOCKADDR_IN& ClientAddr, int& ClientAddrLen, string type) {
    //type 是hello或goodbye
    int sendsign = 0, recvsign = 0;
    if (type == "hello") {
        sendsign = 0x5;
        recvsign = 0x4;
    }
    else {
        sendsign = 0x3;
        recvsign = 0x2;
    }

    Header recvh;
    Header sendh;
    char* Buffer = new char[sizeof(recvh)];
    int res = 0;
    //接收第一次握手信息
    while (true) {
        res = recvfrom(sockServ, Buffer, sizeof(recvh), 0, (sockaddr*)&ClientAddr, &ClientAddrLen);
        if (res == -1) {
            cout << "first " << type << " ----miss" << endl;
            continue;
        }
        memcpy(&recvh, Buffer, sizeof(recvh));
        if (check_sign(recvh, recvsign)) {
            SEQ = recvh.seq;
            cout << "first " << type << " ----checked" << endl;
            sendh.setHeader(0, sendsign, 0);
            setSum(sendh);
            memcpy(Buffer, &sendh, sizeof(sendh));
            res = sendto(sockServ, Buffer, sizeof(sendh), 0, (sockaddr*)&ClientAddr, ClientAddrLen);
            if (res == -1) {
                return false;
            }
            else {
                cout << "connect/disconnect successfully!" << endl;
                break;
            }
        }
        else {
            cout << "connect/disconnect failed!" << endl;
            return false;
        }
    }
}

int receive(SOCKET& servsocket, SOCKADDR_IN& clientaddr, int& len, char* filedata) {
    Header sendh;
    sendh.ack = exptdseqnum ; //准备收的包的ack值应为期待值
    setSum(sendh); //mkpkt(expseq)
    Packet recvp;
    char* buf = new char[sizeof(sendh)];
    int offset = 0;
    while (true) { //一直等待接收中
        int recvlen = recvfrom(servsocket, recvp.Buffer, sizeof(recvp.header) + MAXSIZE, 0, (sockaddr*)&clientaddr, &len);
        if (recvlen) {//收到了东西
            memcpy(&recvp.header, recvp.Buffer, sizeof(recvp.header));
            if ( exptdseqnum == recvp.header.seq && recvp.header.flag == 0x0 && cksum((u_short*)&recvp.Buffer, strlen(recvp.Buffer))) { //是期待的包

                memcpy(filedata + offset, recvp.Buffer + sizeof(recvp.header), recvlen - sizeof(recvp.header));
                offset += recvp.header.datasize;
                sendh.ack = recvp.header.seq;
                sendh.setHeader(0, 0x1, exptdseqnum);
                setSum(sendh); //mkpkt(expseq)
                memcpy(buf, &sendh, sizeof(sendh));
                sendto(servsocket, buf, sizeof(sendh), 0, (sockaddr*)&clientaddr, len); //sndpkt
                cout << "[EXPECTED packet] send ack:" << (int)sendh.ack<<" ";
                sendh.show_header();
                exptdseqnum++;
                exptdseqnum %= 256;

            }
            else if(exptdseqnum == recvp.header.seq && recvp.header.flag == 0x7 && cksum((u_short*)&recvp.Buffer, strlen(recvp.Buffer))){
                
                sendh.ack = recvp.header.seq;
                memcpy(filedata + offset, recvp.Buffer + sizeof(recvp.header), recvlen - sizeof(recvp.header));
                offset += recvp.header.datasize;
                sendh.setHeader(0, 0x7, recvp.header.seq);
                setSum(sendh); //mkpkt(expseq)
                memcpy(buf, &sendh, sizeof(sendh));
                if( sendto(servsocket, buf, sizeof(sendh), 0, (sockaddr*)&clientaddr, len))//sndpkt
                    cout << "the file has been received" << endl;
                break;
            }
            else {
                //continue;//不是期待的包，嘛也不干因为发送端也得忽略
                //下面浅试一下发送上一个的ack
                int lastack = exptdseqnum - 1;
                sendh.ack = lastack;
                sendh.setHeader(0, 0x0, lastack);//发送重复的ack，flag置0和别的区分一下
                setSum(sendh); //mkpkt(expseq)
                memcpy(buf, &sendh, sizeof(sendh));
                sendto(servsocket, buf, sizeof(sendh), 0, (sockaddr*)&clientaddr, len); //sndpkt
                cout << "[UNEXPECTED packet] send last ack:" << (int)sendh.ack<<" ";
                sendh.show_header();

            }
        }
    }

    exptdseqnum = 0;
    return offset;

}
void start(int len) {
    char* fileName = new char[20];
    char* myfile = new char[100000000];

    int namelen = receive(server, server_addr, len, fileName);
    int filelen = receive(server, server_addr, len, myfile);

    string file;
    for (int i = 0; i < namelen; i++) {
        file = file + fileName[i];
    }
    ofstream fout(file.c_str(), ofstream::binary);
    for (int i = 0; i < filelen; i++) {
        fout << myfile[i];
    }
    fout.close();
    cout << "the file has been downloaded." << endl;
}

#endif

#pragma once
