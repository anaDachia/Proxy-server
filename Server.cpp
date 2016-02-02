#include <iostream>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <netdb.h>
#include <sys/select.h>
#include <sstream>
#include <mutex>
#include <string>

#define BUF_SIZE 2000

/////////////////////////////
// close all the sockets!!!

/////////////////////////////
using namespace std;

mutex write_lock;
mutex socket_lock;


void* client_hndlr(void*);

int send_sock(int sock, const char *buffer, uint32_t size) {
    int index = 0, ret;
    while(size) {
        if((ret = send(sock, &buffer[index], size, 0)) <= 0)
            return (!ret) ? index : -1;
        index += ret;
        size -= ret;
    }
    return index;
}
/*
int recv_sock(int sock, char *buffer) {
	int index = 0, ret;
	while(index < 8) {
		if((ret = recv(sock, &buffer[index], BUF_SIZE, 0)) <= 0)
			return (!ret) ? index : -1;
		index += ret;
	}
	
	while (true){
		if((ret = recv(sock, &buffer[index], BUF_SIZE, 0)) <= 0)
			return (!ret) ? index : -1;
		index += ret;
		for (int i = 8; i < index; i++){
			if (buffer[i] == '\0' || buffer[i] == 0)
				return (!ret) ? index : -1;
		}
	}
	return index;
}*/
int recv_sock(int sock, char *buffer, uint32_t size) {
	int index = 0, ret;
	while(size) {
		if((ret = recv(sock, &buffer[index], size, 0)) <= 0)
			return (!ret) ? index : -1;
		if (index >= 8){
			for (int i = 8; i < index; i++){
				if (buffer[i] == '\0' || buffer[i] == 0)
					return (!ret) ? index : -1;
			}
		}
		index += ret;
		size -= ret;
	}
	return index;
}

int check_4a(u_int32_t ip){
    ostringstream oss;
    int sum = 0;
    for (unsigned i=0; i<4; i++) {
         oss<<((ip >> (i*8) ) & 0xFF);
         int a = atoi(oss.str().c_str());

         if (i == 3 && sum == 0)
            return 1;
			
        else if(i == 3 && sum != 0)
            return 0;
         sum = sum + a;
         //cerr<< "sum: " << sum << "  a: " << a <<endl;

         oss.str("");
    }
    return 0;
}
string int_to_str(uint32_t ip) {
    ostringstream oss;
    for (unsigned i=0; i<4; i++) {
        oss << ((ip >> (i*8) ) & 0xFF);
        if(i != 3)
            oss << '.';
    }
    return oss.str();
}
string int_to_str_port(uint16_t ip) {
    ostringstream oss;
    for (unsigned i=0; i<2; i++) {
        oss << ((ip >> (i*8) ) & 0xFF);
    }
    return oss.str();
}

struct DataIn {
    char VN;
    char CD;
    u_int16_t DST_PORT;
    u_int32_t DST_IP;
    char ID[1];
    char name[500];


}__attribute__((packed));

struct Response {
    char NL;
    char STAT;
    u_int16_t DST_PORT;
    u_int32_t DST_IP;

}__attribute__((packed));

struct handler_inp {
    int port;
    sockaddr_in client;
    int* sock;
};

int main(int argc, char* argv[])
{
    int port = atoi(argv[1]);//8080;
    int client_sock;
    int socket_desc = socket(PF_INET, SOCK_STREAM, 0);
    if (socket_desc == -1){
        cerr<<"socket creation failed\n";
        return 1;
    }

    /////////////////////////////
    struct sockaddr_in addr_in;
    memset(&addr_in, 0, sizeof(sockaddr_in)) ;
    addr_in.sin_family = PF_INET;
    addr_in.sin_port = htons(port);
    /////////////////////////////

    if (bind(socket_desc,(struct sockaddr *) &addr_in, sizeof(addr_in)) < 0){
        cerr<<"binding failed\n";
        return 1;
    }


    if (listen(socket_desc, 6)){
        cerr <<"listen failed\n";
    }


    struct sockaddr_in client_adr_in;
    int c = sizeof(struct sockaddr_in);
     while ( (client_sock = accept(socket_desc, (struct sockaddr*) &client_adr_in, (socklen_t*)&c))){

        pthread_t sniff_th;
        int* cd = new int(client_sock);
        /// COUT #1/////////////////////////////////////////////////////////////////////////////////////////
        cout << "A client from " << int_to_str(client_adr_in.sin_addr.s_addr) << ":" << port << " was connected"<<endl;

        struct handler_inp* hndl_inp = new handler_inp();
        hndl_inp->port = port;
        hndl_inp->client = client_adr_in;
        hndl_inp->sock = cd;


        if (pthread_create(&sniff_th, NULL ,client_hndlr, (void*)hndl_inp ) < 0){
            cerr<<"thread creation failed\n";
            return 1;
        }

    }
     if (client_sock < 0){
         cerr<< "acception failed \n";
         return 1;
    }
    return 0;
}

