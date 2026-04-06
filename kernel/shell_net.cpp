#include "shell_net.h"
#include "net.h"
#include "tcp.h"
#include "dns.h"
#include "udp_listen.h"
#include "rtl8139.h"
#include "terminal.h"
#include "kstring.h"
#include "heap.h"
#include "pit.h"

static void print_ok(const char* s) {
    terminal_set_color_fg(10); terminal_puts(s); terminal_puts("\n");
    terminal_reset_color();
}
static void print_err(const char* s) {
    terminal_set_color_fg(12); terminal_puts(s); terminal_puts("\n");
    terminal_reset_color();
}

static void cmd_ifconfig(const ShellArgs&) {
    if (!rtl8139_present()) { print_err("ifconfig: no NIC"); return; }
    uint8_t mac[6]; rtl8139_get_mac(mac);
    terminal_set_color_fg(11); terminal_puts("eth0:\n"); terminal_reset_color();
    terminal_puts("  MAC: ");
    for (int i = 0; i < 6; i++) {
        uint8_t b = mac[i];
        char tmp[3];
        tmp[0] = "0123456789ABCDEF"[b >> 4];
        tmp[1] = "0123456789ABCDEF"[b & 0xF];
        tmp[2] = 0;
        terminal_puts(tmp);
        if (i < 5) terminal_putchar(':');
    }
    terminal_putchar('\n');
    char ip_str[16];
    ip_to_str(net_get_ip(), ip_str);
    terminal_puts("  IP:  "); terminal_puts(ip_str); terminal_putchar('\n');
    terminal_puts("  GW:  10.0.2.2\n");
}

static void cmd_ping(const ShellArgs& args) {
    if (args.argc < 2) { terminal_puts("Usage: ping <ip>\n"); return; }
    if (!rtl8139_present()) { print_err("ping: no NIC"); return; }

    uint32_t dst = ip_from_str(args.argv[1]);
    if (!dst) { print_err("ping: bad IP"); return; }

    char ip_str[16]; ip_to_str(dst, ip_str);
    terminal_puts("PING "); terminal_puts(ip_str); terminal_puts(":\n");

    uint8_t dst_mac[6];
    if (!net_arp_lookup(dst, dst_mac)) kmemset(dst_mac, 0xFF, 6);

    const uint16_t PAYLOAD_LEN = 32;
    uint16_t ip_total = (uint16_t)(sizeof(IpHeader) + sizeof(IcmpHeader) + PAYLOAD_LEN);
    uint16_t pkt_size = (uint16_t)(sizeof(EthHeader) + ip_total);
    uint8_t* pkt = (uint8_t*)kmalloc(pkt_size);
    if (!pkt) { print_err("ping: OOM"); return; }

    for (int i = 0; i < 4; i++) {
        kmemset(pkt, 0, pkt_size);
        EthHeader* eth = (EthHeader*)pkt;
        kmemcpy(eth->dst, dst_mac, 6);
        net_get_mac(eth->src);
        eth->type = htons(ETH_TYPE_IP);

        IpHeader* ip = (IpHeader*)(pkt + sizeof(EthHeader));
        ip->ver_ihl = 0x45; ip->total_len = htons(ip_total);
        ip->id = htons((uint16_t)(200 + i)); ip->ttl = 64;
        ip->protocol = IP_PROTO_ICMP;
        ip->src_ip = net_get_ip(); ip->dst_ip = dst;
        ip->checksum = 0;
        { const uint16_t* p = (const uint16_t*)ip; uint32_t sum = 0;
          for (uint32_t k = 0; k < sizeof(IpHeader)/2; k++) sum += p[k];
          while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
          ip->checksum = (uint16_t)~sum; }

        IcmpHeader* icmp = (IcmpHeader*)(pkt + sizeof(EthHeader) + sizeof(IpHeader));
        icmp->type = ICMP_ECHO_REQUEST; icmp->code = 0;
        icmp->id = htons(0x5AB4); icmp->seq = htons((uint16_t)i);
        uint8_t* payload = (uint8_t*)icmp + sizeof(IcmpHeader);
        for (int j = 0; j < PAYLOAD_LEN; j++) payload[j] = (uint8_t)j;
        icmp->checksum = 0;
        { uint16_t icmp_len = (uint16_t)(sizeof(IcmpHeader) + PAYLOAD_LEN);
          const uint16_t* p = (const uint16_t*)icmp; uint32_t sum = 0;
          for (uint32_t k = 0; k < icmp_len/2; k++) sum += p[k];
          if (icmp_len & 1) sum += ((uint8_t*)icmp)[icmp_len-1];
          while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
          icmp->checksum = (uint16_t)~sum; }

        rtl8139_send(pkt, pkt_size);
        terminal_puts("  seq="); char buf[8]; kuitoa((uint32_t)i, buf, 10);
        terminal_puts(buf); terminal_puts(" sent\n");
        pit_sleep_ms(500);
    }
    kfree(pkt);
    terminal_puts("ping: done (ICMP replies processed via net_poll)\n");
}

