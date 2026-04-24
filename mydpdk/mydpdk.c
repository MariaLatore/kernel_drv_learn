#include <inttypes.h>
#include <netinet/in.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_mbuf.h>
#include <rte_udp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024
#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

static const struct rte_eth_conf port_conf_default = {0};

static void print_port_info(uint16_t portid) {
  struct rte_eth_dev_info dev_info;
  struct rte_ether_addr mac_addr;
  struct rte_eth_link link;
  char dev_name[RTE_ETH_NAME_MAX_LEN] = {0};

  memset(&dev_info, 0, sizeof(dev_info));
  memset(&link, 0, sizeof(link));

  if (rte_eth_dev_info_get(portid, &dev_info) != 0) {
    printf("port %u: failed to get device info\n", portid);
    return;
  }

  if (rte_eth_dev_get_name_by_port(portid, dev_name) != 0)
    snprintf(dev_name, sizeof(dev_name), "unknown");

  if (rte_eth_macaddr_get(portid, &mac_addr) != 0)
    memset(&mac_addr, 0, sizeof(mac_addr));

  rte_eth_link_get_nowait(portid, &link);

  printf("port %u info:\n", portid);
  printf("  device name : %s\n", dev_name);
  printf("  driver name : %s\n",
         dev_info.driver_name ? dev_info.driver_name : "unknown");
  printf("  if_index    : %u\n", dev_info.if_index);
  printf("  socket id   : %d\n", rte_eth_dev_socket_id(portid));
  printf("  MAC         : %02X:%02X:%02X:%02X:%02X:%02X\n",
         mac_addr.addr_bytes[0], mac_addr.addr_bytes[1], mac_addr.addr_bytes[2],
         mac_addr.addr_bytes[3], mac_addr.addr_bytes[4],
         mac_addr.addr_bytes[5]);
  printf("  link        : %s, speed %u Mbps\n",
         link.link_status ? "UP" : "DOWN", link.link_speed);
}

static int build_test_ipv4_udp_packet(struct rte_mbuf *m, uint16_t portid,
                                      uint16_t dst_portid) {
  struct rte_ether_hdr *eth_hdr;
  struct rte_ipv4_hdr *ip_hdr;
  struct rte_udp_hdr *udp_hdr;
  struct rte_ether_addr src_mac;
  struct rte_ether_addr dst_mac;
  char *payload;
  const char tag[] = "DPDKTEST";
  const uint16_t pkt_size = 64;
  const uint16_t eth_len = sizeof(struct rte_ether_hdr);
  const uint16_t ip_len = sizeof(struct rte_ipv4_hdr);
  const uint16_t udp_len = sizeof(struct rte_udp_hdr);
  const uint16_t payload_len = pkt_size - eth_len - ip_len - udp_len;

  payload = rte_pktmbuf_append(m, pkt_size);
  if (payload == NULL)
    return -1;

  memset(payload, 0, pkt_size);

  eth_hdr = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
  ip_hdr = (struct rte_ipv4_hdr *)(eth_hdr + 1);
  udp_hdr = (struct rte_udp_hdr *)(ip_hdr + 1);
  payload = (char *)(udp_hdr + 1);

  if (rte_eth_macaddr_get(portid, &src_mac) != 0)
    return -1;
  if (rte_eth_macaddr_get(dst_portid, &dst_mac) != 0)
    return -1;

  rte_ether_addr_copy(&dst_mac, &eth_hdr->dst_addr);
  rte_ether_addr_copy(&src_mac, &eth_hdr->src_addr);
  eth_hdr->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);

  ip_hdr->version_ihl = RTE_IPV4_VHL_DEF;
  ip_hdr->type_of_service = 0;
  ip_hdr->total_length =
      rte_cpu_to_be_16((uint16_t)(ip_len + udp_len + payload_len));
  ip_hdr->packet_id = 0;
  ip_hdr->fragment_offset = 0;
  ip_hdr->time_to_live = 64;
  ip_hdr->next_proto_id = IPPROTO_UDP;
  ip_hdr->hdr_checksum = 0;
  ip_hdr->src_addr = rte_cpu_to_be_32(RTE_IPV4(198, 18, 0, 1));
  ip_hdr->dst_addr = rte_cpu_to_be_32(RTE_IPV4(198, 18, 0, 2));
  ip_hdr->hdr_checksum = rte_ipv4_cksum(ip_hdr);

  udp_hdr->src_port = rte_cpu_to_be_16(9);
  udp_hdr->dst_port = rte_cpu_to_be_16(9);
  udp_hdr->dgram_len = rte_cpu_to_be_16((uint16_t)(udp_len + payload_len));
  udp_hdr->dgram_cksum = 0;

  memcpy(payload, tag, sizeof(tag) - 1);
  return 0;
}

