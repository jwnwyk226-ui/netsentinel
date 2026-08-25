// port_service_scanner.cpp
// Simple TCP port scanner with optional banner grabbing.

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <algorithm>

static bool connect_timeout(int sock, const struct sockaddr* addr, socklen_t alen, int timeout_ms) {
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags >= 0) fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    int res = connect(sock, addr, alen);
    if (res == 0) {
        if (flags >= 0) fcntl(sock, F_SETFL, flags);
        return true;
    }
    if (errno != EINPROGRESS) return false;
    fd_set wf;
    FD_ZERO(&wf); FD_SET(sock, &wf);
    struct timeval tv; tv.tv_sec = timeout_ms/1000; tv.tv_usec = (timeout_ms%1000)*1000;
    int r = select(sock+1, nullptr, &wf, nullptr, &tv);
    if (r <= 0) return false;
    int soerr = 0; socklen_t sl = sizeof(soerr);
    if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &soerr, &sl) < 0) return false;
    if (flags >= 0) fcntl(sock, F_SETFL, flags);
    return soerr == 0;
}

static std::string grab_banner(int sock, int timeout_ms) {
    fd_set rf; FD_ZERO(&rf); FD_SET(sock, &rf);
    struct timeval tv; tv.tv_sec = timeout_ms/1000; tv.tv_usec = (timeout_ms%1000)*1000;
    int r = select(sock+1, &rf, nullptr, nullptr, &tv);
    if (r <= 0) return "";
    char buf[2048]; ssize_t n = recv(sock, buf, sizeof(buf)-1, 0);
    if (n <= 0) return "";
    buf[n] = '\0';
    return std::string(buf);
}

struct ScanResult { int port; bool open; std::string banner; };

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "الاستخدام: port_service_scanner <ip> [start_port end_port] [threads]\n";
        return 1;
    }
    std::string ip = argv[1];
    int start = 1, end = 1024;
    int threads = 100;
    if (argc >= 4) { start = atoi(argv[2]); end = atoi(argv[3]); }
    if (argc >= 5) threads = atoi(argv[4]);

    struct sockaddr_in base_addr;
    memset(&base_addr,0,sizeof(base_addr)); base_addr.sin_family = AF_INET;
    if (inet_pton(AF_INET, ip.c_str(), &base_addr.sin_addr) != 1) {
        std::cerr << "عنوان IP غير صالح\n"; return 1;
    }

    std::cout << "جارٍ فحص " << ip << " المنافذ من " << start << " إلى " << end << " بـ " << threads << " خيوط\n";

    std::queue<int> port_queue;
    for (int p = start; p <= end; ++p) port_queue.push(p);

    std::mutex qmut, outmut;
    std::condition_variable qcv;
    std::vector<ScanResult> results;
    // no longer need 'done'

    auto worker = [&]() {
        while (true) {
            int port = 0;
            {
                std::unique_lock<std::mutex> lk(qmut);
                if (port_queue.empty()) break;
                port = port_queue.front(); port_queue.pop();
            }
            struct sockaddr_in addr = base_addr;
            addr.sin_port = htons(port);
            int sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0) continue;
            bool ok = connect_timeout(sock, (struct sockaddr*)&addr, sizeof(addr), 300);
            ScanResult r; r.port = port; r.open = false;
            if (ok) {
                r.open = true;
                r.banner = grab_banner(sock, 300);
            }
            close(sock);
            {
                std::lock_guard<std::mutex> lk(outmut);
                results.push_back(r);
            }
        }
    };

    std::vector<std::thread> pool;
    for (int i = 0; i < threads; ++i) pool.emplace_back(worker);
    for (auto &t : pool) if (t.joinable()) t.join();

    std::sort(results.begin(), results.end(), [](const ScanResult&a,const ScanResult&b){return a.port<b.port;});
    for (auto &r : results) {
        if (r.open) {
            std::cout << "مفتوح: " << r.port;
            if (!r.banner.empty()) std::cout << "  بانر: " << r.banner.substr(0,200);
            std::cout << "\n";
        }
    }
    return 0;
}