void set_fds(int sock1, int sock2, fd_set *fds) {
FD_ZERO (fds);
FD_SET (sock1, fds);
FD_SET (sock2, fds);
}

void do_proxy(int client, int conn, char*buffer, string client_ip, int client_port, string server_ip, string server_port){

    fd_set readfds;
    int result, nfds = max(client,conn) + 1;
    set_fds(client, conn, &readfds);
    while((result = select(nfds, &readfds, 0, 0, 0)) > 0) {
        if (FD_ISSET (client, &readfds)) {

            int recvd = recv(client, buffer, BUF_SIZE, 0);
            if(recvd <= 0){
				socket_lock.lock();
                close(client);
                socket_lock.unlock();
                /////////////////////             
                write_lock.lock();
                cout<< "The client from " << client_ip << ":" << client_port << " was disconnected"<<endl;
                write_lock.unlock();
                
                
                socket_lock.lock();
                close(conn);
                socket_lock.unlock();
                ////////////////////
                write_lock.lock();
                cout << "The connection with " << server_ip <<":" << server_port << " was lost" << endl;
                write_lock.unlock();
                pthread_exit(NULL);
               
                pthread_exit(NULL);
            }

            send_sock(conn, buffer, recvd);
        }
        if (FD_ISSET (conn, &readfds)) {

            int recvd = recv(conn, buffer, BUF_SIZE, 0);
            if(recvd <= 0){
				socket_lock.lock();
                close(conn);
                socket_lock.unlock();
               ///////////////////////// 
                write_lock.lock();
                cout << "The connection with " << server_ip <<":" << server_port << " was lost" << endl;
                write_lock.unlock();
                
                
                socket_lock.lock();
                close(client);
                socket_lock.unlock();
                /////////////////////             
                write_lock.lock();
                cout<< "The client from " << client_ip << ":" << client_port << " was disconnected"<<endl;
                write_lock.unlock();
               
               
                pthread_exit(NULL);
                
                
            }

            send_sock(client, buffer, recvd);
        }
        set_fds(client, conn, &readfds);
    }
}