static void cmd_udpsend(const ShellArgs& args) {
    if (args.argc < 4) { terminal_puts("Usage: udpsend <ip> <port> <msg>\n"); return; }
    if (!rtl8139_present()) { print_err("udpsend: no NIC"); return; }
    uint32_t dst_ip   = ip_from_str(args.argv[1]);
    uint16_t dst_port = (uint16_t)katoi(args.argv[2]);
    const char* msg   = args.argv[3];
    bool ok = net_udp_send(dst_ip, 12345, dst_port,
                           (const uint8_t*)msg, (uint16_t)kstrlen(msg));
    if (ok) print_ok("udpsend: sent"); else print_err("udpsend: failed");
}

static void cmd_netstat(const ShellArgs&) {
    terminal_set_color_fg(11); terminal_puts("=== Network ===\n"); terminal_reset_color();
    terminal_puts("NIC:  RTL8139 ");
    terminal_puts(rtl8139_present() ? "[OK]\n" : "[NOT FOUND]\n");
    if (rtl8139_present()) {
        char ip[16]; ip_to_str(net_get_ip(), ip);
        terminal_puts("IP:   "); terminal_puts(ip); terminal_putchar('\n');
        terminal_puts("GW:   10.0.2.2 (QEMU SLIRP)\n");
        terminal_puts("TCP:  active (IRQ queue + retransmit)\n");
    }
}

static void cmd_arp(const ShellArgs&) {
    terminal_puts("ARP cache:\n");
    terminal_puts("  10.0.2.2  -> 52:55:0A:00:02:02 (QEMU SLIRP gateway)\n");
    terminal_puts("  10.0.2.3  -> 52:55:0A:00:02:02 (QEMU SLIRP DNS)\n");
    terminal_puts("  (dynamic entries cached on receive)\n");
}

static void cmd_wget(const ShellArgs& args) {
    if (args.argc < 4) {
        terminal_puts("Usage: wget <ip> <port> <path>\n");
        terminal_puts("  Example: wget 10.0.2.2 80 /\n");
        return;
    }
    if (!rtl8139_present()) { print_err("wget: no NIC"); return; }

    uint32_t dst_ip   = ip_from_str(args.argv[1]);
    uint16_t dst_port = (uint16_t)katoi(args.argv[2]);
    const char* path  = args.argv[3];

    char ip_str[16]; ip_to_str(dst_ip, ip_str);
    terminal_puts("Connecting to ");
    terminal_puts(ip_str); terminal_putchar(':');
    char pbuf[8]; kuitoa(dst_port, pbuf, 10); terminal_puts(pbuf);
    terminal_puts(" ...\n");

    int sock = tcp_connect(dst_ip, dst_port, 5000);
    if (sock < 0) { print_err("wget: connect failed"); return; }
    print_ok("Connected.");

    const uint32_t REQ_MAX = 512;
    char* req = (char*)kmalloc(REQ_MAX);
    if (!req) { print_err("wget: OOM"); tcp_close(sock); return; }

    kstrcpy(req, "GET ");
    kstrcat(req, path);
    kstrcat(req, " HTTP/1.0\r\nHost: ");
    kstrcat(req, ip_str);
    kstrcat(req, "\r\nUser-Agent: SabakaOS/1.0\r\nConnection: close\r\n\r\n");

    uint16_t req_len = (uint16_t)kstrlen(req);
    if (tcp_send(sock, (const uint8_t*)req, req_len) < 0) {
        print_err("wget: send failed");
        kfree(req); tcp_close(sock); return;
    }
    kfree(req);

    const uint16_t RBUF = 512;
    uint8_t* buf = (uint8_t*)kmalloc(RBUF + 1);
    if (!buf) { print_err("wget: OOM"); tcp_close(sock); return; }

    bool     in_body    = false;
    uint32_t body_bytes = 0;

    uint8_t  window[4]  = {0, 0, 0, 0};
    uint32_t win_fill   = 0;

    terminal_set_color_fg(7);

    for (;;) {
        int n = tcp_recv_wait(sock, buf, RBUF, 200);

        if (n <= 0) {
            TcpState st = tcp_state(sock);
            if (st == TCP_CLOSE_WAIT || st == TCP_LAST_ACK ||
                st == TCP_CLOSED     || st == TCP_TIME_WAIT) {
                n = tcp_recv(sock, buf, RBUF);
                if (n <= 0) break;
            } else {
                continue;
            }
        }

        buf[n] = 0;

        if (in_body) {
            terminal_puts((const char*)buf);
            body_bytes += (uint32_t)n;
        } else {
            for (int i = 0; i < n; i++) {
                uint8_t c = buf[i];
                window[0] = window[1];
                window[1] = window[2];
                window[2] = window[3];
                window[3] = c;
                win_fill++;

                if (win_fill >= 4 &&
                    window[0] == '\r' && window[1] == '\n' &&
                    window[2] == '\r' && window[3] == '\n') {
                    in_body = true;
                    int body_start = i + 1;
                    int body_len   = n - body_start;
                    if (body_len > 0) {
                        buf[n] = 0;
                        terminal_puts((const char*)(buf + body_start));
                        body_bytes += (uint32_t)body_len;
                    }
                    break;
                }
            }
        }

        TcpState st = tcp_state(sock);
        if (st == TCP_CLOSED || st == TCP_TIME_WAIT) break;
    }

    terminal_reset_color();
    terminal_putchar('\n');

    kfree(buf);
    tcp_close(sock);

    terminal_set_color_fg(10);
    terminal_puts("\n[wget] ");
    char sbuf[16]; kuitoa(body_bytes, sbuf, 10);
    terminal_puts(sbuf); terminal_puts(" bytes received\n");
    terminal_reset_color();
}

