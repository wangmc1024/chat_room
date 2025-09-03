#include<sys/socket.h>
#include<sys/epoll.h>
#include<iostream>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<unistd.h>
#include<cstring>
#include<unordered_map>
#include<vector>
#include<fcntl.h>
#include <utility>

constexpr int USER_LIMIT = 5;
constexpr int BUFFER_SIZE = 1024;
constexpr int FD_LIMIT = 65535;



int setnoblocking(int fd){
    int old_flag = fcntl(fd,F_GETFL,0);
    if(old_flag == -1){
        std::cerr<<"set nonblock erro: "<<strerror(errno)<<std::endl;
        return -1;
    }
    int new_flag = old_flag | O_NONBLOCK;
    fcntl(fd,F_SETFL,new_flag);
    return old_flag;
}

struct Users{
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

    int value() const {return fd_;}

    operator int(){
        return this->value();
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


// namespace std{
//     template<>
//     struct hash<Socket>{
//         size_t operator()(const Socket& socket)const noexcept{
//             return hash<int>()(socket.value());
//         }
//     };
// }

// bool operator==(const Socket&lhs,const Socket&rhs){
//     return lhs.value() == rhs.value();
// }

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

    void ListenConnect(std::string IP,std::string port){
        Bind(std::move(IP),std::move(port));
        Listen(BUFFER_SIZE);
    }

    void Run(){
        revents_.resize(FD_LIMIT);
        while(1){
            int ret = epoll_.Wait(revents_.data(),-1);
            for(int i = 0; i < ret; ++i){
                if((revents_[i].data.fd == listen_fd_)&&(revents_[i].events & EPOLLIN)){
                    struct sockaddr_in client_addr{};
                    socklen_t len = sizeof(client_addr);
                    Socket client_sock(accept(listen_fd_.value(),reinterpret_cast<struct sockaddr*>(&client_addr),&len));
                    if(client_sock == -1){
                        std::cerr<<"accept error:"<<strerror(errno)<<std::endl;
                        continue;
                    }
                    if(ret >= USER_LIMIT){
                        std::string info = "there are too many users, please wait";
                        send(client_sock.value(),info.data(),sizeof(info),0);
                        close(client_sock.value());
                        continue;
                    }
                    setnoblocking(client_sock.value());
                    epoll_.Add(client_sock.value(),EPOLLIN|EPOLLHUP|EPOLLERR);
                    users_[client_sock.value()].addr = client_addr;
                    this->client_sock_.insert(std::make_pair(client_sock.value(),std::move(client_sock)));
                    std::cout<<"one user enter"<<std::endl;
                }
                else if(revents_[i].events & (EPOLLHUP | EPOLLERR)){
                    int temp_fd = revents_[i].data.fd;
                    epoll_.Remove(temp_fd);
                    users_[temp_fd] = Users{};
                    --ret;
                    revents_[i] = revents_[ret];
                    --i;
                    client_sock_.erase(temp_fd);
                    std::cerr<<"infor: one user left"<<std::endl;
                    continue;
                }
                else if(revents_[i].events & EPOLLIN){
                    int temp_fd = revents_[i].data.fd;

                    memset(users_[temp_fd].read_buff,'\0',BUFFER_SIZE);
                    if(recv(temp_fd,users_[temp_fd].read_buff,BUFFER_SIZE-1,0) < 0){
                        std::cerr<<"revevie error: "<<strerror(errno)<<std::endl;
                        continue;
                    }
                    std::cout<<"from client:"<<users_[temp_fd].read_buff<<std::endl;
                    for(int j = 0; j < ret; ++j){
                        if(revents_[j].data.fd == listen_fd_ || revents_[j].data.fd == temp_fd){
                            continue;
                        }
                        users_[revents_[j].data.fd].write_buff = users_[temp_fd].read_buff;
                       epoll_.Modify(temp_fd, EPOLLOUT | EPOLLHUP | EPOLLERR);
                    }
                }
                else if(revents_[i].events & EPOLLOUT){
                    int temp_fd = revents_[i].data.fd;
                    if(!users_[temp_fd].write_buff){
                        continue;
                    }
                    int data_len = send(temp_fd,users_[temp_fd].write_buff,strlen(users_[temp_fd].write_buff),0);
                    if(data_len < 0){
                        std::cerr<<"send error: "<<strerror(errno)<<std::endl;
                    }
                    epoll_.Modify(temp_fd, EPOLLIN | EPOLLHUP | EPOLLERR);
                    users_[temp_fd].write_buff = nullptr;
                }

            }
        }
    }

private:
    TcpServer(){
        listen_fd_ = socket(PF_INET,SOCK_STREAM,0);
        epoll_ = Epoll();
        epoll_.Add(listen_fd_.value(),EPOLLIN);
    }

    void Bind(std::string&& IP,std::string&& port){
        uint16_t port_ = std::stoi(port);

        struct sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port_);
        inet_pton(AF_INET,IP.data(),&server_addr.sin_addr);

        int reuse = 1;
        setsockopt(listen_fd_.value(),SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));

        int ret = bind(listen_fd_.value(),reinterpret_cast<struct sockaddr*>(&server_addr),sizeof(server_addr));
        if(ret == -1){
            std::cerr<<"bind error: "<<strerror(errno)<<std::endl;
        }
    }

    void Listen(int backlog){
        if(listen(listen_fd_.value(),backlog)==-1){
            std::cerr<<"listen error: "<<strerror(errno)<<std::endl;
            return;
        }
    }

private:
    Socket listen_fd_;
    Epoll epoll_;
    std::vector<struct Users> users_{FD_LIMIT};
    std::vector<struct epoll_event> revents_;
    std::unordered_map<int,Socket> client_sock_{USER_LIMIT};
  
};