static void send_one_test_packet(uint16_t portid, uint16_t dst_portid,
                                 struct rte_mempool *mbuf_pool) {
  struct rte_mbuf *m;
  uint16_t nb_tx;
  struct rte_ether_addr src_mac;
  struct rte_ether_addr dst_mac;
  const uint16_t pkt_size = 64;

  m = rte_pktmbuf_alloc(mbuf_pool);
  if (m == NULL) {
    printf("failed to alloc mbuf for test packet\n");
    return;
  }

  if (build_test_ipv4_udp_packet(m, portid, dst_portid) != 0) {
    printf("failed to build valid IPv4/UDP test packet\n");
    rte_pktmbuf_free(m);
    return;
  }

  if (rte_eth_macaddr_get(portid, &src_mac) != 0 ||
      rte_eth_macaddr_get(dst_portid, &dst_mac) != 0) {
    printf("failed to get MAC addresses for log output\n");
    rte_pktmbuf_free(m);
    return;
  }

  printf("TX port %u -> port %u, len=%u, "
         "src=%02X:%02X:%02X:%02X:%02X:%02X, "
         "dst=%02X:%02X:%02X:%02X:%02X:%02X, "
         "ethertype=0x%04X, l3=IPv4, l4=UDP, tag=DPDKTEST\n",
         portid, dst_portid, pkt_size, src_mac.addr_bytes[0],
         src_mac.addr_bytes[1], src_mac.addr_bytes[2], src_mac.addr_bytes[3],
         src_mac.addr_bytes[4], src_mac.addr_bytes[5], dst_mac.addr_bytes[0],
         dst_mac.addr_bytes[1], dst_mac.addr_bytes[2], dst_mac.addr_bytes[3],
         dst_mac.addr_bytes[4], dst_mac.addr_bytes[5], RTE_ETHER_TYPE_IPV4);

  nb_tx = rte_eth_tx_burst(portid, 0, &m, 1);
  if (nb_tx == 1) {
    printf("sent 1 test packet on port %u -> port %u\n", portid, dst_portid);
  } else {
    printf("failed to send test packet on port %u\n", portid);
    rte_pktmbuf_free(m);
  }
}

static void wait_for_link(uint16_t portid) {
  struct rte_eth_link link;
  int i;

  memset(&link, 0, sizeof(link));

  for (i = 0; i < 10; i++) {
    rte_eth_link_get_nowait(portid, &link);
    printf("port %u link check %d: %s, speed %u Mbps\n", portid, i,
           link.link_status ? "UP" : "DOWN", link.link_speed);
    if (link.link_status)
      return;
    sleep(1);
  }
}

static void print_port_stats(uint16_t portid) {
  struct rte_eth_stats stats;

  memset(&stats, 0, sizeof(stats));

  if (rte_eth_stats_get(portid, &stats) == 0) {
    printf("port %u stats: ipackets=%" PRIu64 ", opackets=%" PRIu64
           ", ibytes=%" PRIu64 ", obytes=%" PRIu64 ", ierrors=%" PRIu64
           ", oerrors=%" PRIu64 "\n",
           portid, stats.ipackets, stats.opackets, stats.ibytes, stats.obytes,
           stats.ierrors, stats.oerrors);
  } else {
    printf("failed to get stats for port %u\n", portid);
  }
}