static void cmd_tcpconnect(const ShellArgs& args) {
    if (args.argc < 3) {
        terminal_puts("Usage: tcpconnect <ip> <port>\n");
        return;
    }
    if (!rtl8139_present()) { print_err("tcpconnect: no NIC"); return; }

    uint32_t dst_ip   = ip_from_str(args.argv[1]);
    uint16_t dst_port = (uint16_t)katoi(args.argv[2]);

    char ip_str[16]; ip_to_str(dst_ip, ip_str);
    terminal_puts("Connecting to "); terminal_puts(ip_str);
    terminal_putchar(':'); char pbuf[8]; kuitoa(dst_port, pbuf, 10);
    terminal_puts(pbuf); terminal_puts(" ...\n");

    int sock = tcp_connect(dst_ip, dst_port, 5000);
    if (sock < 0) { print_err("tcpconnect: failed"); return; }
    print_ok("Connected. Reading for 5s...\n");

    const uint16_t RBUF = 256;
    uint8_t* buf = (uint8_t*)kmalloc(RBUF + 1);
    if (!buf) { tcp_close(sock); return; }

    uint32_t deadline = pit_uptime_ms() + 5000;
    while (pit_uptime_ms() < deadline) {
        int n = tcp_recv_wait(sock, buf, RBUF, 100);
        if (n > 0) { buf[n] = 0; terminal_puts((const char*)buf); }
        TcpState st = tcp_state(sock);
        if (st == TCP_CLOSED || st == TCP_TIME_WAIT) break;
    }

    kfree(buf);
    tcp_close(sock);
    terminal_puts("\n[connection closed]\n");
}

static void cmd_nslookup(const ShellArgs& args) {
    if (args.argc < 2) { terminal_puts("Usage: nslookup <hostname>\n"); return; }
    if (!rtl8139_present()) { print_err("nslookup: no NIC"); return; }

    const char* host = args.argv[1];
    terminal_puts("Resolving: "); terminal_puts(host); terminal_puts(" ...\n");

    uint32_t ip = dns_resolve(host);
    if (!ip) {
        print_err("nslookup: failed (NXDOMAIN or timeout)");
        return;
    }
    char ip_str[16]; ip_to_str(ip, ip_str);
    terminal_set_color_fg(10);
    terminal_puts(host); terminal_puts(" -> "); terminal_puts(ip_str);
    terminal_putchar('\n');
    terminal_reset_color();
}

static void cmd_dnscache(const ShellArgs&) {
    int n = dns_cache_count();
    if (n == 0) { terminal_puts("DNS cache is empty\n"); return; }
    terminal_set_color_fg(11); terminal_puts("DNS cache:\n"); terminal_reset_color();
    uint32_t now = pit_uptime_ms();
    for (int i = 0; i < DNS_CACHE_SIZE; i++) {
        const DnsCacheEntry* e = dns_cache_get(i);
        if (!e || !e->valid) continue;
        if (now >= e->ttl_deadline_ms) continue;
        char ip_str[16]; ip_to_str(e->ip, ip_str);
        terminal_puts("  "); terminal_puts(e->name);
        terminal_puts(" -> "); terminal_puts(ip_str);
        char tbuf[12];
        uint32_t ttl_left = (e->ttl_deadline_ms - now) / 1000;
        kuitoa(ttl_left, tbuf, 10);
        terminal_puts(" (TTL "); terminal_puts(tbuf); terminal_puts("s)\n");
    }
}

