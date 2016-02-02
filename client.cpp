
#include <iostream>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

using namespace std;

int main(/*int argc, char* argv[]*/)
{
    int sock;
    int port  = 8088;
    struct sockaddr_in server;
    char message[1000], reply[2000];
    sock = socket (PF_INET, SOCK_STREAM, 0);
    if(sock == -1){
        cerr<"sock creation failed\n";
        return 1;
    }
    cerr<<"socket created";
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_family = PF_INET;

    server.sin_port = htons(port);
    if (connect(sock, (struct sockaddr*)&server), sizeof(server) <0){
        cerr<"client connection failed \n";
        return 1;
    }
    cerr<<"connection made \n";
    while(1){
        if (send (sock, message, strlen(message), 0) < 0){
            cerr<< "sending failed\n";
            return 1;
        }
    }
}
