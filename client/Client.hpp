#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdexcept>
#include <string>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <utility>

inline bool setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        std::cerr << "fcntl F_GETFL failed: " << strerror(errno) << std::endl;
        return false;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        std::cerr << "fcntl F_SETFL failed: " << strerror(errno) << std::endl;
        return false;
    }
    return true;
}

class Socket {
public:
    Socket() : fd_(-1) {}

    explicit Socket(int fd) : fd_(fd) {
        if (fd_ != -1 && !setNonBlocking(fd_)) {
            Close();
            std::cerr<<"Failed to set socket non-blocking"<<std::endl;
        }
    }
    
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    
    Socket(Socket&& other) noexcept : fd_(other.fd_) {
        other.fd_ = -1;
    }
    

    Socket& operator=(Socket&& other) noexcept {
        if (this != &other) {
            Close();
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }
    
    ~Socket() {
        Close();
    }
    
    // 转换为int获取fd
    operator int() const {
        return fd_;
    }

    void Close() {
        if (fd_ != -1) {
            close(fd_);
            fd_ = -1;
        }
    }
    
    // 重置为新的fd
    bool reset(int fd) {
        if (fd <= 0) {
            return false;
        }
        
        Close();
        fd_ = fd;
        return setNonBlocking(fd_);
    }
   

private:
    int fd_;
};

class Epoll {
public:
    explicit Epoll(size_t max_events = 1024) : max_events_(max_events) {
        epfd_ = epoll_create1(EPOLL_CLOEXEC);
        if (epfd_ == -1) {
           std::cerr<<"epoll_create1 failed: " + std::string(strerror(errno))<<std::endl;
        }
    }
    
    ~Epoll() {
        if (epfd_ != -1) {
            close(epfd_);
            epfd_ = -1;
        }
    } 
   
    Epoll(const Epoll&) = delete;
    Epoll& operator=(const Epoll&) = delete;
    
   
    Epoll(Epoll&& other) noexcept : epfd_(other.epfd_), max_events_(other.max_events_) {
        other.epfd_ = -1;
    }
    
    Epoll& operator=(Epoll&& other) noexcept {
        if (this != &other) {
            if (epfd_ != -1) {
                close(epfd_);
            }
            epfd_ = other.epfd_;
            max_events_ = other.max_events_;
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
    
   
    bool Modify(int fd, uint32_t events, void* data = nullptr) {
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
        
        if (epoll_ctl(epfd_, EPOLL_CTL_MOD, fd, &ev) == -1) {
            std::cerr << "epoll_ctl MOD failed for fd " << fd << ": " 
                      << strerror(errno) << std::endl;
            return false;
        }
        return true;
    }
    
    // 等待事件发生
    int Wait(struct epoll_event* events, int timeout) const {
        if (!events) {
            errno = EINVAL;
            return -1;
        }
        
        return epoll_wait(epfd_, events, static_cast<int>(max_events_), timeout);
    }
    
   
private:
    int epfd_ = -1;
    size_t max_events_;
};

class EpollHandle {
public:
    explicit EpollHandle(Epoll&& epoll,Socket&& server_sock) : epoll_(std::move(epoll)),server_sock_(std::move(server_sock)) {}
    
    EpollHandle(const EpollHandle&) = delete;
    EpollHandle& operator=(const EpollHandle&) = delete;
    EpollHandle(EpollHandle&&) = delete;
    EpollHandle& operator=(EpollHandle&&) = delete;
    
    // 事件处理主循环
    void run() {
        int wait_times = 0;
        while (true) {
            int ready = epoll_.Wait(events_, -1); // 无限等待，直到有事件发生
            
            if (ready == -1) {
                if (errno == EINTR) {
                    // 被信号中断，继续等待
                    continue;
                }
                std::cerr << "epoll_wait error: " << strerror(errno) << std::endl;
                break;
            }

            if(wait_times == 5){
                std::cerr<<"服务端关闭连接"<<std::endl;
                break;
            }
            
            for (int i = 0; i < ready; ++i) {
                if(handleEvent(events_[i])==-1){
                    ++wait_times;
                    break;
                }
            }
        }
    }

private:
    // 处理单个事件
    int handleEvent(const struct epoll_event& ev) {
        if (ev.events & (EPOLLERR | EPOLLHUP)) {
            handleErrorEvent(ev);
            return -1;
        }
        
        if (ev.events & EPOLLIN) {
            handleReadEvent(ev);
            return 0;
        }
        
        if (ev.events & EPOLLOUT) {
            // 可写事件（当前代码未使用，预留接口）
            handleWriteEvent(ev);
            return 0;
        }

        return -1;
    }
    
    void handleErrorEvent(const struct epoll_event& ev) {
        int fd = (ev.data.ptr) ? -1 : ev.data.fd;
        std::cerr << "Error event for fd " << fd << std::endl;
        
        if (fd != -1) {
            epoll_.Remove(fd);
            close(fd); 
        }
    }
    
    // 处理可读事件
    void handleReadEvent(const struct epoll_event& ev) {
        int fd = (ev.data.ptr) ? -1 : ev.data.fd;
        if (fd == 0) {
            handleStdin();
        } else if (fd != -1) {
            handleSocketRead(fd);
        }
    }
    
    // 处理可写事件（预留）
    void handleWriteEvent(const struct epoll_event& /*ev*/) {
        // 可在此添加处理可写事件的逻辑
    }
    
    // 处理标准输入
    void handleStdin() {
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            std::cerr << "pipe creation failed: " << strerror(errno) << std::endl;
            return;
        }
        
        // 设置管道为非阻塞
        setNonBlocking(pipefd[0]);
        setNonBlocking(pipefd[1]);
        
        // 从标准输入复制数据到管道
        ssize_t n = splice(STDIN_FILENO, nullptr, pipefd[1], nullptr, 
                          32768, SPLICE_F_MORE | SPLICE_F_MOVE);
        if (n == -1) {
            std::cerr << "splice from stdin failed: " << strerror(errno) << std::endl;
            close(pipefd[0]);
            close(pipefd[1]);
            return;
        }
        
        n = splice(pipefd[0], nullptr, server_sock_, nullptr, 
                          32768, SPLICE_F_MORE | SPLICE_F_MOVE);
        if (n == -1) {
            std::cerr << "splice from stdin failed: " << strerror(errno) << std::endl;
            close(pipefd[0]);
            close(pipefd[1]);
            return;
        }
        
        // 关闭管道
        close(pipefd[0]);
        close(pipefd[1]);
    }
    
    // 处理socket读取
    void handleSocketRead(int fd) {
        char buffer[1024];
        memset(buffer,'\0',sizeof(buffer));
        ssize_t bytes_read = recv(fd, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_read == -1) {
            std::cerr << "recv error for fd " << fd << ": " << strerror(errno) << std::endl;
            epoll_.Remove(fd);
            close(fd);
            return;
        }

        
        if (bytes_read == 0) {
            // 连接关闭
            std::cout << "Connection closed for fd " << fd << std::endl;
            epoll_.Remove(fd);
            close(fd);
            return;
        }

        std::cout<<buffer<<std::endl;
        
    }

private:
    Epoll epoll_;
    Socket server_sock_;
    static constexpr size_t MAX_EVENTS = 1024;
    struct epoll_event events_[MAX_EVENTS];
};