static void cmd_udplisten(const ShellArgs& args) {
    if (args.argc < 2) {
        terminal_puts("Usage: udplisten <port> [timeout_sec]\n");
        return;
    }
    if (!rtl8139_present()) { print_err("udplisten: no NIC"); return; }

    uint16_t port     = (uint16_t)katoi(args.argv[1]);
    uint32_t timeout  = args.argc >= 3 ? (uint32_t)katoi(args.argv[2]) : 10;
    uint32_t timeout_ms = timeout * 1000;

    int handle = udpl_open(port);
    if (handle < 0) { print_err("udplisten: no free listener slots"); return; }

    terminal_set_color_fg(11);
    terminal_puts("Listening on UDP port ");
    char pbuf[8]; kuitoa(port, pbuf, 10); terminal_puts(pbuf);
    terminal_puts(" (");
    kuitoa(timeout, pbuf, 10); terminal_puts(pbuf);
    terminal_puts("s, Ctrl+C not supported — will auto-stop)\n");
    terminal_reset_color();

    uint32_t deadline = pit_uptime_ms() + timeout_ms;
    uint32_t count    = 0;

    while (pit_uptime_ms() < deadline) {
        UdpPacket pkt;
        if (udpl_recv_wait(handle, &pkt, 500)) {
            count++;
            char src[16]; ip_to_str(pkt.src_ip, src);
            terminal_set_color_fg(14);
            terminal_puts("[UDP] from ");
            terminal_puts(src); terminal_putchar(':');
            char spbuf[8]; kuitoa(pkt.src_port, spbuf, 10);
            terminal_puts(spbuf);
            terminal_puts(" ("); kuitoa(pkt.len, spbuf, 10);
            terminal_puts(spbuf); terminal_puts(" bytes): ");
            terminal_reset_color();

            // Вывод данных: печатаем как текст, непечатные — точка
            uint16_t show = pkt.len < 128 ? pkt.len : 128;
            for (uint16_t i = 0; i < show; i++) {
                char c = (char)pkt.data[i];
                terminal_putchar(c >= 32 && c < 127 ? c : '.');
            }
            if (pkt.len > 128) terminal_puts("...");
            terminal_putchar('\n');
        }
    }

    udpl_close(handle);
    terminal_set_color_fg(10);
    terminal_puts("[udplisten] stopped. Received ");
    char cbuf[12]; kuitoa(count, cbuf, 10);
    terminal_puts(cbuf); terminal_puts(" packets\n");
    terminal_reset_color();
}

static void cmd_wgetd(const ShellArgs& args) {
    if (args.argc < 4) {
        terminal_puts("Usage: wgetd <hostname> <port> <path>\n");
        terminal_puts("  Example: wgetd example.com 80 /\n");
        return;
    }
    if (!rtl8139_present()) { print_err("wgetd: no NIC"); return; }

    const char* host = args.argv[1];
    uint32_t ip = dns_resolve(host);
    if (!ip) { print_err("wgetd: DNS resolve failed"); return; }

    char ip_str[16]; ip_to_str(ip, ip_str);
    terminal_puts("Resolved "); terminal_puts(host);
    terminal_puts(" -> "); terminal_puts(ip_str); terminal_putchar('\n');

    char line[256];
    kstrcpy(line, "wget ");
    kstrcat(line, ip_str); kstrcat(line, " ");
    kstrcat(line, args.argv[2]); kstrcat(line, " ");
    kstrcat(line, args.argv[3]);
    shell_execute(line);
}

void shell_net_register() {
    shell_register("ifconfig",   "ifconfig              Show network info",     cmd_ifconfig);
    shell_register("ping",       "ping <ip>             Send ICMP echo",        cmd_ping);
    shell_register("udpsend",    "udpsend <ip> <p> <m>  Send UDP packet",       cmd_udpsend);
    shell_register("netstat",    "netstat               Network status",        cmd_netstat);
    shell_register("arp",        "arp                   Show ARP cache",        cmd_arp);
    shell_register("wget",       "wget <ip> <p> <path>  HTTP GET request",      cmd_wget);
    shell_register("tcpconnect", "tcpconnect <ip> <p>   Raw TCP connect+recv",  cmd_tcpconnect);
    shell_register("nslookup",   "nslookup <host>       DNS resolve",             cmd_nslookup);
    shell_register("dnscache",   "dnscache              Show DNS cache",           cmd_dnscache);
    shell_register("udplisten",  "udplisten <p> [sec]   Listen on UDP port",       cmd_udplisten);
    shell_register("wgetd",      "wgetd <host> <p> <path> HTTP GET with DNS",     cmd_wgetd);
}