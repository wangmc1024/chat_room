#include<sys/socket.h>
#include<sys/epoll.h>
#include<iostream>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<unistd.h>
#include<cstring>
#include<vector>

constexpr int USER_LIMIT = 5;
constexpr int BUFFER_SIZE = 1024;
constexpr int FD_LIMIT = 65535;

class Users{
public:
    struct sockaddr_in addr;
    char* write_buff;
    char read_buff[BUFFER_SIZE];
};

class Socket{
public:
    Socket():fd_(-1){}
    Socket(int fd){
        if(fd > 0){
            fd_ = fd;
        }
    }

    Socket(const Socket&)=delete;
    Socket(Socket&& other) noexcept:fd_(other.fd_){ other.fd_ = -1;}
    Socket& operator=(const Socket&)=delete;
    Socket& operator=(Socket&& other){
        if(this != &other){
            Close();
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }

    operator int(){
        return fd_;
    }

    bool Reset(int fd){
        if(fd < 0) return false;
        Close();
        fd_ = fd;
        return true;
    }

    ~Socket(){
        Close();
    }

    void Close(){
        if(fd_ != -1){
            close(fd_);
            fd_ = -1;
        }
    }

private:
    int fd_;
};


class Epoll{
public:
    Epoll(){
        epfd_ = epoll_create1(EPOLL_CLOEXEC);
    }
    Epoll(const Epoll&) = delete;
    Epoll(Epoll&& other) noexcept: epfd_(other.epfd_){ other.epfd_ = -1;}
    Epoll& operator=(const Epoll&) = delete;
    Epoll& operator=(Epoll&& other){
        if(this != &other){
            Close();
            epfd_ = other.epfd_;
            other.epfd_ = -1;
        }
        return *this;
    }

     bool Add(int fd, uint32_t events, void* data = nullptr) {
        if (fd == -1) {
            std::cerr << "Invalid file descriptor" << std::endl;
            return false;
        }
        
        struct epoll_event ev{};
        ev.events = events;
        if (data) {
            ev.data.ptr = data;
        } else {
            ev.data.fd = fd;
        }
        
        if (epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev) == -1) {
            std::cerr << "epoll_ctl ADD failed for fd " << fd << ": " 
                      << strerror(errno) << std::endl;
            return false;
        }
        return true;
    }
    
    
    bool Remove(int fd) {
        if (fd == -1) {
            return true; 
        }
        
        if (epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr) == -1) {
            std::cerr << "epoll_ctl DEL failed for fd " << fd << ": " 
                      << strerror(errno) << std::endl;
            return false;
        }
        return true;
    }
    
    bool Modify(int fd,uint32_t events, void* data = nullptr){
        if(fd < 0){
            return false;
        }

        struct epoll_event ev;
        ev.events = events;
        if(data){
            ev.data.ptr = data;
        }else{
            ev.data.fd = fd;
        }

        if(epoll_ctl(epfd_,EPOLL_CTL_MOD,fd,&ev) == -1){
            return false;
        }
        return true;
    }

    int Wait(struct epoll_event* ev,int timeout){
        if(!ev){
            errno = EINVAL;
            return -1;
        }

        return epoll_wait(epfd_,ev,FD_LIMIT,timeout);
    }

    void Close(){
        if(epfd_ != -1){
            close(epfd_);
        }
    }

    ~Epoll(){ Close(); }

private:
    int epfd_;
};


class TcpServer{
public:
    static TcpServer& getInstance(){
        static TcpServer instance;
        return instance;
    }

    int ListenConnect(std::string IP,std::string port){
        Bind(std::move(IP),std::move(port));
        Listen(BUFFER_SIZE);
    }

    void Run(){

    }

private:
    TcpServer(){
        listen_fd_ = socket(PF_INET,SOCK_STREAM,0);
        epoll_ = Epoll();
        epoll_.Add(listen_fd_,EPOLLIN);
    }

    void Bind(std::string&& IP,std::string&& port){
        uint16_t port_ = std::stoi(port);

        struct sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port_);
        inet_pton(AF_INET,IP.data(),&server_addr.sin_addr);

        int ret = bind(listen_fd_,reinterpret_cast<struct sockaddr*>(&server_addr),sizeof(server_addr));
        if(ret == -1){
            std::cerr<<"bind error: "<<strerror(errno)<<std::endl;
        }
    }

    void Listen(int backlog){
        if(listen(listen_fd_,backlog)==-1){
            std::cerr<<"listen error: "<<strerror(errno)<<std::endl;
        }
    }

private:
    Socket listen_fd_;
    Epoll epoll_;
    std::vector<Users> users_;

};