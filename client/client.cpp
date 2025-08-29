#include"Client.hpp"


int main(int argc, char* argv[]){
    if(argc !=3 ){
        std::cerr<<"Error: missing parameter"<<std::endl;
        return EXIT_FAILURE;
    }

    std::string ip = argv[1];
    int port = std::stoi(argv[2]);
    
    if(port<0 || port > 65535){
        std::cerr<<"Error: port is wrong"<<std::endl;
        return EXIT_FAILURE;
    }

    struct sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET,ip.data(),&server_addr.sin_addr);

    Socket server_sock(socket(PF_INET,SOCK_STREAM,0));

    connect(server_sock,reinterpret_cast<struct sockaddr*>(&server_addr),sizeof(server_addr));

    Epoll e;
    e.Add(server_sock,EPOLLIN);
    e.Add(STDIN_FILENO,EPOLLIN);

    EpollHandle eh(std::move(e),std::move(server_sock));
    eh.run();

}