#pragma once

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#pragma comment (lib, "Ws2_32.lib")
#pragma comment(lib,"Iphlpapi.lib")
#else
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif

#include <cstring>
#include <cstdint>
#include <map>
#include <vector>
#include <string>

#define SOCKET_DEFAULT_BUF_SIZE (256 * 1024)

namespace infra {

int close_socket(int fd);
int ioctl(int fd, long cmd, u_long *ptr);
int get_uv_error(bool netErr);

class SocketUtil {
public:

    SocketUtil();

    ~SocketUtil();

    static bool getDomainIP(const char *host, uint16_t port, struct sockaddr_storage &addr, int ai_family = AF_INET,
                            int ai_socktype = SOCK_STREAM, int ai_protocol = IPPROTO_TCP, int expire_sec = 60);

    static int listen(const uint16_t port, const char *local_ip = "::", int back_log = 1024);

    static int connect(const char *host, uint16_t port, bool async = true, const char *local_ip = "::", uint16_t local_port = 0);

    static int setNoBlocked(int fd, bool noblock = true);

    static int setReuseable(int fd, bool on = true, bool reuse_port = true);

    static int setNoDelay(int fd, bool on = true);

    static int setNoSigpipe(int fd);

    static int setSendBuf(int fd, int size = SOCKET_DEFAULT_BUF_SIZE);

    static int setRecvBuf(int fd, int size = SOCKET_DEFAULT_BUF_SIZE);

    static int setCloseWait(int sock, int second = 0);

    static int setCloExec(int fd, bool on = true);

    static std::string get_local_ip(int sock);

    static uint16_t get_local_port(int sock);

    static std::string get_peer_ip(int sock);

    static uint16_t get_peer_port(int sock);

    static uint16_t inet_port(const struct sockaddr *addr);

    static bool support_ipv6();

    static bool is_ipv4(const char *str);

    static bool is_ipv6(const char *str);

    static socklen_t get_sock_len(const struct sockaddr *addr);

    static struct sockaddr_storage make_sockaddr(const char *host, uint16_t port);
};

}