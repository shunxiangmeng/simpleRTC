#include <memory>
#include <mutex>
#include <unordered_map>
#include "socket_util.h"
#include "logger.h"

namespace infra {

int close_socket(int fd) {
    #if defined(_WIN32)
        return closesocket(fd);
    #else
        return close(fd);
    #endif
}

int ioctl(int fd, long cmd, u_long *ptr) {
#if defined(_WIN32)
    return ioctlsocket(fd, cmd, ptr);
#else
    return ioctl(fd, cmd, ptr);
#endif
}

int get_uv_error(bool netErr) {
#if defined(_WIN32)
    auto errCode = netErr ? WSAGetLastError() : GetLastError();
    return errCode;
#endif 
}

static int set_ipv6_only(int fd, bool flag) {
    int opt = flag;
    int ret = setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&opt, sizeof opt);
    if (ret == -1) {
        errorf("setsockopt IPV6_V6ONLY failed\n");
    }
    return ret;
}

static int bind_sock6(int fd, const char *ifr_ip, uint16_t port) {
    set_ipv6_only(fd, false);
    struct sockaddr_in6 addr;
    memset(&addr, 0x00, sizeof(addr));
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(port);
    if (1 != inet_pton(AF_INET6, ifr_ip, &(addr.sin6_addr))) {
        if (strcmp(ifr_ip, "0.0.0.0")) {
            errorf("inet_pton to ipv6 address failed: {}", ifr_ip);
        }
        addr.sin6_addr = IN6ADDR_ANY_INIT;
    }
    if (::bind(fd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
        errorf("Bind socket failed: %d\n", fd);
        return -1;
    }
    return 0;
}

static int bind_sock4(int fd, const char *ifr_ip, uint16_t port) {
    struct sockaddr_in addr;
    memset(&addr, 0x00, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (1 != inet_pton(AF_INET, ifr_ip, &(addr.sin_addr))) {
        if (strcmp(ifr_ip, "::")) {
            errorf("inet_pton to ipv4 address failed: {}", ifr_ip);
        }
        addr.sin_addr.s_addr = INADDR_ANY;
    }
    if (::bind(fd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
        errorf("Bind socket failed: {}", fd);
        return -1;
    }
    return 0;
}

static int bind_sock(int fd, const char *ifr_ip, uint16_t port, int family) {
    switch (family) {
        case AF_INET: 
            return bind_sock4(fd, ifr_ip, port);
        case AF_INET6: 
            return bind_sock6(fd, ifr_ip, port);
        default:
            return -1;
    }
}


class DnsCache {
public:
    static DnsCache &Instance() {
        static DnsCache instance;
        return instance;
    }

    bool getDomainIP(const char *host, sockaddr_storage &storage, int ai_family = AF_INET,
                     int ai_socktype = SOCK_STREAM, int ai_protocol = IPPROTO_TCP, int expire_sec = 60) {
        try {
            storage = SocketUtil::make_sockaddr(host, 0);
            return true;
        } catch (...) {
            auto item = getCacheDomainIP(host, expire_sec);
            if (!item) {
                item = getSystemDomainIP(host);
                if (item) {
                    setCacheDomainIP(host, item);
                }
            }
            if (item) {
                auto addr = getPerferredAddress(item.get(), ai_family, ai_socktype, ai_protocol);
                memcpy(&storage, addr->ai_addr, addr->ai_addrlen);
            }
            return (bool)item;
        }
    }

private:
    class DnsItem {
    public:
        std::shared_ptr<struct addrinfo> addr_info;
        time_t create_time;
    };

    std::shared_ptr<struct addrinfo> getCacheDomainIP(const char *host, int expireSec) {
        std::lock_guard<std::mutex> lck(_mtx);
        auto it = _dns_cache.find(host);
        if (it == _dns_cache.end()) {
            //没有记录
            return nullptr;
        }
        if (it->second.create_time + expireSec < time(nullptr)) {
            //已过期
            _dns_cache.erase(it);
            return nullptr;
        }
        return it->second.addr_info;
    }

    void setCacheDomainIP(const char *host, std::shared_ptr<struct addrinfo> addr) {
        std::lock_guard<std::mutex> lck(_mtx);
        DnsItem item;
        item.addr_info = std::move(addr);
        item.create_time = time(nullptr);
        _dns_cache[host] = std::move(item);
    }

    std::shared_ptr<struct addrinfo> getSystemDomainIP(const char *host) {
        struct addrinfo *answer = nullptr;
        //阻塞式dns解析，可能被打断
        int ret = -1;
        do {
            ret = getaddrinfo(host, nullptr, nullptr, &answer);
        } while (ret == -1 && get_uv_error(true) == EINTR);

        if (!answer) {
            errorf("getaddrinfo failed: {}", host);
            return nullptr;
        }
        return std::shared_ptr<struct addrinfo>(answer, freeaddrinfo);
    }

    struct addrinfo *getPerferredAddress(struct addrinfo *answer, int ai_family, int ai_socktype, int ai_protocol) {
        auto ptr = answer;
        while (ptr) {
            if (ptr->ai_family == ai_family && ptr->ai_socktype == ai_socktype && ptr->ai_protocol == ai_protocol) {
                return ptr;
            }
            ptr = ptr->ai_next;
        }
        return answer;
    }

private:
    std::mutex _mtx;
    std::unordered_map<std::string, DnsItem> _dns_cache;
};

SocketUtil s_socketUtil;

SocketUtil::SocketUtil() {
#if defined(_WIN32)
    WSADATA wsdata;
    WSAStartup(MAKEWORD(2, 2), &wsdata);
    infof("win32 sokcet init\n");
#endif
}

SocketUtil::~SocketUtil() {
#if defined(_WIN32)
    WSACleanup();
    infof("win32 sokcet deinit\n");
#endif
}

bool SocketUtil::getDomainIP(const char *host, uint16_t port, struct sockaddr_storage &addr,
                           int ai_family, int ai_socktype, int ai_protocol, int expire_sec) {
    bool flag = DnsCache::Instance().getDomainIP(host, addr, ai_family, ai_socktype, ai_protocol, expire_sec);
    if (flag) {
        switch (addr.ss_family ) {
            case AF_INET : ((sockaddr_in *) &addr)->sin_port = htons(port); break;
            case AF_INET6 : ((sockaddr_in6 *) &addr)->sin6_port = htons(port); break;
            default: break;
        }
    }
    return flag;
}

int SocketUtil::listen(const uint16_t port, const char *local_ip, int back_log) {
    int fd = -1;
    int family = support_ipv6() ? (is_ipv4(local_ip) ? AF_INET : AF_INET6) : AF_INET;
    if ((fd = (int)socket(family, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        warnf("Create socket failed!\n");
        return -1;
    }

    setReuseable(fd, true, false);
    setNoBlocked(fd);
    setCloExec(fd);

    if (bind_sock(fd, local_ip, port, family) == -1) {
        close_socket(fd);
        errorf("bind_socket error");
        return -1;
    }
    if (::listen(fd, back_log) == -1) {
        errorf("Listen socket failed: %d", get_uv_error(true));
        close_socket(fd);
        return -1;
    }

    return fd;
}

int SocketUtil::connect(const char *host, uint16_t port, bool async, const char *local_ip, uint16_t local_port) {
    struct sockaddr_storage addr;
    //dns
    if (!getDomainIP(host, port, addr, AF_INET, SOCK_STREAM, IPPROTO_TCP)) {
        //dns解析失败
        return -1;
    }

    int sockfd = (int) socket(addr.ss_family, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd < 0) {
        warnf("Create socket failed: %s", host);
        return -1;
    }

    setReuseable(sockfd);
    setNoSigpipe(sockfd);
    setNoBlocked(sockfd, async);
    setNoDelay(sockfd);
    setSendBuf(sockfd);
    setRecvBuf(sockfd);
    setCloseWait(sockfd);
    setCloExec(sockfd);

    if (bind_sock(sockfd, local_ip, local_port, addr.ss_family) == -1) {
        close_socket(sockfd);
        return -1;
    }
    if (::connect(sockfd, (sockaddr *) &addr, get_sock_len((sockaddr *)&addr)) == 0) {
        //同步连接成功
        return sockfd;
    }
    int error = get_uv_error(true);
    errorf("connect error %d", error);

    //返回错误码判断
    close_socket(sockfd);
    return -1;
}




int SocketUtil::setNoBlocked(int fd, bool noblock) {
#if defined(_WIN32)
    unsigned long ul = noblock;
    int ret = ioctlsocket(fd, FIONBIO, &ul); //设置为非阻塞模式
#else
    int ul = noblock;
    int ret = ioctl(fd, FIONBIO, &ul);
#endif //defined(_WIN32)
    if (ret == -1) {
        tracef("ioctl FIONBIO failed");
    }

    return ret;
}

int SocketUtil::setReuseable(int fd, bool on, bool reuse_port) {
    int opt = on ? 1 : 0;
    int ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *) &opt, static_cast<socklen_t>(sizeof(opt)));
    if (ret == -1) {
        tracef("setsockopt SO_REUSEADDR failed");
        return ret;
    }
#if defined(SO_REUSEPORT)
    if (reuse_port) {
        ret = setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, (char *) &opt, static_cast<socklen_t>(sizeof(opt)));
        if (ret == -1) {
            tracef("setsockopt SO_REUSEPORT failed");
        }
    }
#endif
    return ret;
}

int SocketUtil::setNoSigpipe(int fd) {
#if defined(SO_NOSIGPIPE)
    int set = 1;
    auto ret = setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, (char *) &set, sizeof(int));
    if (ret == -1) {
        TraceL << "setsockopt SO_NOSIGPIPE failed";
    }
    return ret;
#else
    return -1;
#endif
}

int SocketUtil::setNoDelay(int fd, bool on) {
    int opt = on ? 1 : 0;
    int ret = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *) &opt, static_cast<socklen_t>(sizeof(opt)));
    if (ret == -1) {
        errorf("setsockopt TCP_NODELAY failed");
    }
    return ret;
}

