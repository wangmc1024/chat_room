#include<iostream>
#include<unistd.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<string>
#include<fcntl.h>

class Socket{
public:
    Socket(int fd = -1): fd_(fd){};
    Socket(const Socket& other) = delete;
    Socket(Socket&& other) noexcept: fd_(other.fd_) {other.fd_ = -1;}
    Socket& operator= (const Socket& other) = delete;
    Socket& operator= (Socket&& other) noexcept {
        if(this != &other){
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }

    operator int() {
        return fd_;
    }

    void Close(){
        if(fd_ != -1){
            close(fd_);
        }
    }

    ~Socket(){
       Close();
    }
private:
    int fd_;
};

class Client{
public:



};