#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <atomic> // تم الإضافة لدعم التدفق الآمن بين الخيوط
#include <chrono>
#include <set>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <csignal>
#include <net/if.h>
#include <netinet/if_ether.h>
#include <net/ethernet.h>
#include <netpacket/packet.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <unistd.h>

// إجبار المترجم على عدم إضافة بايتات حشو الذاكرة (Padding)
struct __attribute__((packed)) arp_header {
    unsigned short hardware_type;
    unsigned short protocol_type;
    unsigned char hardware_len;
    unsigned char protocol_len;
    unsigned short opcode;
    unsigned char sender_mac[6];
    unsigned char sender_ip[4];
    unsigned char target_mac[6];
    unsigned char target_ip[4];
};

struct DeviceInfo {
    uint32_t ip_num;
    std::string ip_str;
    std::string mac_str;

    // فرز عددي دقيق للـ IP بدلاً من الفرز النصي
    bool operator<(const DeviceInfo& other) const {
        return ntohl(ip_num) < ntohl(other.ip_num);
    }
};

std::set<DeviceInfo> discovered_devices;
std::mutex devices_mutex;

// تعديل: استخدام std::atomic لمنع Data Race أثناء وصول الخيوط
std::atomic<bool> keep_listening{true};

uint32_t my_ip_global = 0;
uint32_t netmask_global = 0;

struct SocketGuard {
    int fd;
    SocketGuard(int fd_ = -1) : fd(fd_) {}
    ~SocketGuard() { if (fd >= 0) ::close(fd); }
    SocketGuard(const SocketGuard&) = delete;
    SocketGuard& operator=(const SocketGuard&) = delete;
    SocketGuard(SocketGuard&& o) noexcept : fd(o.fd) { o.fd = -1; }
    SocketGuard& operator=(SocketGuard&& o) noexcept { if (fd >= 0) ::close(fd); fd = o.fd; o.fd = -1; return *this; }
    explicit operator bool() const { return fd >= 0; }
};

static void handle_signal(int) {
    keep_listening.store(false);
}

bool is_in_local_subnet(uint32_t ip) {
    return (ip & netmask_global) == (my_ip_global & netmask_global);
}

void add_device(uint32_t ip_num, const std::string& ip_str, const std::string& mac_str) {
    if (ip_str == "0.0.0.0" || mac_str == "00:00:00:00:00:00") return;
    if (ip_num == my_ip_global) return; // استثناء جهازك المحلي من القائمة

    std::lock_guard<std::mutex> lock(devices_mutex);
    discovered_devices.insert({ip_num, ip_str, mac_str});
}

void trigger_udp_probe(int udp_sock, uint32_t target_ip, uint16_t port) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = target_ip;

    const char dummy = '\x00';
    sendto(udp_sock, &dummy, 1, MSG_DONTWAIT, (struct sockaddr*)&addr, sizeof(addr));
}

void read_kernel_arp_cache() {
    std::ifstream arp_file("/proc/net/arp");
    if (!arp_file.is_open()) return;

    std::string line;
    std::getline(arp_file, line); // تخطي الترويسة

    while (std::getline(arp_file, line)) {
        std::istringstream iss(line);
        std::string ip, hw_type, flags, mac, mask, dev;
        if (iss >> ip >> hw_type >> flags >> mac >> mask >> dev) {
            unsigned long flag_val = std::strtoul(flags.c_str(), nullptr, 16);
            if ((flag_val & 0x2) && mac != "00:00:00:00:00:00") {
                struct in_addr ina;
                if (inet_pton(AF_INET, ip.c_str(), &ina) == 1) {
                    uint32_t ip_num = ina.s_addr;
                    if (is_in_local_subnet(ip_num)) {
                        add_device(ip_num, ip, mac);
                    }
                }
            }
        }
    }
}

void packet_sniffer(int raw_sock) {
    unsigned char recv_buffer[2048];
    while (keep_listening.load()) {
        ssize_t bytes_received = recv(raw_sock, recv_buffer, sizeof(recv_buffer), 0);
        if (bytes_received <= 0) continue;

        // تعديل: فحص حجم الحزمة قبل تحليليها لتفادي قراءة ذاكرة عشوائية (Out-of-Bounds Read)
        const size_t min_req_size = sizeof(struct ether_header) + sizeof(struct arp_header);
        if (static_cast<size_t>(bytes_received) < min_req_size) {
            continue;
        }

        struct ether_header* eth = (struct ether_header*)recv_buffer;
        if (ntohs(eth->ether_type) == ETH_P_ARP) {
            struct arp_header* arp = (struct arp_header*)(recv_buffer + sizeof(struct ether_header));
            uint16_t opcode = ntohs(arp->opcode);

            if (opcode == ARPOP_REPLY || opcode == ARPOP_REQUEST) {
                uint32_t sender_ip_raw;
                memcpy(&sender_ip_raw, arp->sender_ip, 4);

                if (is_in_local_subnet(sender_ip_raw)) {
                    char ip_buf[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, arp->sender_ip, ip_buf, sizeof(ip_buf));

                    char mac_buf[18];
                    snprintf(mac_buf, sizeof(mac_buf), "%02x:%02x:%02x:%02x:%02x:%02x",
                             arp->sender_mac[0], arp->sender_mac[1], arp->sender_mac[2],
                             arp->sender_mac[3], arp->sender_mac[4], arp->sender_mac[5]);

                    add_device(sender_ip_raw, ip_buf, mac_buf);
                }
            }
        }
    }
}

