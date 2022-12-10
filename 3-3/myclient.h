#ifndef MYCLIENT_H//定义这个头文件
#define MYCLIENT_H
#include <iostream>
#include <WINSOCK2.h>
#include <time.h>
#include<process.h>
#include <sstream>
#include <windows.h>
#include <fstream>
#include <string>
#include <vector>
#include <io.h>
#pragma comment(lib, "ws2_32.lib")

using namespace std;

class Header {
public:
    u_short datasize = 0;
    u_short sum = 0;
    u_char flag = 0;
    u_char ack = 0;//==seq
    u_char seq = 0;//seq==ack

    Header() { flag = 0; }
    //设置header里的各位，注意这里的seq没有模256，需要传参时转换
    void setHeader(u_short d, u_char f, u_short se) {
        this->datasize = d;
        this->seq = se;
        this->flag = f;

    }

    void show_header() {
        cout << "datasize: " << (int)this->datasize << " seq: " << (int)this->seq << " flag: " << (int)this->flag << endl;
    }

};
/*-----------------各个全局变量定义-----------------------------------*/
const int MAXSIZE = 2048;
SOCKADDR_IN server_addr;
SOCKET server;
double TIMEOUT =0.5;
double INTERACT_TIME = 1.0;
double cwnd = 1; //窗口大小
double ssthreash = 16; //阈值
int curAcked=-1;//当前已经确认的 ack 
int nextseqnum = 0;
int dupACKcount = 0;//重复接收的ack；
int base = 0;//基序号也是未确认的第一个包
//慢启动、快速重传、拥塞避免定义：
const int SLOW_START = -1;
const int FAST_RE = 0;
const int CON_AVOID = 1;
int curState = -1;
int addrlen = sizeof(server_addr);
long long head = 0;
int num = 0;

/*-------------------类型帧首部定义-------------------------*/

/*--------------------------------数据包结构定义-----------------------------------------------*/
struct Packet {
    Header header;//UDP首部
    char* Buffer;//数据区
    Packet() {
        Buffer = new char[MAXSIZE + sizeof(header)]();
    }
};
/*----------------------------所要用的函数--------------------------------------------------*/
void varReset() {
    cwnd = 1; //窗口大小
    ssthreash = 16; //阈值
    curAcked = -1;//当前已经确认的 ack 
    nextseqnum = 0;
    dupACKcount = 0;//重复接收的ack；
    base = 0;//基序号也是未确认的第一个包
    curState = -1;
    head = 0;
    num = 0;

}
void showState() {
    switch (curState)
    {
    case(0):
        cout << "[FAST_RE]";
        break;
    case(-1):
        cout << "[SLOW_START]";
        break;
    case(1):
        cout << "[CON_AVOID]";
        break;
    default:
        break;
    }
}
bool cksend(Header h);
bool ckend(Header h);
u_short cksum(u_short* buff, int size);
long long time(long long head);
void setSum(Header& header);
bool check_sign(Header header, u_char sign);
void init();
//void packet_send(SOCKET& clientsocket, SOCKADDR_IN& serveraddr, int& addrlen, char* message, int len, bool END);
void send(SOCKET& clientsocket, SOCKADDR_IN& serveraddr, int& addrlen, char* message, int len);
void start(int len);


/*-----------------------各函数定义----------------------------------------------------------*/