void* client_hndlr(void*inp_data){
    handler_inp d = *(handler_inp*)inp_data;
    int sock = *(d.sock);
    int port = d.port;
    sockaddr_in client_conf = d.client;

    string client_ip  = int_to_str(client_conf.sin_addr.s_addr);;
    int client_port = port;
    string server_ip= "";
    string server_port = "";

    DataIn* read_data = new DataIn();
    int read_size;


    while((read_size = recv(sock, (char*)read_data, 100, 0)) > 0){
		
      ///1) converting data and authentication///////////////////////////
      int read_VN = (int) ((*read_data).VN);
      int read_CD = (int)((*read_data).CD);
      u_int16_t read_PORT = (*read_data).DST_PORT;
      u_int32_t read_IP = (*read_data).DST_IP;
      char* read_ID= (*read_data).ID;
      ////////////
      const char* ip_str = int_to_str(read_IP).c_str();
      const char* port_str = int_to_str_port(read_PORT).c_str();

     
      ////////////authen//////////
 /// tODO close the connection /////////////////////////////////////////////////////////
 /// TODO write a func for close to print messages/////////////////////////////////////
 /// TODO close sock finish threads by output of read write!!
 /// TODO mutex for close and print!!

      if (read_VN != 4 || read_CD != 1){
		  write_lock.lock();
            cout<< "The client from " << client_ip << ":" << client_port << " was disconnected"<<endl;
            write_lock.unlock();
            
			socket_lock.lock();
			close(sock);
			socket_lock.unlock();
			
			pthread_exit(NULL);
       }


      ///////////////////////////////////getaddrinfo config set//////////////////////////////////
      int status;
      struct addrinfo host_info;
      struct addrinfo* host_info_list;
      memset(&host_info, 0,sizeof (host_info));
      host_info.ai_family = PF_INET;
      host_info.ai_socktype = SOCK_STREAM;

      ///////////////////////////////////getaddrinfo and connection ////////////////////////////////
          //new_socket = socket(host_info_list->ai_family,host_info_list->ai_socktype, host_info_list->ai_protocol);
          //status = connect (new_socket, host_info_list->ai_addr, host_info_list->ai_addrlen);
          //cout<<"status in getaddr connection " << status << endl;
		

     int new_socket = -1;
     int is_string = check_4a (read_IP);
	
     if (is_string == 1){
		 
		 //string final_name = "";
		 char* final_name= (*read_data).name;
		  
		 
		  write_lock.lock();
		  cout << "the client from " << int_to_str(client_conf.sin_addr.s_addr) << " : " << port << " requested for " << final_name << ":" << int_to_str_port(read_PORT)<< endl;
         write_lock.unlock();
         
          status = getaddrinfo(final_name, /*(int_to_str(read_PORT)).c_str()*/"80" , &host_info, &host_info_list);
          //status = getaddrinfo("www.google.com", "80", &host_info, &host_info_list);

          
          if (status != 0){
            std::cerr << "getaddrinfo error " << gai_strerror(status)<<endl ;
            pthread_exit(NULL);
		   }

          //for loop on host_info_list and connect to each one!
          struct addrinfo* ptr = host_info_list;

          for (ptr = host_info_list; ptr; ptr = ptr->ai_next){
              new_socket = socket(ptr->ai_family,ptr->ai_socktype, ptr->ai_protocol);
              
              if(new_socket < 0 )
                 continue;

              status = connect (new_socket, ptr->ai_addr, ptr->ai_addrlen);
              
              if (status < 0){
                  close(new_socket);
                  new_socket = -1;
                  continue;
              }
              
              break;
          }

          freeaddrinfo(host_info_list);
          
          if (ptr == NULL){
			  write_lock.lock();
              cout<< "Could not connect to " << final_name <<":"<<int_to_str_port(read_PORT)<< endl;
              write_lock.unlock();
              
              Response* resp = new Response();
				resp->STAT = 0x5b;
				send_sock(sock, (char*)resp, sizeof(resp));
              pthread_exit(NULL);
          }
          
          struct sockaddr_in *addr;
          addr = (struct sockaddr_in *)ptr->ai_addr; 
          server_ip = string(inet_ntoa((struct in_addr)addr->sin_addr));
          
		   write_lock.lock();
          cout<< "Connection established with " << string(inet_ntoa((struct in_addr)addr->sin_addr)) <<":"<<int_to_str_port(read_PORT)<< endl;
          write_lock.unlock();
      }
      else{
		
          struct sockaddr_in server;
          memset(&server, 0,sizeof (server));
          server.sin_addr.s_addr = read_IP;
          server.sin_family = PF_INET;
          server.sin_port = read_PORT;
          new_socket = socket(PF_INET, SOCK_STREAM, 0);
          /// PRINT #2 ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
          write_lock.lock();
          cout << "the client from " << int_to_str(client_conf.sin_addr.s_addr) << " : " << port << " requested for " << int_to_str(read_IP) << ":" << int_to_str_port(read_PORT)<< endl;
          write_lock.unlock();
          
          status  = connect(new_socket, (struct sockaddr*)&server, sizeof(server));
          if (status == 0){
            write_lock.lock();
            cout << "Connection established with " << int_to_str(read_IP) << ":" << int_to_str_port(read_PORT) <<endl;
            write_lock.unlock();
            server_ip = int_to_str(read_IP);
            }

          else {
            write_lock.lock();
            cout <<"Could not connect to " << int_to_str(read_IP) << ":" << int_to_str_port(read_PORT) << endl;
            write_lock.unlock();
            
            Response* resp = new Response();
			resp->STAT = 0x5b;
			send_sock(sock, (char*)resp, sizeof(resp));
            pthread_exit(NULL);
            }
        }

    /////////////////////////////////////sending first response and call proxy///////////////////////////////
       ///TODO ino bar asase connect konam!!!!!
       Response* resp = new Response();
       resp->STAT = 0x5a;

       send_sock(sock, (char*)resp, sizeof(resp));

       client_ip = int_to_str(client_conf.sin_addr.s_addr);
       client_port = port;
       server_port = int_to_str_port(read_PORT);

       char* buffer = new char [BUF_SIZE];
       do_proxy(new_socket, sock, buffer, client_ip, client_port, server_ip, server_port);


    }

    if(read_size <= 0)	{
        cout<< "The client from " << client_ip << ":" << client_port << " was disconnected"<<endl;
        socket_lock.lock();
        close(sock);
        socket_lock.unlock();
        pthread_exit(NULL);

    }
     /*
    else if(read_size == -1)
        cerr<<"rcve failed\n";*/
    free(d.sock);
}

