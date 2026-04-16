#include <linux/etherdevice.h>
#include <linux/inet.h>
#include <linux/ip.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/skbuff.h>

static char *in_ifname = "veth_in_root";
static char *out_ifname = "veth_out_root";

static __be32 target_ip;

static unsigned char src_mac[ETH_ALEN] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x11};
static unsigned char dst_mac[ETH_ALEN] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x22};

static unsigned int hook_func_tunnel_in(void *priv, struct sk_buff *skb,
                                        const struct nf_hook_state *state) {
  struct iphdr *ip_header;
  struct net_device *dev;
  struct net_device *in;
  struct ethhdr *eth;
  // Check if the packet has IP header
  if (!skb || !state || !state->in)
    return NF_ACCEPT;

  if (!pskb_may_pull(skb, sizeof(struct iphdr)))
    return NF_ACCEPT;

  ip_header = ip_hdr(skb);
  in = state->in;

  // Check if the packet is IPv4
  if (!ip_header || ip_header->version != 4)
    return NF_ACCEPT;

  pr_info("hook: in=%s len=%u\n", in->name, skb->len);
  pr_info("hook: daddr=%pI4\n", &ip_header->daddr);

  // Check if the packet is received from in_ifname and destination IP is ip1
  if (in && strcmp(in->name, in_ifname) == 0 && ip_header->daddr == target_ip) {

    // Enqueue the packet for transmission on eth1
    dev = dev_get_by_name(state->net, out_ifname);
    if (!dev)
      return NF_ACCEPT;

    if (skb_cow_head(skb, ETH_HLEN) < 0) {
      dev_put(dev);
      return NF_ACCEPT;
    }
    // Perform tunnling
    skb_push(skb, ETH_HLEN);
    skb_reset_mac_header(skb);

    eth = eth_hdr(skb);
    ether_addr_copy(eth->h_dest, dst_mac);
    ether_addr_copy(eth->h_source, src_mac);
    eth->h_proto = htons(ETH_P_IP);

    skb->dev = dev;
    skb->protocol = htons(ETH_P_IP);
    skb->pkt_type = PACKET_OUTGOING;

    dev_queue_xmit(skb);
    dev_put(dev);
    return NF_STOLEN; // Indicate that we have taken ownership of the packet
  }

  return NF_ACCEPT; // Allow the packet to proceed normally
}

// Netfilter hook structure
static struct nf_hook_ops nfho_tunnel_in = {
    .hook = hook_func_tunnel_in,
    .hooknum = NF_INET_PRE_ROUTING, // Use NF_INET_PRE_ROUTING for IPv4
    .pf = NFPROTO_IPV4,             // Use NFPROTO_IPV4 for IPv4
    .priority = NF_IP_PRI_FIRST,
};

// Module initialization function
static int __init nf_hook_init(void) {
  int ret;
  target_ip = in_aton("10.0.10.1");

  // Register the Netfilter hook
  ret = nf_register_net_hook(&init_net, &nfho_tunnel_in);
  if (ret < 0) {
    pr_err("Failed to register NEtfilter hook\n");
    return ret;
  }
  pr_info("NF_HOOK:Module installed\n");
  pr_info("NF_HOOK:in_if=%s out_if=%s target_ip=10.0.10.1\n", in_ifname,
          out_ifname);
  return 0;
}

// Module clean up function
static void __exit nf_hook_exit(void) {
  // Unregister the Netfilter hook
  nf_unregister_net_hook(&init_net, &nfho_tunnel_in);
  pr_info("NF_HOOK:Module removed\n");
}

module_init(nf_hook_init);
module_exit(nf_hook_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("BPB");
MODULE_DESCRIPTION("Netfilter hook example");
