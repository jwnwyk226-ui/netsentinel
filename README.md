# complete_scanner extras

This folder contains helper utilities in C++ to complement `complete_scanner.cpp`:

- `device_names.cpp` — reverse DNS and simple NetBIOS (NBSTAT) probe to discover hostnames.
- `port_service_scanner.cpp` — TCP port scanner with banner grabbing.
- `monitor_visits.cpp` — passive capture (libpcap) of HTTP `Host` headers and TLS SNI for a chosen device.

Build (requires libpcap for `monitor_visits`):

```
g++ -std=c++17 -O2 -pthread complete_scanner.cpp -o complete_scanner
g++ -std=c++17 -O2 device_names.cpp -o device_names
g++ -std=c++17 -O2 port_service_scanner.cpp -o port_service_scanner
g++ -std=c++17 -O2 monitor_visits.cpp -lpcap -o monitor_visits
```

Usage examples:

- Discover hostnames:
  `./device_names 192.168.1.10 192.168.1.11`

- Port scan with banners:
  `./port_service_scanner 192.168.1.10 1 1000`

- Monitor what sites a device visits (requires root to capture on interface):
  `sudo ./monitor_visits wlp3s0 192.168.1.10`

Ethical and legal notice:

Only run these tools on networks and devices you own or have explicit permission to test. Passive and active scanning can be intrusive and may violate local policies or law if used without authorization.
