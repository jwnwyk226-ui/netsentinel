// device_names.cpp
// Discover hostnames for given IPs using reverse DNS and NetBIOS (NBSTAT) query.

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

static bool reverse_dns(const std::string& ip, std::string& name) {
    struct sockaddr_in sa;
    char host[NI_MAXHOST];
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    if (inet_pton(AF_INET, ip.c_str(), &sa.sin_addr) != 1) return false;
    if (getnameinfo((struct sockaddr*)&sa, sizeof(sa), host, sizeof(host), nullptr, 0, 0) == 0) {
        name = host;
        return true;
    }
    return false;
}

// Build NBSTAT query for NetBIOS Node Status (RFC1002) for name '*'
static std::vector<unsigned char> build_nbstat_query(uint16_t txid = 0x1337) {
    std::vector<unsigned char> pkt;
    pkt.resize(50);
    // Header
    pkt[0] = (txid >> 8) & 0xff;
    pkt[1] = txid & 0xff;
    pkt[2] = 0x00; pkt[3] = 0x00; // flags
    pkt[4] = 0x00; pkt[5] = 0x01; // questions
    pkt[6] = pkt[7] = pkt[8] = pkt[9] = pkt[10] = pkt[11] = 0x00;
    // Encoded name: '*' padded to 15 chars then 0x20
    // NetBIOS name encoding: each byte -> two ASCII chars
    // We'll encode the 16-byte name consisting of 0x2A '*' + 15 spaces
    const unsigned char name16[16] = {'*', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
    int pos = 12;
    pkt[pos++] = 0x20; // length of encoded name (32)
    for (int i = 0; i < 16; ++i) {
        unsigned char c = name16[i];
        unsigned char hi = ((c >> 4) & 0x0F) + 'A';
        unsigned char lo = (c & 0x0F) + 'A';
        pkt[pos++] = hi;
        pkt[pos++] = lo;
    }
    pkt[pos++] = 0x00; // name null terminator
    // type NBSTAT (0x0021) and class IN (0x0001)
    pkt[pos++] = 0x00; pkt[pos++] = 0x21;
    pkt[pos++] = 0x00; pkt[pos++] = 0x01;
    pkt.resize(pos);
    return pkt;
}

static bool nbstat_query(const std::string& ip, std::string& info, int timeout_ms = 500) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return false;
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(137);
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) != 1) {
        close(sock); return false;
    }
    auto pkt = build_nbstat_query();
    ssize_t sent = sendto(sock, pkt.data(), pkt.size(), 0, (struct sockaddr*)&addr, sizeof(addr));
    if (sent < 0) { close(sock); return false; }
    fd_set rfds;
    FD_ZERO(&rfds); FD_SET(sock, &rfds);
    struct timeval tv; tv.tv_sec = timeout_ms/1000; tv.tv_usec = (timeout_ms%1000)*1000;
    int r = select(sock+1, &rfds, nullptr, nullptr, &tv);
    if (r <= 0) { close(sock); return false; }
    unsigned char buf[1500];
    ssize_t rec = recv(sock, buf, sizeof(buf), 0);
    close(sock);
    if (rec <= 0) return false;
    // Try to extract MAC from the end of NBSTAT (last 6 bytes often MAC)
    if (rec >= 6) {
        char mac[32];
        size_t off = rec >= 6 ? rec-6 : 0;
        snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x",
                 buf[off], buf[off+1], buf[off+2], buf[off+3], buf[off+4], buf[off+5]);
        info = "NBSTAT response, approx MAC=" + std::string(mac);
        return true;
    }
    return false;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "الاستخدام: device_names <ip1> [ip2 ...]\n";
        return 1;
    }
    for (int i = 1; i < argc; ++i) {
        std::string ip = argv[i];
        std::cout << "عنوان IP: " << ip << "\n";
        std::string name;
        if (reverse_dns(ip, name)) {
            std::cout << "  اسم (Reverse DNS): " << name << "\n";
        } else {
            std::cout << "  اسم (Reverse DNS): (لا يوجد)\n";
        }
        std::string nbinfo;
        if (nbstat_query(ip, nbinfo)) {
            std::cout << "  NetBIOS/NBSTAT: " << nbinfo << "\n";
        } else {
            std::cout << "  NetBIOS/NBSTAT: (لا يوجد رد)\n";
        }
    }
    return 0;
}
