#include "myclient.h"
using namespace std;
int main() {
    init();
    int len = sizeof(server_addr);
    /*建立连接*/
    if (!interact(server, server_addr, len, "hello"))//握手失败则退出
    {
        return 0;
    }
    start(len);//开始发送文件
    _sleep(5 * 1000);
    interact(server, server_addr, len, "goodbye");//挥手退出
}



