#include <stdexcept>
#include "event_driver.h"
#include "socket_util.h"
#include "logger.h"

namespace infra {

EventDriver::EventDriver() {
#if defined(_WIN32)
    const char *localip = "127.0.0.1";
    auto fd = SocketUtil::listen(0, localip);
    if (fd < 0) {
        throw std::runtime_error("create socket error");
    }
    SocketUtil::setNoBlocked(fd, false);
    auto localPort = SocketUtil::get_local_port(fd);
    fd_[1] = SocketUtil::connect(localip, localPort, false);
    if (fd_[1] < 0) {
        throw std::runtime_error("SocketUtil::connect error");
    }
    fd_[0] = (int)accept(fd, nullptr, nullptr);
    SocketUtil::setNoDelay(fd_[0]);
    SocketUtil::setNoDelay(fd_[1]);
    close_socket(fd);
#else
    if (pipe(fd_) == -1) {
        throw runtime_error("Create posix pipe failed");
    }
#endif
    SocketUtil::setNoBlocked(fd_[0], true);
    SocketUtil::setNoBlocked(fd_[1], false);
    SocketUtil::setCloExec(fd_[0]);
    SocketUtil::setCloExec(fd_[1]);
}

EventDriver::~EventDriver() {
    if (fd_[0] != -1) {
        close_socket(fd_[0]);
        fd_[0] = -1;
    }
    if (fd_[1] != -1) {
        close_socket(fd_[1]);
        fd_[1] = -1;
    }
}

void EventDriver::WakeUp() {
    int ret;
    const char buf[1] = {};
    do {
#if defined(_WIN32)
        ret = send(fd_[1], (char *)buf, 1, 0);
#else
        ret = ::write(_pipe_fd[1], buf, n);
#endif // defined(_WIN32)
    } while (-1 == ret && EINTR == get_uv_error(true));
}

#if defined(_WIN32)
bool EventDriver::Wait(int64_t wait_duration /*ms*/, bool process_io) {

    fd_set socket_set;
    FD_ZERO(&socket_set);
    FD_SET(fd_[0], &socket_set);

    long wait_seconds = long(wait_duration / 1000);
    long wait_miliseconds = wait_duration % 10000000;
    struct timeval timeout = {wait_seconds, wait_miliseconds};
    if (select(FD_SETSIZE, &socket_set, NULL, NULL, &timeout) == SOCKET_ERROR) {
        errorf("select failed\n");
        return false;
    }

    if (FD_ISSET(fd_[0], &socket_set)) {
        char buffer[1];
        const size_t res = ::recv(fd_[0], buffer, sizeof(buffer), 0);
        if (res != 1) {
            warnf("read fd_[0](%d) is not 1, res:%d\n", fd_[0], res);
            return true;
        }
    }

    return true;
}
#else
bool EventDriver::Wait(int64_t wait_duration /*ms*/, bool process_io) {
    return false;
}
#endif // defined(_WIN32)

}