#include <iostream>
#include <WINSOCK2.h>
#include <time.h>
#include <fstream>
#pragma comment(lib, "ws2_32.lib")
using namespace std;


const int MAXSIZE = 2048;//传输缓冲区最大长度
const u_short SYN_1_ACK_0 = 0x4; //SYN = 1 ACK = 0
const u_short SYN_1_ACK_1 = 0x5;//SYN = 0, ACK = 1
const u_short SYN_0_ACK_1 = 0x1; //give back ack=1 when received 
const u_short FIN_1_ACK_1 = 0x3;//SYN = 1, ACK = 1
const u_short FIN_1_ACK_0 = 0x2;//FIN = 1 ACK = 0
const u_short END = 0x7;//结束标志，SYN=1,FIN=1,ACK=1
double MAX_WAIT_TIME = 0.5 * CLOCKS_PER_SEC;
long long head, tail, freq;


void printsplit() {
    cout << "--------------------------------------------------------------------------" << endl;
}
u_short cksum(u_short* mes, int size) {
    int count = (size + 1) / 2;
    u_short* buf = (u_short*)malloc(size + 1);
    memset(buf, 0, size + 1);
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
    u_short datasize = 0;
    u_short sum = 0;
    u_short flag = 0;
    u_short SEQ = 0;

    HEADER() {
        sum = 0;
        datasize = 0;
        flag = 0;
  
        SEQ = 0;
    }
};

bool Connect(SOCKET& sockServ, SOCKADDR_IN& ClientAddr, int& ClientAddrLen)
{

    HEADER header;
    char* Buffer = new char[sizeof(header)];

    //接收第一次握手信息
    while (1){
        if (recvfrom(sockServ, Buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, &ClientAddrLen) == -1){
            return false;
        }
        memcpy(&header, Buffer, sizeof(header));
        if (header.flag == SYN_1_ACK_0 && cksum((u_short*)&header, sizeof(header)) == 0){
            cout << "first hello----check" << endl;
            break;
        }
    }

    //发送第二次握手信息
    header.flag = SYN_1_ACK_1;
    header.sum = 0;
    header.sum = cksum((u_short*)&header, sizeof(header));
    memcpy(Buffer, &header, sizeof(header));
    if (sendto(sockServ, Buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen) == -1){
        return false;
    }
    
    

    HEADER h;
    memcpy(&h, Buffer, sizeof(header));
    if (h.flag == SYN_1_ACK_1 && cksum((u_short*)&h, sizeof(h) == 0)){
        cout << "connection succeeded" << endl;
    }
    else{
        cout << "error" << endl;
        return false;
    }
    return true;
}

int RecvMessage(SOCKET& sockServ, SOCKADDR_IN& ClientAddr, int& ClientAddrLen, char* message){
    long int total_len = 0;//file length
    HEADER header;
    char* buff = new char[MAXSIZE + sizeof(header)];
    int seq = 0;
    int index = 0;

    while (1){
        int length = recvfrom(sockServ, buff, sizeof(header) + MAXSIZE, 0, (sockaddr*)&ClientAddr, &ClientAddrLen);//接收报文长度
        //cout << length << endl;
        memcpy(&header, buff, sizeof(header));
        //to see if is done
        if (header.flag == END && cksum((u_short*)&header, sizeof(header)) == 0){
            cout << "done" << endl;
            break;
        }
        if (header.flag == u_short(0) && cksum((u_short*)buff, length - sizeof(header))){
            //to see if other data is recieved
            if (seq != int(header.SEQ)){
                //problems occurred. send back ACK=0
                header.flag = 0;
                header.datasize = 0;
                header.SEQ = (u_short)seq;
                header.sum = 0;
                header.sum = cksum((u_short*)&header, sizeof(header));
                memcpy(buff, &header, sizeof(header));                
                sendto(sockServ, buff, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen);
                printsplit();
                cout << "Send to Client ACK:" << (int)header.flag << " SEQ:" << (int)header.SEQ << endl;
                continue;//dump the data
            }
            seq = int(header.SEQ);
            
            printsplit();
            cout << "Send message " << length - sizeof(header) << " B!Flag:" << int(header.flag) << " SEQ : " << int(header.SEQ) << " SUM:" << int(header.sum) << endl;
            char* temp = new char[length - sizeof(header)];
            memcpy(temp, buff + sizeof(header), length - sizeof(header));

            memcpy(message + total_len, temp, length - sizeof(header));
            total_len = total_len + int(header.datasize);

            //return ACK
            header.flag = SYN_0_ACK_1;
            header.datasize = 0;
            header.SEQ = (u_short)seq;
            header.sum = 0;
            header.sum = cksum((u_short*)&header, sizeof(header));
            memcpy(buff, &header, sizeof(header));
            //resend ACK
            sendto(sockServ, buff, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen);
            printsplit();
            cout << "Send to Client ACK:" << (int)header.flag << " SEQ:" << (int)header.SEQ << endl;
            seq++;
            
        }
    }
    //发送结束信息
    header.flag = END;
    header.sum = 0;
    header.sum = cksum((u_short*)&header, sizeof(header));
    memcpy(buff, &header, sizeof(header));
    if (sendto(sockServ, buff, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen) == -1){
        return -1;
    }
    return total_len;
}

bool disConnect(SOCKET& sockServ, SOCKADDR_IN& ClientAddr, int& ClientAddrLen){
    HEADER header;
    char* Buffer = new char[sizeof(header)];
    while (1 == 1){
        int length = recvfrom(sockServ, Buffer, sizeof(header) + MAXSIZE, 0, (sockaddr*)&ClientAddr, &ClientAddrLen);//接收报文长度
        memcpy(&header, Buffer, sizeof(header));
        if (header.flag == FIN_1_ACK_0 && cksum((u_short*)&header, sizeof(header)) == 0)
        {
            cout << "first goodbye----check" << endl;
            break;
        }
    }
    //发送第二次挥手信息
    header.flag = FIN_1_ACK_1;
    header.sum = 0;
    header.sum = cksum((u_short*)&header, sizeof(header));
    memcpy(Buffer, &header, sizeof(header));
    if (sendto(sockServ, Buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen) == -1){
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
    bind(server, (SOCKADDR*)&server_addr, sizeof(server_addr));//绑定套接字，进入监听状态
    cout << "waiting client....." << endl;
    int len = sizeof(server_addr);
    //建立连接
    Connect(server, server_addr, len);
    char* name = new char[20];
    char* data = new char[100000000];
    int namelen = RecvMessage(server, server_addr, len, name);
    int datalen = RecvMessage(server, server_addr, len, data);
    string a;
    for (int i = 0; i < namelen; i++){
        a = a + name[i];
    }
    disConnect(server, server_addr, len);
    ofstream fout(a.c_str(), ofstream::binary);
    for (int i = 0; i < datalen; i++){
        fout << data[i];
    }
    fout.close();
    cout << "the file has been downloaded." << endl;
}