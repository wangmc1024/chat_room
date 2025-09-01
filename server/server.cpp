#include"server.hpp"

int main(int argc,char* argv[]){
    if(argc != 3){
        std::cerr<<"Error: Parameter Mismatch"<<std::endl;
        return EXIT_FAILURE;
    }

    TcpServer& ts = TcpServer::getInstance();
    ts.ListenConnect(argv[1],argv[2]);
    ts.Run();
}