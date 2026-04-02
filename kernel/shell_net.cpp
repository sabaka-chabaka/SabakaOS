#include "shell_net.h"
#include "net.h"
#include "rtl8139.h"
#include "terminal.h"
#include "kstring.h"
#include "heap.h"

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
}

static void cmd_ping(const ShellArgs& args) {
    if (args.argc < 2) { terminal_puts("Usage: ping <ip>\n"); return; }
    if (!rtl8139_present()) { print_err("ping: no NIC"); return; }

    uint32_t dst = ip_from_str(args.argv[1]);
    if (dst == 0) { print_err("ping: bad IP"); return; }

    char ip_str[16];
    ip_to_str(dst, ip_str);
    terminal_puts("PING "); terminal_puts(ip_str); terminal_puts(":\n");

    uint8_t dst_mac[6];
    if (!net_arp_lookup(dst, dst_mac))
        kmemset(dst_mac, 0xFF, 6);

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
        {
            const uint16_t* p = (const uint16_t*)ip; uint32_t sum = 0;
            for (uint32_t k = 0; k < sizeof(IpHeader)/2; k++) sum += p[k];
            while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
            ip->checksum = (uint16_t)~sum;
        }

        IcmpHeader* icmp = (IcmpHeader*)(pkt + sizeof(EthHeader) + sizeof(IpHeader));
        icmp->type = ICMP_ECHO_REQUEST; icmp->code = 0;
        icmp->id = htons(0x5AB4); icmp->seq = htons((uint16_t)i);
        uint8_t* payload = (uint8_t*)icmp + sizeof(IcmpHeader);
        for (int j = 0; j < PAYLOAD_LEN; j++) payload[j] = (uint8_t)j;
        icmp->checksum = 0;
        {
            uint16_t icmp_len = (uint16_t)(sizeof(IcmpHeader) + PAYLOAD_LEN);
            const uint16_t* p = (const uint16_t*)icmp; uint32_t sum = 0;
            for (uint32_t k = 0; k < icmp_len/2; k++) sum += p[k];
            if (icmp_len & 1) sum += ((uint8_t*)icmp)[icmp_len-1];
            while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
            icmp->checksum = (uint16_t)~sum;
        }

        rtl8139_send(pkt, pkt_size);
        terminal_puts("  seq=");
        char buf[8]; kuitoa((uint32_t)i, buf, 10);
        terminal_puts(buf); terminal_puts(" sent\n");
        for (volatile int j = 0; j < 3000000; j++);
    }
    kfree(pkt);
    terminal_puts("ping: done (ICMP replies via IRQ)\n");
}

static void cmd_udpsend(const ShellArgs& args) {
    if (args.argc < 4) { terminal_puts("Usage: udpsend <ip> <port> <msg>\n"); return; }
    if (!rtl8139_present()) { print_err("udpsend: no NIC"); return; }

    uint32_t dst_ip   = ip_from_str(args.argv[1]);
    uint16_t dst_port = (uint16_t)katoi(args.argv[2]);
    const char* msg   = args.argv[3];
    uint16_t msg_len  = (uint16_t)kstrlen(msg);

    bool ok = net_udp_send(dst_ip, 12345, dst_port, (const uint8_t*)msg, msg_len);
    if (ok) {
        terminal_set_color_fg(10);
        terminal_puts("udpsend: sent "); char buf[8]; kuitoa(msg_len, buf, 10);
        terminal_puts(buf); terminal_puts(" bytes\n");
        terminal_reset_color();
    } else {
        print_err("udpsend: send failed (NIC error)");
    }
}

static void cmd_netstat(const ShellArgs&) {
    terminal_set_color_fg(11); terminal_puts("=== Network ===\n"); terminal_reset_color();
    terminal_puts("NIC:  RTL8139 ");
    terminal_puts(rtl8139_present() ? "[OK]\n" : "[NOT FOUND]\n");
    if (rtl8139_present()) {
        char ip[16]; ip_to_str(net_get_ip(), ip);
        terminal_puts("IP:   "); terminal_puts(ip); terminal_putchar('\n');
        terminal_puts("GW:   10.0.2.2 [52:55:0A:00:02:02] (SLIRP)\n");
    }
}

static void cmd_arp(const ShellArgs&) {
    terminal_puts("ARP cache:\n");
    terminal_puts("  10.0.2.2  -> 52:55:0A:00:02:02 (QEMU SLIRP gateway)\n");
    terminal_puts("  10.0.2.3  -> 52:55:0A:00:02:02 (QEMU SLIRP DNS)\n");
    terminal_puts("  (dynamic entries cached on receive)\n");
}

void shell_net_register() {
    shell_register("ifconfig", "ifconfig              Show network info",   cmd_ifconfig);
    shell_register("ping",     "ping <ip>             Send ICMP echo",      cmd_ping);
    shell_register("udpsend",  "udpsend <ip> <p> <m>  Send UDP packet",     cmd_udpsend);
    shell_register("netstat",  "netstat               Network status",      cmd_netstat);
    shell_register("arp",      "arp                   Show ARP cache",      cmd_arp);
}