int ckstate() {
    if (cwnd < ssthreash) {
        return SLOW_START;
    }
    else {
        return CON_AVOID;
    }
}
bool isFastRe() {
    return dupACKcount == 3;
}
//三种状态再收到非冗余ack的时候需要更新cwnd和ssthresh，在这里根据不同状态进行更新
int state_after_newack(int curState) {
    if (curState == SLOW_START) {
        cwnd += 1;
        dupACKcount = 0;
        return ckstate();
    }
    if (curState == CON_AVOID) {
        cwnd += (1 / cwnd);
        dupACKcount = 0;
        return ckstate();
    }
    if (curState == FAST_RE) {
        cwnd = ssthreash;
        dupACKcount == 0;
        return ckstate();
    }
    return -2;//啥也不是。error了
}
int state_after_timeout(int curState) {
    if (curState == SLOW_START||curState == FAST_RE) {
        ssthreash = cwnd / 2;
        cwnd = 1;
        dupACKcount = 0;
        return SLOW_START;
    }
    if (curState == CON_AVOID) {
        ssthreash = cwnd / 2;
        cwnd = 1;
        dupACKcount = 0;
        return SLOW_START;
    }

}
bool cksend(Header h) {
    return (h.flag == 0x1);
}
bool ckend(Header h) {
    return (h.flag == 0x7);
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
void init() {
    WSADATA wsadata;
    WSAStartup(MAKEWORD(2, 2), &wsadata);
    server_addr.sin_family = AF_INET;//使用IPV4
    server_addr.sin_port = htons(4001);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server = socket(AF_INET, SOCK_DGRAM, 0);
    //bind(server, (SOCKADDR*)&server_addr, sizeof(server_addr));//绑定套接字，进入监听状态
    //cout << "waiting client....." << endl;

}
bool interact(SOCKET& sockServ, SOCKADDR_IN& ClientAddr, int& ClientAddrLen, string type) {
    //type 是hello或goodbye
    int sendsign = 0, recvsign = 0;
    if (type == "hello") {
        sendsign = 0x4;
        recvsign = 0x5;
    }
    else {
        sendsign = 0x2;
        recvsign = 0x3;
    }
    Header recvh;
    Header sendh;
    char* Buffer = new char[sizeof(recvh)];
    int res = 0;
    sendh.setHeader(0, sendsign, 0);//设置flag
    setSum(sendh);//校验和置0并进行计算
    memcpy(Buffer, &sendh, sizeof(sendh));
    //发送第一次握手信息
    res = sendto(sockServ, Buffer, sizeof(recvh), 0, (sockaddr*)&ClientAddr, ClientAddrLen);

    if (res == -1) {
        cout << "first " << type << "----failed" << endl;
        return false;
    }
    else
        cout << "first " << type << " ----sended" << endl;
    long long head;
    QueryPerformanceCounter((LARGE_INTEGER*)&head);
    u_long mode = 1;
    ioctlsocket(server, FIONBIO, &mode);
    _sleep(300);
    while (recvfrom(sockServ, Buffer, sizeof(recvh), 0, (sockaddr*)&ClientAddr, &ClientAddrLen) <= 0) {
        if (time(head) > INTERACT_TIME) {
            memcpy(Buffer, &sendh, sizeof(sendh));
            sendto(sockServ, Buffer, sizeof(recvh), 0, (sockaddr*)&ClientAddr, ClientAddrLen);
            QueryPerformanceCounter((LARGE_INTEGER*)&head);
            cout << "time out for first " << type << ".resending......" << endl;
            continue;
        }
    }
    memcpy(&recvh, Buffer, sizeof(recvh));
    if (check_sign(recvh, recvsign)) {

        cout << "second " << type << " ----checked" << endl;
        cout << "----------------------------ok---------------------------------" << endl;
        return true;
    }
    else {
        cout << "----------------------------failed---------------------------------" << endl;
        return false;
    }
}
//发送pkt并且打印pkt中header的信息
void snd_pkt(Packet sendp) {
    if(sendto(server, sendp.Buffer, sizeof(sendp.header)+sendp.header.datasize, 0, (sockaddr*)&server_addr, addrlen))//发送数据包
    //cout << "send " << sendp.header.datasize << " Byte：";
        sendp.header.show_header();
}
//把对应的Header和databuf写入pkt的buffer中，包括seq模256，计算好校验和等方法
void mk_pkt(Packet& sendp, bool END, char* databuf, int datatsize,int seq) {
    seq %= 256;
    sendp.header.seq = (u_char)seq;
    if (!END)
        sendp.header.flag = 0x0;
    else
        sendp.header.flag = 0x7;
    sendp.header.datasize = datatsize;
    memcpy(sendp.Buffer, &sendp.header, sizeof(sendp.header));
    memcpy(sendp.Buffer + sizeof(sendp.header), databuf, datatsize);
    u_short tempsum = cksum((u_short*)&sendp.Buffer, sizeof(sendp.Buffer));
    sendp.header.sum = tempsum;
    memcpy(sendp.Buffer, &sendp.header, sizeof(sendp.header));
}



DWORD WINAPI thread_recv(LPVOID lparam) {
    Header recvh;
    char* buf = new char[sizeof(recvh)];
   // recvparams* p = (recvparams*)lparam;
   // SOCKET clientsocket = (SOCKET)client_socket;
    int addrlen = sizeof(server_addr);
    while (true) {
        if (recvfrom(server, buf, MAXSIZE, 0, (sockaddr*)&server_addr, &addrlen)) {
            memcpy(&recvh, buf, sizeof(recvh));
            //其他的ack全都忽略，只接收base相等的ack,
            //即new ack
            
            if (recvh.ack >= base % 256 && (recvh.seq - base % 256 <= cwnd)) {
                //状态更新
                int temp = state_after_newack(curState);
                curState = temp;
                //确认了就是这个包，窗口挪动
                cout << "[ACK ] packet seq:" << (int)recvh.ack  << endl;
                //recvh.show_header();
                base += recvh.ack - base % 256 + 1;
                curAcked+= recvh.ack - base % 256 + 1;
                curAcked %= 256;
                if (base != nextseqnum)
                    QueryPerformanceCounter((LARGE_INTEGER*)&head);

            }
            else if (recvh.ack < cwnd && base % 256>255 - cwnd) {
                //状态更新
                int temp = state_after_newack(curState);
                curState = temp;
                //确认了就是这个包，窗口挪动
                cout << "[ACK ] packet seq:" << (int)recvh.ack << endl;
                base += recvh.seq + 256 - base % 256 + 1;
                curAcked += recvh.seq + 256 - base % 256 + 1;
                curAcked %= 256;
                if (base != nextseqnum)
                    QueryPerformanceCounter((LARGE_INTEGER*)&head);
            }
            /*快重传阶段：发送方只要一连接收到三个重复确认就应该立即重传对方尚未接收的报文段，
            而不必等到重传计时器超时后发送。
            由3个重复应答判断有包丢失，重新发送丢包的信息。*/
            else {//收到冗余ack
                if (curState == SLOW_START || curState == CON_AVOID) {
                    dupACKcount++;

                    if (dupACKcount == 3) {
                        curState = FAST_RE;
                        ssthreash = cwnd / 2;
                        cwnd = ssthreash + 3;
                        nextseqnum = base;//重传所有未确认的值
                        cout << "*************************dupACKcount = 3! resend from packet No.[" << base << "]*********************************" << endl;
                        Sleep(2);
                    }
                }
                else {
                    cwnd++;
                }
            }
            

        }

        if (time(head) > TIMEOUT) {
            int temp = state_after_timeout(curState);
            curState = temp;
            nextseqnum = base;
            cout << "*************************TIME OUT! resend from packet No.[" << base << "]*********************************" << endl;

        }
        if (base == num - 1) {
            break;
        }
    }
    return 0;
}

void send(SOCKET& clientsocket, SOCKADDR_IN& serveraddr, int& addrlen, char* data, int len) {


    if (len % MAXSIZE == 0) {
        num = len / MAXSIZE;
    }
    else {
        num = len / MAXSIZE + 1;
    }
    cout << "packets:" << num << endl;
    Packet sendp;//这个包的seq是nextseqnum，是当前要发的
    QueryPerformanceCounter((LARGE_INTEGER*)&head);
    if (base < num - 1)
        CreateThread(NULL, NULL, &thread_recv, NULL, 0, NULL);
    //Sleep(20);
    while (base < num-1) {//前整个maxsize的包都这么发最后一个单独发
        //初始时是慢启动阶段
        if (nextseqnum < base + cwnd ) {
            mk_pkt(sendp, nextseqnum == num - 1, data + nextseqnum * MAXSIZE, nextseqnum == num - 1 ? len - (num - 1) * MAXSIZE : MAXSIZE, nextseqnum % 256);
            cout << "[SEND][cwnd="<<(int)cwnd<<",ssthreash="<<(int)ssthreash<<"] packet No.[" << nextseqnum << "]: ";
            //printf("[SEND][cwnd=%d,ssthreash=%d] packet No.[%d]",cwnd,ssthreash,nextseqnum );
            snd_pkt(sendp);

            if (base == nextseqnum)
                QueryPerformanceCounter((LARGE_INTEGER*)&head); 
            if (curState != FAST_RE) {
            nextseqnum++;
            }

        }
        Sleep(2);

    }

    //发送最后一个包,采用3-1的普通方法，
    Header recvh;
    char* buf = new char[sizeof(recvh)];
    sendp.header.setHeader(len - (num - 1) * MAXSIZE, 0x7, (nextseqnum % 256));
    mk_pkt(sendp, true, data + (num - 1) * MAXSIZE, len - (num - 1) * MAXSIZE, nextseqnum);
    snd_pkt(sendp);
    cout << "last send: ";
    QueryPerformanceCounter((LARGE_INTEGER*)&head);
    while (true) {
        u_long mode = 1;
        ioctlsocket(clientsocket, FIONBIO, &mode);
        while (recvfrom(clientsocket, buf, sizeof(recvh), 0, (sockaddr*)&serveraddr, &addrlen) <= 0) {
            if (time(head) > TIMEOUT) {//超时，重新传输
                cout << "time out! resend:" << endl;
                sendp.header.setHeader(len - (num - 1) * MAXSIZE, 0x7, (nextseqnum % 256));
                mk_pkt(sendp, true, data + (num - 1) * MAXSIZE, len - (num - 1) * MAXSIZE, nextseqnum);
                snd_pkt(sendp);
                QueryPerformanceCounter((LARGE_INTEGER*)&head);
                continue;
            }
        }
        memcpy(&recvh, buf, sizeof(recvh));
        if (recvh.flag) {
            curAcked = recvh.ack;
            //recvh.ack==
            if (ckend(recvh)) {
                cout << "server has received！" << endl;
                break;
            }
            else if (cksend(recvh)) {
                cout << "send ok!" << endl;
                break;
            }
            else {
                continue;
            }
        }

    }
    u_long mode = 0;
    ioctlsocket(clientsocket, FIONBIO, &mode);//改回阻塞模式
    varReset();
    
}
void start(int len) {
    vector<string> fileNames;
    string path("D:\\111programs\\3-3client"); 	//自己选择目录测试

    vector<string> files;
    intptr_t hFile = 0;//文件句柄    
    struct _finddata_t fileinfo;//文件信息
    string p1, p2;
    if ((hFile = _findfirst(p1.assign(path).append("\\*").c_str(), &fileinfo)) != -1)
    {
        do
        {
            if (!(fileinfo.attrib & _A_SUBDIR))
                files.push_back(fileinfo.name);

        } while (_findnext(hFile, &fileinfo) == 0);
        _findclose(hFile);
    }
    int k = 1;
    cout << "***************************files list********************************" << endl;
    for (auto f : files) {
        cout << k << ". ";
        cout << f << endl;
        k++;
    }
    int x;
    cout << "enter the number of the file" << endl;
    cin >> x;
    if (x) {
        string myfile = files[x - 1];
        cout << "starting：" << files[x - 1] << endl;
        ifstream fin(myfile.c_str(), ifstream::binary);//以二进制方式打开文件
        char* buffer = new char[100000000];
        int i = 0;
        u_short temp = fin.get();
        while (fin)
        {
            buffer[i++] = temp;
            temp = fin.get();
        }
        fin.close();
        long long start, end, freq;
        send(server, server_addr, len, (char*)(myfile.c_str()), myfile.length());
        QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
        QueryPerformanceCounter((LARGE_INTEGER*)&start);
        send(server, server_addr, len, buffer, i);
        QueryPerformanceCounter((LARGE_INTEGER*)&end);
        cout << "传输总时间为: " << (end - start) * 1.0 / freq << " s" << endl;
        cout << "吞吐率为: " << ((double)i) / ((end - start) * 1.0 / freq) << " byte/s" << endl;

    }
}

#endif