static void send_burst_test_packets(uint16_t portid, uint16_t dst_portid,
                                    struct rte_mempool *mbuf_pool) {
  struct rte_mbuf *pkts[BURST_SIZE];
  struct rte_ether_addr src_mac, dst_mac;
  uint16_t i, nb_tx;
  const uint16_t pkt_size = 64;

  if (rte_eth_macaddr_get(portid, &src_mac) != 0) {
    printf("failed to get source MAC for port %u\n", portid);
    return;
  }

  if (rte_eth_macaddr_get(dst_portid, &dst_mac) != 0) {
    printf("failed to get destination MAC for port %u\n", dst_portid);
    return;
  }

  for (i = 0; i < BURST_SIZE; i++) {
    struct rte_mbuf *m;

    m = rte_pktmbuf_alloc(mbuf_pool);
    if (m == NULL) {
      printf("failed to alloc mbuf for burst packet %u\n", i);
      break;
    }

    if (build_test_ipv4_udp_packet(m, portid, dst_portid) != 0) {
      printf("failed to build valid IPv4/UDP packet %u\n", i);
      rte_pktmbuf_free(m);
      break;
    }

    pkts[i] = m;
  }

  if (i == 0)
    return;

  printf("TX burst port %u -> port %u, count=%u, len=%u, "
         "src=%02X:%02X:%02X:%02X:%02X:%02X, "
         "dst=%02X:%02X:%02X:%02X:%02X:%02X, "
         "ethertype=0x%04X, l3=IPv4, l4=UDP, tag=DPDKTEST\n",
         portid, dst_portid, i, pkt_size, src_mac.addr_bytes[0],
         src_mac.addr_bytes[1], src_mac.addr_bytes[2], src_mac.addr_bytes[3],
         src_mac.addr_bytes[4], src_mac.addr_bytes[5], dst_mac.addr_bytes[0],
         dst_mac.addr_bytes[1], dst_mac.addr_bytes[2], dst_mac.addr_bytes[3],
         dst_mac.addr_bytes[4], dst_mac.addr_bytes[5], RTE_ETHER_TYPE_IPV4);

  nb_tx = rte_eth_tx_burst(portid, 0, pkts, i);

  printf("sent burst on port %u -> port %u: requested=%u, sent=%u\n", portid,
         dst_portid, i, nb_tx);

  if (nb_tx < i) {
    uint16_t j;

    for (j = nb_tx; j < i; j++)
      rte_pktmbuf_free(pkts[j]);
  }
}

int main(int argc, char *argv[]) {
  int ret;
  uint16_t portid;
  unsigned nb_ports;
  struct rte_mempool *mbuf_pool;
  struct rte_eth_conf port_conf = port_conf_default;

  ret = rte_eal_init(argc, argv);
  if (ret < 0)
    rte_exit(EXIT_FAILURE, "Error: init dpdk environment error!\n");

  printf("dpdk init success\n");

  nb_ports = rte_eth_dev_count_avail();
  printf("Available ports: %u\n", nb_ports);

  if (nb_ports < 2)
    rte_exit(EXIT_FAILURE, "Error: at least 2 ports are required\n");

  for (portid = 0; portid < nb_ports; portid++)
    print_port_info(portid);

  mbuf_pool =
      rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS, MBUF_CACHE_SIZE, 0,
                              RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
  if (mbuf_pool == NULL)
    rte_exit(EXIT_FAILURE, "Error: cannot create mbuf pool\n");

  for (portid = 0; portid < nb_ports; portid++) {
    ret = rte_eth_dev_configure(portid, 1, 1, &port_conf);
    if (ret < 0)
      rte_exit(EXIT_FAILURE, "Error: cannot configure port %u\n", portid);

    ret =
        rte_eth_rx_queue_setup(portid, 0, RX_RING_SIZE,
                               rte_eth_dev_socket_id(portid), NULL, mbuf_pool);
    if (ret < 0)
      rte_exit(EXIT_FAILURE, "Error: cannot configure RX queue for port %u\n",
               portid);

    ret = rte_eth_tx_queue_setup(portid, 0, TX_RING_SIZE,
                                 rte_eth_dev_socket_id(portid), NULL);
    if (ret < 0)
      rte_exit(EXIT_FAILURE, "Error: cannot configure TX queue for port %u\n",
               portid);

    ret = rte_eth_dev_start(portid);
    if (ret < 0)
      rte_exit(EXIT_FAILURE, "Error: cannot start port %u\n", portid);

    rte_eth_promiscuous_enable(portid);
    wait_for_link(portid);
    print_port_info(portid);
  }

  printf("start transmit test: port 0 -> port 1\n");
  send_one_test_packet(0, 1, mbuf_pool);

  while (1) {
    struct rte_mbuf *bufs[BURST_SIZE];
    uint16_t nb_rx, i;

    send_one_test_packet(0, 1, mbuf_pool);
    printf("sent periodic test packets\n");
    print_port_stats(0);
    print_port_stats(1);
    fflush(stdout);

    nb_rx = rte_eth_rx_burst(1, 0, bufs, BURST_SIZE);
    if (nb_rx > 0) {
      for (i = 0; i < nb_rx; i++) {
        printf("received 1 packet on port 1, pkt_len=%u\n",
               rte_pktmbuf_pkt_len(bufs[i]));
        rte_pktmbuf_free(bufs[i]);
      }
    }

    sleep(1);
  }

  return 0;
}