int SocketUtil::setSendBuf(int fd, int size) {
    int ret = setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (char *) &size, sizeof(size));
    if (ret == -1) {
        errorf("setsockopt SO_SNDBUF failed");
    }
    return ret;
}

int SocketUtil::setRecvBuf(int fd, int size) {
    int ret = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char *) &size, sizeof(size));
    if (ret == -1) {
        errorf("setsockopt SO_RCVBUF failed");
    }
    return ret;
}


int SocketUtil::setCloseWait(int fd, int second) {
    linger sLinger;
    //在调用closesocket()时还有数据未发送完，允许等待
    // 若m_sLinger.l_onoff=0;则调用closesocket()后强制关闭
    sLinger.l_onoff = (second > 0);
    sLinger.l_linger = second; //设置等待时间为x秒
    int ret = setsockopt(fd, SOL_SOCKET, SO_LINGER, (char *) &sLinger, sizeof(linger));
    if (ret == -1) {
        errorf("setsockopt SO_LINGER failed");
    }
    return ret;
}

int SocketUtil::setCloExec(int fd, bool on) {
#if !defined(_WIN32)
    int flags = fcntl(fd, F_GETFD);
    if (flags == -1) {
        errorf("fcntl F_GETFD failed");
        return -1;
    }
    if (on) {
        flags |= FD_CLOEXEC;
    } else {
        int cloexec = FD_CLOEXEC;
        flags &= ~cloexec;
    }
    int ret = fcntl(fd, F_SETFD, flags);
    if (ret == -1) {
        errorf("fcntl F_SETFD failed");
        return -1;
    }
    return ret;
#else
    return -1;
#endif
}


