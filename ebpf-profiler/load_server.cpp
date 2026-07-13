#include <iostream>
#include <cstring>
#include <vector>
#include <chrono>
#include <unordered_map>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

using namespace std;
using namespace chrono;

constexpr int MAX_EVENTS = 1024;
constexpr const char* MSG = "hello";
constexpr size_t MSG_LEN = 5;

struct ConnState {
    int requests_sent = 0;
    int requests_done = 0;
    bool awaiting_response = false;
    char buf[1024];
};

int setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main(int argc, char** argv) {
    if (argc < 4) {
        cerr << "Usage: " << argv[0] << " <host> <port> <num_connections> <requests_per_conn>\n";
        return 1;
    }

    const char* host = argv[1];
    int port = atoi(argv[2]);
    int num_conns = atoi(argv[3]);
    int reqs_per_conn = argc > 4 ? atoi(argv[4]) : 100;

    int epollFd = epoll_create1(0);
    if (epollFd == -1) { perror("epoll_create1"); return 1; }

    unordered_map<int, ConnState> conns;
    conns.reserve(num_conns);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &addr.sin_addr);

    // open all connections up front
    for (int i = 0; i < num_conns; i++) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        setNonBlocking(fd);

        int ret = connect(fd, (sockaddr*)&addr, sizeof(addr));
        if (ret < 0 && errno != EINPROGRESS) {
            perror("connect");
            close(fd);
            continue;
        }

        conns[fd] = ConnState{};

        epoll_event ev{};
        ev.events = EPOLLOUT | EPOLLIN;  // ready to write first, read later
        ev.data.fd = fd;
        epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &ev);
    }

    int total_reqs = num_conns * reqs_per_conn;
    int completed_reqs = 0;

    epoll_event events[MAX_EVENTS];
    auto t_start = steady_clock::now();

    while (completed_reqs < total_reqs && !conns.empty()) {
        int nfds = epoll_wait(epollFd, events, MAX_EVENTS, 5000);
        if (nfds == 0) {
            cerr << "Timed out waiting for events — server may be stalled.\n";
            break;
        }

        for (int i = 0; i < nfds; i++) {
            int fd = events[i].data.fd;
            auto it = conns.find(fd);
            if (it == conns.end()) continue;
            ConnState& state = it->second;

            if (events[i].events & EPOLLOUT) {
                if (!state.awaiting_response && state.requests_sent < reqs_per_conn) {
                    ssize_t sent = send(fd, MSG, MSG_LEN, 0);
                    if (sent > 0) {
                        state.requests_sent++;
                        state.awaiting_response = true;
                    }
                }
            }

            if (events[i].events & EPOLLIN) {
                ssize_t n = recv(fd, state.buf, sizeof(state.buf), 0);
                if (n > 0) {
                    state.requests_done++;
                    completed_reqs++;
                    state.awaiting_response = false;

                    if (state.requests_sent >= reqs_per_conn) {
                        epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, nullptr);
                        close(fd);
                        conns.erase(fd);
                    }
                } else if (n == 0) {
                    epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, nullptr);
                    close(fd);
                    conns.erase(fd);
                }
            }
        }
    }

    auto t_end = steady_clock::now();
    double elapsed_sec = duration_cast<duration<double>>(t_end - t_start).count();

    cout << "connections=" << num_conns << endl
         << " requests_per_conn=" << reqs_per_conn << endl
         << " completed=" << completed_reqs << endl
         << " elapsed=" << elapsed_sec << "s" << endl
         << " req/sec=" << (completed_reqs / elapsed_sec) << endl;

    // clean up any stragglers
    for (auto& [fd, state] : conns) close(fd);

    return 0;
}