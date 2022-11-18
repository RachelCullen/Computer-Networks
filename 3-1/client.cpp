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
/*/*标志位为： |  2  |  1  |  0  |
*          ---------------------
*             | SYN | FIN | ACK |
*/


const int MAXSIZE = 2048;//传输缓冲区最大长度
const u_short SYN_1_ACK_0 = 0x4; //SYN = 1 ACK = 0
const u_short SYN_1_ACK_1 = 0x5;//SYN = 0, ACK = 1
const u_short SYN_0_ACK_1 = 0x1;
const u_short FIN_1_ACK_1 = 0x3;//SYN = 1, ACK = 1
const u_short FIN_1_ACK_0 = 0x2;//FIN = 1 ACK = 0
const u_short END = 0x7;//结束标志，SYN=1,FIN=1,ACK=1
double MAX_WAIT_TIME = 0.5* CLOCKS_PER_SEC;
void printsplit() {
    cout << "--------------------------------------------------------------------------" << endl;
}
u_short cksum(u_short* mes, int size) {
    int count = (size + 1) / 2;
    u_short* buf = (u_short*)malloc(size + 1);
    memset(buf, 0, size + 1); //fill with 0
    memcpy(buf, mes, size);
    u_long sum = 0;
    while (count--) {
        sum += *buf++;
        if (sum & 0xffff0000) { 
            sum &= 0xffff;
            sum++;
        }
    }
    return ~(sum & 0xffff);
}

struct HEADER{
    u_short datasize = 0;//所包含数据长度 16位
    u_short sum = 0;//校验和 16位
    u_short flag = 0;//标志位 16位
    u_short SEQ = 0; //序列号 16位
    HEADER() {
        sum = 0;
        datasize = 0;
        flag = 0;
        SEQ = 0;
    }
};

bool Connect(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen){//两次握手建立连接
  
    HEADER header;
    char* buff = new char[sizeof(header)];
    u_short sum;
    //进行第一次握手
    header.flag = SYN_1_ACK_0;
    header.sum = 0;//校验和置0
    header.sum = cksum((u_short*)&header, sizeof(header));
    memcpy(buff, &header, sizeof(header));//将首部放入缓冲区
    if (sendto(socketClient, buff, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen) == -1){
        return false;
    }
    clock_t start = clock();
    u_long mode = 1;
    ioctlsocket(socketClient, FIONBIO, &mode);//防止线程阻塞

    //接收第二次握手
    while (recvfrom(socketClient, buff, sizeof(header), 0, (sockaddr*)&servAddr, &servAddrlen) <= 0){
        
        if (clock()-start > MAX_WAIT_TIME){//超时，重新传输第一次握手
            header.flag = SYN_1_ACK_0;
            header.sum = 0;//校验和置0
            header.sum = cksum((u_short*)&header, sizeof(header));//计算校验和
            memcpy(buff, &header, sizeof(header));//将首部放入缓冲区
            sendto(socketClient, buff, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);
            start = clock();
            cout << "time out for first hello. resending....." << endl;
        }
    }


    //进行校验和检验
    memcpy(&header, buff, sizeof(header));
    if (header.flag == SYN_1_ACK_1 && cksum((u_short*)&header, sizeof(header) == 0)){
        cout << "second hello----check\nconnection succeeded" << endl;
    }
    else{
        cout << "error" << endl;
        return false;
    }
    return true;
    
}