using getsockname_type = decltype(getsockname);

static bool get_socket_addr(int fd, struct sockaddr_storage &addr, getsockname_type func) {
    socklen_t addr_len = sizeof(addr);
    if (-1 == func(fd, (struct sockaddr *)&addr, &addr_len)) {
        return false;
    }
    return true;
}

uint16_t SocketUtil::inet_port(const struct sockaddr *addr) {
    switch (addr->sa_family) {
        case AF_INET: 
            return ntohs(((struct sockaddr_in *)addr)->sin_port);
        case AF_INET6: 
            return ntohs(((struct sockaddr_in6 *)addr)->sin6_port);
        default:
            return 0;
    }
}

static uint16_t get_socket_port(int fd, getsockname_type func) {
    struct sockaddr_storage addr;
    if (!get_socket_addr(fd, addr, func)) {
        return 0;
    }
    return SocketUtil::inet_port((struct sockaddr *)&addr);
}

uint16_t SocketUtil::get_local_port(int fd) {
    return get_socket_port(fd, getsockname);
}

uint16_t SocketUtil::get_peer_port(int fd) {
    return get_socket_port(fd, getpeername);
}

bool SocketUtil::support_ipv6() {
    auto fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    if (fd == -1) {
        return false;
    }
    close_socket((int)fd);
    return true;
}

bool SocketUtil::is_ipv4(const char *host) {
    struct in_addr addr;
    return 1 == inet_pton(AF_INET, host, &addr);
}

bool SocketUtil::is_ipv6(const char *host) {
    struct in6_addr addr;
    return 1 == inet_pton(AF_INET6, host, &addr);
}

socklen_t SocketUtil::get_sock_len(const struct sockaddr *addr) {
    switch (addr->sa_family) {
        case AF_INET : return sizeof(sockaddr_in);
        case AF_INET6 : return sizeof(sockaddr_in6);
        default:return 0;
    }
}

struct sockaddr_storage SocketUtil::make_sockaddr(const char *host, uint16_t port) {
    struct sockaddr_storage storage;
    memset(&storage, 0x00, sizeof(storage));

    struct in_addr addr;
    struct in6_addr addr6;
    if (1 == inet_pton(AF_INET, host, &addr)) {
        // host是ipv4
        reinterpret_cast<struct sockaddr_in &>(storage).sin_addr = addr;
        reinterpret_cast<struct sockaddr_in &>(storage).sin_family = AF_INET;
        reinterpret_cast<struct sockaddr_in &>(storage).sin_port = htons(port);
        return storage;
    }
    if (1 == inet_pton(AF_INET6, host, &addr6)) {
        // host是ipv6
        reinterpret_cast<struct sockaddr_in6 &>(storage).sin6_addr = addr6;
        reinterpret_cast<struct sockaddr_in6 &>(storage).sin6_family = AF_INET6;
        reinterpret_cast<struct sockaddr_in6 &>(storage).sin6_port = htons(port);
        return storage;
    }
    throw std::invalid_argument(std::string("Not ip address: ") + host);
}

}