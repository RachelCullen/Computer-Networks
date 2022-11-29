#include "myserver.h"
using namespace std;

int main() {
    init();
    
    //建立连接
    interact(server, server_addr, addrlen,"hello");
    start(addrlen);
    _sleep(5 * 1000);
    interact(server, server_addr, addrlen,"goodbye");
    
  
}