int main(int argc, char* argv[]) {
    const char* interface_name = (argc > 1) ? argv[1] : "wlp3s0";

    if (geteuid() != 0) {
        std::cerr << "خطأ: يجب تشغيل البرنامج بصلاحيات الجذر (root) لاستخدام raw sockets.\n";
        return 1;
    }

    std::signal(SIGINT, handle_signal);

    int raw_fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (raw_fd < 0) {
        perror("socket AF_PACKET");
        return 1;
    }
    SocketGuard raw_sock_guard(raw_fd);

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 300000;
    setsockopt(raw_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    int ifindex = if_nametoindex(interface_name);
    if (ifindex == 0) {
        std::cerr << "خطأ: الواجهة " << interface_name << " غير موجودة.\n";
        return 1;
    }

    struct ifreq ifr, ifr_ip, ifr_mask;
    memset(&ifr, 0, sizeof(ifr));
    memset(&ifr_ip, 0, sizeof(ifr_ip));
    memset(&ifr_mask, 0, sizeof(ifr_mask));

    snprintf(ifr.ifr_name, IFNAMSIZ, "%s", interface_name);
    snprintf(ifr_ip.ifr_name, IFNAMSIZ, "%s", interface_name);
    snprintf(ifr_mask.ifr_name, IFNAMSIZ, "%s", interface_name);

    if (ioctl(raw_fd, SIOCGIFHWADDR, &ifr) < 0 ||
        ioctl(raw_fd, SIOCGIFADDR, &ifr_ip) < 0 ||
        ioctl(raw_fd, SIOCGIFNETMASK, &ifr_mask) < 0) {
        perror("ioctl error");
        return 1;
    }

    my_ip_global = ((struct sockaddr_in*)&ifr_ip.ifr_addr)->sin_addr.s_addr;
    netmask_global = ((struct sockaddr_in*)&ifr_mask.ifr_netmask)->sin_addr.s_addr;

    struct sockaddr_ll sll;
    memset(&sll, 0, sizeof(sll));
    sll.sll_family = AF_PACKET;
    sll.sll_ifindex = ifindex;
    sll.sll_protocol = htons(ETH_P_ALL);

    if (bind(raw_fd, (struct sockaddr*)&sll, sizeof(sll)) < 0) {
        perror("bind");
        return 1;
    }

    int udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_fd < 0) {
        perror("udp socket");
        return 1;
    }
    SocketGuard udp_sock_guard(udp_fd);

    std::thread listener(packet_sniffer, raw_fd);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    uint32_t start_ip = ntohl(my_ip_global & netmask_global) + 1;
    uint32_t end_ip = ntohl(my_ip_global | ~netmask_global) - 1;

    std::cout << "جاري المسح والدعم عبر الواجهة: " << interface_name << "...\n";

    for (int pass = 1; pass <= 2 && keep_listening.load(); ++pass) {
        for (uint32_t ip = start_ip; ip <= end_ip && keep_listening.load(); ++ip) {
            uint32_t target_ip_net = htonl(ip);

            if (target_ip_net == my_ip_global) continue;

            unsigned char buffer[42];
            struct ether_header* eth = (struct ether_header*)buffer;
            struct arp_header* arp = (struct arp_header*)(buffer + sizeof(struct ether_header));

            memset(eth->ether_dhost, 0xff, 6);
            memcpy(eth->ether_shost, ifr.ifr_hwaddr.sa_data, 6);
            eth->ether_type = htons(ETH_P_ARP);

            arp->hardware_type = htons(1);
            arp->protocol_type = htons(ETH_P_IP);
            arp->hardware_len = 6;
            arp->protocol_len = 4;
            arp->opcode = htons(ARPOP_REQUEST);

            memcpy(arp->sender_mac, ifr.ifr_hwaddr.sa_data, 6);
            memcpy(arp->sender_ip, &my_ip_global, 4);
            memset(arp->target_mac, 0x00, 6);
            memcpy(arp->target_ip, &target_ip_net, 4);

            struct sockaddr_ll socket_address;
            memset(&socket_address, 0, sizeof(socket_address));
            socket_address.sll_family = AF_PACKET;
            socket_address.sll_ifindex = ifindex;
            socket_address.sll_halen = ETH_ALEN;
            memset(socket_address.sll_addr, 0xff, 6);

            sendto(raw_fd, buffer, sizeof(buffer), 0, (struct sockaddr*)&socket_address, sizeof(socket_address));

            trigger_udp_probe(udp_fd, target_ip_net, 53);
            trigger_udp_probe(udp_fd, target_ip_net, 137);
            trigger_udp_probe(udp_fd, target_ip_net, 5353);

            usleep(600);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(800));

    read_kernel_arp_cache();

    keep_listening.store(false);
    if (listener.joinable()) listener.join();

    std::cout << "\n=========================================\n";
    std::cout << "الأجهزة المكتشفة الموثوقة (" << discovered_devices.size() << " جهاز):\n";
    std::cout << "=========================================\n";
    std::cout << "IP Address\t\tMAC Address\n";
    std::cout << "-----------------------------------------\n";
    for (const auto& dev : discovered_devices) {
        std::cout << dev.ip_str << "\t\t" << dev.mac_str << "\n";
    }

    return 0;
}