void sendMessage(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen, char* msg, int lenlen)
{

    int packagenum = lenlen / MAXSIZE + (lenlen % MAXSIZE != 0); //切分数据包，2048B一个数据包
    int seq = 0;
    for (int i = 0; i < packagenum; i++){
        char* message = msg + i * MAXSIZE;
        int len = (i == packagenum - 1 ? lenlen - (packagenum - 1) * MAXSIZE : MAXSIZE); //长度是2048还是最后一个包的余数
        HEADER header;
        char* buff = new char[MAXSIZE + sizeof(header)];
        header.datasize = len;
        header.SEQ = u_short(seq);//序列号
        memcpy(buff, &header, sizeof(header)); //首部放进缓冲区
        memcpy(buff + sizeof(header), message, sizeof(header) + len);//数据放进缓冲区
        header.sum = cksum((u_short*)buff, sizeof(header) + len);//计算校验和
        memcpy(buff, &header, sizeof(header));
        sendto(socketClient, buff, len + sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);//发送
        printsplit();
        cout << "Send message " << len << " B!" << " flag:" << int(header.flag) << " SEQ:" << int(header.SEQ) << " SUM:" << int(header.sum) << endl;
        clock_t start = clock();
        //接收ack等信息
        while (1){
            u_long mode = 1;
            ioctlsocket(socketClient, FIONBIO, &mode);
            while (recvfrom(socketClient, buff, MAXSIZE, 0, (sockaddr*)&servAddr, &servAddrlen) <= 0){               
                if (clock()-start > MAX_WAIT_TIME){
                    header.datasize = len;
                    header.SEQ = u_char(seq);//序列号
                    header.flag = u_char(0x0);
                    memcpy(buff, &header, sizeof(header));
                    memcpy(buff + sizeof(header), message, sizeof(header) + len);
                    header.sum = cksum((u_short*)buff, sizeof(header) + len);//计算校验和
                    memcpy(buff, &header, sizeof(header));
                    sendto(socketClient, buff, len + sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);//发送
                    printsplit();
                    cout << "TIME OUT! ReSend message " << len << " bytes! Flag:" << int(header.flag) << " SEQ:" << int(header.SEQ) << endl;
                    clock_t start = clock();
                }
            }
            memcpy(&header, buff, sizeof(header));//缓冲区接收到信息，读取
            u_short check = cksum((u_short*)&header, sizeof(header));
            if (header.SEQ == u_short(seq) && header.flag == SYN_0_ACK_1)
            {
                printsplit();
                cout << "Send has been confirmed! Flag:" << int(header.flag) << " SEQ:" << int(header.SEQ) << endl;
                break;
            }
            else
            {
                continue;
            }
        }
        u_long mode = 0;
        ioctlsocket(socketClient, FIONBIO, &mode);//改回阻塞模式       
        seq++;
        
    }
    //发送结束信息
    HEADER header;
    char* Buffer = new char[sizeof(header)];
    header.flag = END;
    header.sum = 0;
    header.sum = cksum((u_short*)&header, sizeof(header));
    memcpy(Buffer, &header, sizeof(header));
    sendto(socketClient, Buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);
    cout << "Send End!" << endl;
    clock_t start = clock();
    while (1){
        u_long mode = 1;
        ioctlsocket(socketClient, FIONBIO, &mode);
        while (recvfrom(socketClient, Buffer, MAXSIZE, 0, (sockaddr*)&servAddr, &servAddrlen) <= 0)
        {
           
            if (clock()-start > MAX_WAIT_TIME)
            {
                char* Buffer = new char[sizeof(header)];
                header.flag = END;
                header.sum = 0;
                header.sum = cksum((u_short*)&header, sizeof(header));
                memcpy(Buffer, &header, sizeof(header));
                sendto(socketClient, Buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);
                printsplit();
                cout << "Time Out! ReSend End!" << endl;
                start = clock();
            }
        }
        memcpy(&header, Buffer, sizeof(header));//缓冲区接收到信息，读取
        u_short check = cksum((u_short*)&header, sizeof(header));
        if (header.flag == END){
            cout << "server has received the file!" << endl;
            break;
        }
        else{
            continue;
        }
    }
    u_long mode = 0;
    ioctlsocket(socketClient, FIONBIO, &mode);//改回阻塞模式
}



bool Disconnect(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen){
 
    HEADER header;
    char* Buffer = new char[sizeof(header)];

    u_short sum;

    //进行第一次挥手
    header.flag = FIN_1_ACK_0;
    header.sum = 0;//校验和置0
    header.sum = cksum((u_short*)&header, sizeof(header));
    memcpy(Buffer, &header, sizeof(header));//将首部放入缓冲区
    if (sendto(socketClient, Buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen) == -1){
        return false;
    }
    clock_t start = clock();
    u_long mode = 1;
    ioctlsocket(socketClient, FIONBIO, &mode);

    //接收第二次挥手
    while (recvfrom(socketClient, Buffer, sizeof(header), 0, (sockaddr*)&servAddr, &servAddrlen) <= 0){
        if (clock()-start > MAX_WAIT_TIME){//超时，重新传输第一次挥手
            header.flag = FIN_1_ACK_1;
            header.sum = 0;//校验和置0
            header.sum = cksum((u_short*)&header, sizeof(header));//计算校验和
            memcpy(Buffer, &header, sizeof(header));//将首部放入缓冲区
            sendto(socketClient, Buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);
            start = clock();
            cout << "time out for first goodbye. resending....." << endl;
        }
    }


    //进行校验和检验
    memcpy(&header, Buffer, sizeof(header));
    if (header.flag == FIN_1_ACK_1 && cksum((u_short*)&header, sizeof(header) == 0))
    {
        cout << "second goodbye----check" << endl;
    }
    else
    {
        cout << "error" << endl;
        return false;
    }

    
    return true;
}


int main(){
    WSADATA wsadata;
    WSAStartup(MAKEWORD(2, 2), &wsadata);

    SOCKADDR_IN server_addr;
    SOCKET server;

    server_addr.sin_family = AF_INET;//使用IPV4
    server_addr.sin_port = htons(4001);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    server = socket(AF_INET, SOCK_DGRAM, 0);
    int len = sizeof(server_addr);
    //建立连接
    if (!Connect(server, server_addr, len) )
    {
        return 0;
    }
    vector<string> fileNames;
    string path("D:\\111programs\\计网lab3-1_client"); 	//自己选择目录测试
    
    vector<string> files;
    intptr_t hFile = 0;//文件句柄    
    struct _finddata_t fileinfo;//文件信息
    string p1,p2;
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
    printsplit();
    cout << "files list：" << endl;
    for (auto f : files) {
        cout << k << ". ";
        cout << f << endl;
        k++;
    }   
    int x = 0;   
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
        long long head, tail, freq;
        sendMessage(server, server_addr, len, (char*)(myfile.c_str()), myfile.length());
        QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
        QueryPerformanceCounter((LARGE_INTEGER*)&head);
        sendMessage(server, server_addr, len, buffer, i);
        QueryPerformanceCounter((LARGE_INTEGER*)&tail);
        cout << "传输总时间为: " << (tail - head)*1.0  / freq << " s" << endl;
        cout << "吞吐率为: " << ((double)i) / ((tail - head)*1.0 / freq) << " byte/s" << endl;

    }      
    Disconnect(server, server_addr, len);
}
