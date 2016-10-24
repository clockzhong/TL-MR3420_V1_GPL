/*
 *	Forwarding decision
 *	Linux ethernet bridge
 *
 *	Authors:
 *	Lennert Buytenhek		<buytenh@gnu.org>
 *
 *	$Id: //depot/sw/releases/7.3_AP/linux/kernels/mips-linux-2.6.15/net/bridge/br_forward.c#2 $
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/netfilter_bridge.h>
#include "br_private.h"

#define WLAN_DEV_NAME_PRE "ath"	//atheros wlan device name
static inline int should_deliver(const struct net_bridge_port *p, 
				 const struct sk_buff *skb)
{
	if (skb->dev == p->dev ||
	    p->state != BR_STATE_FORWARDING)
		return 0;
	
	//added by yzt for vaps isolation, 2010-05-12
	if(wlanwds_enable)
	{
		return 1;
	}
	if ((0 == strncmp(skb->dev->name, WLAN_DEV_NAME_PRE, strlen(WLAN_DEV_NAME_PRE))) && 
		(0 == strncmp(p->dev->name, WLAN_DEV_NAME_PRE, strlen(WLAN_DEV_NAME_PRE))))
	{
		//printk("drop packets transmitted between vaps\n");
		return 0;
	}
	
	return 1;
}

int br_dev_queue_push_xmit(struct sk_buff *skb)
{
	/* drop mtu oversized packets except tso */
	if (skb->len > (skb->dev->mtu + ((skb->protocol == ETH_P_8021Q) ? 4 : 0) ) && !skb_shinfo(skb)->tso_size)
		kfree_skb(skb);
	else {
#ifdef CONFIG_BRIDGE_NETFILTER
		/* ip_refrag calls ip_fragment, doesn't copy the MAC header. */
		nf_bridge_maybe_copy_header(skb);
#endif
		skb_push(skb, ETH_HLEN);

		dev_queue_xmit(skb);
	}

	return 0;
}

int br_forward_finish(struct sk_buff *skb)
{
	NF_HOOK(PF_BRIDGE, NF_BR_POST_ROUTING, skb, NULL, skb->dev,
			br_dev_queue_push_xmit);

	return 0;
}

static void __br_deliver(const struct net_bridge_port *to, struct sk_buff *skb)
{
	skb->dev = to->dev;
	NF_HOOK(PF_BRIDGE, NF_BR_LOCAL_OUT, skb, NULL, skb->dev,
			br_forward_finish);
}

static void __br_forward(const struct net_bridge_port *to, struct sk_buff *skb)
{
	struct net_device *indev;

	indev = skb->dev;
	skb->dev = to->dev;
	skb->ip_summed = CHECKSUM_NONE;

	NF_HOOK(PF_BRIDGE, NF_BR_FORWARD, skb, indev, skb->dev,
			br_forward_finish);
}

/* called with rcu_read_lock */
void br_deliver(const struct net_bridge_port *to, struct sk_buff *skb)
{
	if (should_deliver(to, skb)) {
		__br_deliver(to, skb);
		return;
	}

	kfree_skb(skb);
}

/* called with rcu_read_lock */
void br_forward(const struct net_bridge_port *to, struct sk_buff *skb)
{
	if (should_deliver(to, skb)) {
		__br_forward(to, skb);
		return;
	}

	kfree_skb(skb);
}

/* called under bridge lock */
static void br_flood(struct net_bridge *br, struct sk_buff *skb, int clone,
	void (*__packet_hook)(const struct net_bridge_port *p, 
			      struct sk_buff *skb))
{
	struct net_bridge_port *p;
	struct net_bridge_port *prev;

	if (clone) {
		struct sk_buff *skb2;

		if ((skb2 = skb_clone(skb, GFP_ATOMIC)) == NULL) {
			br->statistics.tx_dropped++;
			return;
		}

		skb = skb2;
	}

	prev = NULL;

#ifdef CONFIG_TP_MULTICAST			
#define IS_MULTICAST_ADDR(ptr)  ((ptr[0] == 0x01) && (ptr[1] == 0x00) && (ptr[2] == 0x5e) ? 1 : 0)
	mac_addr multi_mac_addr;
	unsigned char *pmac = multi_mac_addr.addr;
	memset(pmac, 0, 6/*ETH_ALEN*/);

	if(IS_MULTICAST_ADDR(skb->mac.raw))
	{
		/*backup multicast address*/
		memcpy(pmac, skb->mac.raw, 6/*ETH_ALEN*/);
	}
#endif

	list_for_each_entry_rcu(p, &br->port_list, list) {
		if (should_deliver(p, skb)) {
			if (prev != NULL) {
				struct sk_buff *skb2;

				if ((skb2 = skb_clone(skb, GFP_ATOMIC)) == NULL) {
					br->statistics.tx_dropped++;
					kfree_skb(skb);
					return;
				}
#ifdef CONFIG_TP_MULTICAST				
				if(IS_MULTICAST_ADDR(pmac))
				{
					/*restore multicast address*/
					memcpy(skb2->mac.raw, pmac, 6/*ETH_ALEN*/);
				}
#endif
				__packet_hook(prev, skb2);
			}

			prev = p;
		}
	}

	if (prev != NULL) {

#ifdef CONFIG_TP_MULTICAST				
		if(IS_MULTICAST_ADDR(pmac))
		{
			/*restore multicast address*/
			memcpy(skb->mac.raw, pmac, 6/*ETH_ALEN*/);
		}
#endif
		
		__packet_hook(prev, skb);
		return;
	}

	kfree_skb(skb);
}


/* called with rcu_read_lock */
void br_flood_deliver(struct net_bridge *br, struct sk_buff *skb, int clone)
{
	br_flood(br, skb, clone, __br_deliver);
}

/* called under bridge lock */
void br_flood_forward(struct net_bridge *br, struct sk_buff *skb, int clone)
{
	br_flood(br, skb, clone, __br_forward);
}
