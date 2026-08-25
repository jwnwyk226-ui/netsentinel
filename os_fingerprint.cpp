// os_fingerprint.cpp
// Simple OS guesser using ICMP echo reply TTL heuristic.

#include <arpa/inet.h>
#include <netinet/ip_icmp.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <string>

static uint8_t compute_checksum(uint16_t *buf, int sz) {
    uint32_t sum = 0;
    while (sz > 1) { sum += *buf++; sz -= 2; }
    if (sz == 1) sum += *(uint8_t*)buf;
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    return (uint8_t)(~sum);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "الاستخدام: os_fingerprint <ip>\n";
        return 1;
    }
    const char* target = argv[1];
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock < 0) { perror("socket"); return 1; }

    struct sockaddr_in addr; memset(&addr,0,sizeof(addr));
    addr.sin_family = AF_INET;
    if (inet_pton(AF_INET, target, &addr.sin_addr) != 1) { std::cerr << "عنوان IP غير صالح\n"; close(sock); return 1; }

    struct icmphdr icmp_req;
    memset(&icmp_req, 0, sizeof(icmp_req));
    icmp_req.type = ICMP_ECHO;
    icmp_req.un.echo.id = htons(0x1234);
    icmp_req.un.echo.sequence = htons(1);
    icmp_req.checksum = 0;
    // compute checksum over icmp header only
    icmp_req.checksum = (uint16_t)~(uint16_t)(0); // let kernel fill if possible

    if (sendto(sock, &icmp_req, sizeof(icmp_req), 0, (struct sockaddr*)&addr, sizeof(addr)) <= 0) {
        perror("sendto"); close(sock); return 1;
    }

    unsigned char buf[1500];
    struct sockaddr_in raddr; socklen_t sl = sizeof(raddr);
    ssize_t n = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&raddr, &sl);
    if (n <= 0) { perror("recvfrom"); close(sock); return 1; }

    // parse IP header to get TTL
    struct ip* iph = (struct ip*)buf;
    int ttl = (int)iph->ip_ttl;

    std::string guess = "unknown";
    if (ttl >= 128) guess = "Windows (likely)";
    else if (ttl >= 64) guess = "Linux/Unix (likely)";
    else if (ttl >= 32) guess = "Embedded/Router (likely)";

    std::cout << "IP: " << target << " TTL=" << ttl << " -> " << guess << "\n";
    close(sock);
    return 0;
}
