#include "myclient.h"
using namespace std;
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
        cout << "first "<<type<<"----failed" << endl;
        return false;
    }
    else
        cout << "first "<<type<<" ----sended" << endl;
    long long head;
    QueryPerformanceCounter((LARGE_INTEGER*)&head);
    u_long mode = 1;
    ioctlsocket(server, FIONBIO, &mode);
    while (recvfrom(sockServ, Buffer, sizeof(recvh), 0, (sockaddr*)&ClientAddr, &ClientAddrLen) <= 0) {
        if (time(head) > TIMEOUT) {
            memcpy(Buffer, &sendh, sizeof(sendh));
            sendto(sockServ, Buffer, sizeof(recvh), 0, (sockaddr*)&ClientAddr, ClientAddrLen);
            QueryPerformanceCounter((LARGE_INTEGER*)&head);
            cout << "time out for first "<<type<<".resending......" << endl;
            continue;
        }
    }
    memcpy(&recvh, Buffer, sizeof(recvh));
    if (check_sign(recvh,recvsign)) {

        cout << "second "<<type<<" ----checked" << endl;
        cout << "----------------------------ok---------------------------------" << endl;
        return true;
    }
    else {
        cout << "----------------------------failed---------------------------------" << endl;
        return false;
    }
}

void packet_send(SOCKET& clientsocket, SOCKADDR_IN& serveraddr, int& addrlen, char* message, int len, bool END) {
    Header recvh;
    Packet sendp;
    char* buf = new char[sizeof(recvh)];
    sendp.header.seq = SEQ;
    if (!END)
        sendp.header.flag = 0x0;
    else
        sendp.header.flag = 0x7;
    sendp.header.datasize = len;
    memcpy(sendp.Buffer, &sendp.header, sizeof(sendp.header));
    memcpy(sendp.Buffer + sizeof(sendp.header), message, sizeof(sendp.header) + len);
    u_short tempsum = cksum((u_short*)&sendp.Buffer, sizeof(sendp.Buffer));
    sendp.header.sum = tempsum;
    memcpy(sendp.Buffer, &sendp.header, sizeof(sendp.header));
    sendto(clientsocket, sendp.Buffer, sizeof(sendp.header) + len, 0, (sockaddr*)&serveraddr, addrlen);//发送数据包
    cout << "send " << len << " Byte：";
    sendp.header.show_header();
    long long head;
    QueryPerformanceCounter((LARGE_INTEGER*)&head);
    //clock_t start = clock(); //记录发送时间
    while (true) {
        u_long mode = 1;
        ioctlsocket(clientsocket, FIONBIO, &mode);
        while (recvfrom(clientsocket, buf, sizeof(recvh), 0, (sockaddr*)&serveraddr, &addrlen) <= 0) {
            if (time(head) > TIMEOUT){//超时，重新传输
                sendp.header.setHeader(len, 0x0, SEQ);
                memcpy(sendp.Buffer, &sendp.header, sizeof(sendp.header));
                memcpy(sendp.Buffer + sizeof(sendp.header), message, sizeof(sendp.header) + len);
                u_short tempsum = cksum((u_short*)&sendp, sizeof(sendp.Buffer));
                sendp.header.sum = tempsum;
                memcpy(sendp.Buffer, &sendp.header, sizeof(sendp.header));
                sendto(clientsocket, sendp.Buffer, sizeof(sendp.header) + len, 0, (sockaddr*)&serveraddr, addrlen);
                cout << "time out! resend:";
                sendp.header.show_header();
                //<< len << "个字节（SEQ为：" << SEQ << "  校验和为：" << sendp.header.checksum << ")" << endl;
                QueryPerformanceCounter((LARGE_INTEGER*)&head);
                continue;
            }
        }
        memcpy(&recvh, buf, sizeof(recvh));
        if (recvh.flag) {
            ACK = recvh.ack;
            if (ckack() && ckend(recvh)) {
                cout << "server has received！" << endl;
                break;
            }
            else if (ckack && cksend(recvh)) {
                //cout << "ACK:" << ACK << endl;
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
}
void send(SOCKET& clientsocket, SOCKADDR_IN& serveraddr, int& addrlen, char* message, int len) {
    //数据缓冲区最大为2048字节，将数据分包进行发送
    int num = 0;
    if (len % MAXSIZE == 0) {
        num = len / MAXSIZE;
    }
    else {
        num = len / MAXSIZE + 1;
    }
    cout << "packets:" << num << endl;
    SEQ = 0;//将序列号请0
    for (int i = 0; i < num; i++) {
        cout << "send No.[" << i + 1 << "]packet" << endl;
        if (i != num - 1) {
            int templen = MAXSIZE;
            packet_send(clientsocket, serveraddr, addrlen, message + i * MAXSIZE, templen, false);
        }
        else {
            int templen = len - (num - 1) * MAXSIZE;
            packet_send(clientsocket, serveraddr, addrlen, message + i * MAXSIZE, templen, true);//最后一个数据包
        }
        SEQ++;

    }
}



int main() {
    init();
    int len = sizeof(server_addr);
    //建立连接
    if (!interact(server, server_addr, len,"hello"))
    {
        return 0;
    }
    vector<string> fileNames;
    string path("D:\\111programs\\3-2client"); 	//自己选择目录测试

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
        send(server, server_addr, len, (char*)(myfile.c_str()), myfile.length());
        QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
        QueryPerformanceCounter((LARGE_INTEGER*)&head);
        send(server, server_addr, len, buffer, i);
        QueryPerformanceCounter((LARGE_INTEGER*)&tail);
        cout << "传输总时间为: " << (tail - head) * 1.0 / freq << " s" << endl;
        cout << "吞吐率为: " << ((double)i) / ((tail - head) * 1.0 / freq) << " byte/s" << endl;

    }
    interact(server, server_addr, len,"goodbye");
}
