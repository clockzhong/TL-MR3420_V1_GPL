/*!Copyright(c) 2008-2010 Shenzhen TP-LINK Technologies Co.Ltd.
 *
 *\file	tp_mroute.c
 *\brief	IGMP & Mcast UDP diliver module. 
 *
 *\author	Wang Wenhao
 *\version	1.0.1
 *\date	05Jan10
 *
 *\history \arg 1.0.2, 06May10, Yin Zhongtao，增加对WLAN的组播转发支持，修复WAN口QUERY问题，
                                              优化了组播数据转发效率

 		\arg 1.0.1, 05Jan10, Wang Wenhao, 将IGMP包和UDP转发分开发送，解决UDP可能找不到路由的问题.
 *											通过查找静态路由获取dst，解决可能出现的kernel panic. 
 * 				    
 *		\arg 1.0.0, ???, Wang Wenhao, Create the file.
 */

#include <linux/types.h>
#include <linux/tp_mroute.h>
#include <linux/igmp.h>
#include <linux/netdevice.h>
#include <net/ip.h>

//#define TEST_DEBUG
#ifdef TEST_DEBUG
#define debugk(x) printk(x)
#else
#define debugk(X)
#endif

#define IGMP_SIZE (sizeof(struct igmphdr)+sizeof(struct iphdr)+4)

int initialed = 0;

struct net_device *br0_dev = NULL;
struct net_device *eth0_dev = NULL;
struct net_device *eth1_dev = NULL;

int br0_index, eth0_index, eth1_index;

struct mc_table table;

/*
 * 	create IGMP packet's IP header
 */
static struct sk_buff *create_igmp_ip_skb(struct net_device *dev, __u32 daddr)
{
    struct sk_buff *skb;
    struct iphdr *iph;
//  struct in_device *in_dev;
	struct rtable *rt;

	//copy from igmp.c
    struct flowi fl = { .oif = dev->ifindex,
                .nl_u = { .ip4_u = { .daddr = daddr } },
                .proto = IPPROTO_IGMP };

	//eth0 don't have static route, use br0 instead
    if (dev->ifindex == eth0_index)
    {
        fl.oif = br0_index;
    }
    
    if (ip_route_output_key(&rt, &fl))
    {
        printk("Ooops, static route igmp error!\n");
//		ip_rt_put(rt);
        return NULL;
    }
    
    if (rt->rt_src == 0)
    {
        debugk("no igmp route source\n");
//      ip_rt_put(rt);
		return NULL;
	}

    skb = alloc_skb(IGMP_SIZE+LL_RESERVED_SPACE(dev), GFP_ATOMIC);
	if (skb == NULL)
    {
//		ip_rt_put(rt);
		return NULL;
	}

	skb->dst = &rt->u.dst;

	skb_reserve(skb, LL_RESERVED_SPACE(dev));

	skb->nh.iph = iph = (struct iphdr *)skb_put(skb, sizeof(struct iphdr) + 4);

	iph->version  = 4;
	iph->ihl      = (sizeof(struct iphdr)+4) >> 2;
	iph->tos      = 0xc0;
	iph->frag_off = htons(IP_DF);
	iph->ttl      = 1;
	iph->daddr    = daddr;
	iph->saddr    = rt->rt_src;
#ifdef TEST_DEBUG
	printk("saddr = %x; daddr = %x; oif = %s\n", iph->saddr, iph->daddr, skb->dst->dev->name);
#endif
/*
//	get in_device for ipaddr, eth0 may have no ipaddr, use br0 instead 
    if (dev->ifindex == eth0_dev->ifindex)
    {
        in_dev    = __in_dev_get_rtnl(br0_dev);
    }
    else
    {
        in_dev    = __in_dev_get_rtnl(dev);
    }
//	our in_device only have one ipaddr, so use the list's first node
    if (in_dev->ifa_list != NULL)
    {
        iph->saddr    = in_dev->ifa_list->ifa_local;
    }
//	printk("daddr = %d\n", iph->saddr);
*/
	iph->protocol = IPPROTO_IGMP;
	iph->tot_len  = htons(IGMP_SIZE);
	ip_select_ident(iph, &rt->u.dst, NULL);
//  ip_select_ident(iph, NULL, NULL);	//not used
	((u8*)&iph[1])[0] = IPOPT_RA;
	((u8*)&iph[1])[1] = 4;
	((u8*)&iph[1])[2] = 0;
	((u8*)&iph[1])[3] = 0;
	ip_send_check(iph);

    return skb;
}

/*
 *	fulfill IGMP report header and send 
 */
static int tp_send_igmp_report(struct net_device *dev, __u32 daddr)
{
    struct sk_buff *skb;
    struct igmphdr *ih;
    
    if ((skb = create_igmp_ip_skb(dev, daddr)) == NULL)
    {
        return -1;
    }

	ih = (struct igmphdr *)skb_put(skb, sizeof(struct igmphdr));
	ih->type = IGMPV2_HOST_MEMBERSHIP_REPORT;
	ih->code = 0;
	ih->csum = 0;
	ih->group = daddr;
	ih->csum = ip_compute_csum((void *)ih, sizeof(struct igmphdr));

    return ip_output(skb);
}

/*
 *	fulfill IGMP query header and send 
 */
static int tp_send_igmp_query(struct net_device *dev, __u32 daddr, __u32 group, __u8 delay)
{
    struct sk_buff *skb;
    struct igmphdr *ih;
    
    if ((skb = create_igmp_ip_skb(dev, daddr)) == NULL)
    {
        debugk("create IGMP pack failed\n");
        return -1;
    }

	ih = (struct igmphdr *)skb_put(skb, sizeof(struct igmphdr));
	ih->type = IGMP_HOST_MEMBERSHIP_QUERY;
	ih->code = delay;
	ih->csum = 0;
	ih->group = group;
	ih->csum = ip_compute_csum((void *)ih, sizeof(struct igmphdr));

    return ip_output(skb);
}

/*
 *	fulfill IGMP leave header and send 
 */
static int tp_send_igmp_leave(struct net_device *dev, __u32 group)
{
    struct sk_buff *skb;
    struct igmphdr *ih;
    
    if ((skb = create_igmp_ip_skb(dev, IGMP_ALL_ROUTER_ADDR)) == NULL)
    {
        return -1;
    }

	ih = (struct igmphdr *)skb_put(skb, sizeof(struct igmphdr));
	ih->type = IGMP_HOST_LEAVE_MESSAGE;
	ih->code = 0;
	ih->csum = 0;
	ih->group = group;
	ih->csum = ip_compute_csum((void *)ih, sizeof(struct igmphdr));

    return ip_output(skb);

}

/*
 *	send general query only
 */
static void general_query_send(char *dev_name)
{
    debugk("send general query\n");

	br0_dev = dev_get_by_name(dev_name);
    
    if (tp_send_igmp_query(br0_dev, IGMP_ALL_HOST_ADDR, 0, QUERY_RESPONSE_INTERVAL_NUM * 10))
    {
        printk("Ooops, general query send failed!\n");
    }

	dev_put(br0_dev);
	
}

/*
 *	g-q timer expired, send general query & reset timer
 */
static void general_query_timer_expired(unsigned long data)
{
    debugk("g-q tiemer expired\n");

	br0_dev = dev_get_by_name("br0");
    
    if (tp_send_igmp_query(br0_dev, IGMP_ALL_HOST_ADDR, 0, QUERY_RESPONSE_INTERVAL_NUM * 10))
    {
        printk("Ooops, general query send failed!\n");
    }

	dev_put(br0_dev);
    
    table.generl_query_timer.expires = jiffies + QUERY_INTERVAL;
    add_timer(&table.generl_query_timer);
}

/*
 *	r-c timer expired, check all items, remove the unreported one
 */
static void report_checking_timer_expired(unsigned long data)
{
    int i;

    debugk("r-c timer expired\n");
    struct mc_entry *mcp;

    for (i = 0; i < MAX_GROUP_ENTRIES; i++)
    {
        mcp = &table.entry[i];
		//if mcp->mc_addr is 0, it must not be used
        if (!mcp->reported && mcp->mc_addr)
        {
			//the timer of the removing item must be stopped
            if (timer_pending(&mcp->wan_qr_timer))
            {
                del_timer(&mcp->wan_qr_timer);
            }

            spin_lock_bh(table.lock);
            __hlist_del((struct hlist_node *)mcp);
            hlist_add_head((struct hlist_node *)mcp, &table.empty_list);
            table.groups--;
            spin_unlock_bh(table.lock);

			eth1_dev = dev_get_by_name("eth1");
            tp_send_igmp_leave(eth1_dev, mcp->mc_addr);
			dev_put(eth1_dev);
			
            mcp->mc_addr = 0;
			if(mcp->dst)
			{
				dst_release(mcp->dst);
				mcp->dst = NULL;
			}

            debugk("del some tips from mc_table\n");
        }
		// clear the report flag
        mcp->reported = 0;
    }

	// if no tips alive, stop all function
    if (!table.groups)
    {
		//keep g-q timer, yzt 2010-02-21
        //del_timer(&table.generl_query_timer);
        if (timer_pending(&table.report_checking_timer))
        {
            del_timer(&table.report_checking_timer);
        }

        return;
    }
    
    table.report_checking_timer.expires = jiffies + GROUP_MEMBER_SHIP_INTERVAL;
    add_timer(&table.report_checking_timer);
}

/*
 *	wan q-r timer expired, send report to wan port
 */
static void wan_qr_timer_expired(unsigned long data)
{
    int i = (int) data;

    debugk("wan q-r timer expired\n");
    struct mc_entry *mcp = &table.entry[i];

	eth1_dev = dev_get_by_name("eth1");
    tp_send_igmp_report(eth1_dev, mcp->mc_addr);
	dev_put(eth1_dev);
}

/*
 * initaily get the device pointer
 */
static int update_dev_index(void)
{
    br0_dev = dev_get_by_name("br0");
    eth0_dev = dev_get_by_name("eth0");
    eth1_dev = dev_get_by_name("eth1");

    if (br0_dev && eth0_dev && eth1_dev)
    {
		br0_index = br0_dev->ifindex;
		eth0_index = eth0_dev->ifindex;
		eth1_index = eth1_dev->ifindex;

		dev_put(eth0_dev);
		dev_put(eth1_dev);
		dev_put(br0_dev);
		
        return 0;
    }
    else
    {
        return -1;
    }
}

/*
 * find the items by the muticast group
 */
struct mc_entry *hlist_sort(__u32 group)
{
    struct hlist_node *hnp;
    struct mc_entry *mcp;
    
    for (hnp = table.mc_hash[HASH256(group)].first; hnp != NULL; hnp = hnp->next)
    {
        mcp = (struct mc_entry *)hnp;
        if (mcp->mc_addr == group)
        {
            return mcp;
        }
    }
    return NULL;
}

/*
 * initail the mc table, zero the muticast group, define the timer expire function
 */
int mc_table_init(void)
{
    int i;

    debugk("start initial\n");
    if (update_dev_index())
    {
        printk("Ooops, why the devices couldn't been initialed?\n");
        return -1;
    }
    
    for (i = 0; i < MAX_HASH_ENTRIES; i++)
    {
        table.mc_hash[i].first = NULL;
    }
    
    for (i = MAX_GROUP_ENTRIES - 1; i >= 0; i--)
    {
        hlist_add_head((struct hlist_node *)&table.entry[i], &table.empty_list);
        init_timer(&table.entry[i].wan_qr_timer);
        table.entry[i].wan_qr_timer.function = wan_qr_timer_expired;
        table.entry[i].wan_qr_timer.data = (unsigned long)i;
        table.entry[i].membership_flag = 0;
        table.entry[i].reported = 0;
        table.entry[i].mc_addr = 0;
		table.entry[i].dst = NULL;
    }
    init_timer(&table.generl_query_timer);
    table.generl_query_timer.function = general_query_timer_expired;
	
	//start g-q timer here, yzt 2010-02-21
	table.generl_query_timer.expires = jiffies + QUERY_INTERVAL;
	add_timer(&table.generl_query_timer);

    init_timer(&table.report_checking_timer);
    table.report_checking_timer.function = report_checking_timer_expired;
    table.groups = 0;
    spin_lock_init(&table.lock);

    initialed = 1;
    return 0;
}

/*
 * calssify the muticast packet, deliver to different function
 */
int tp_mr_classify(struct sk_buff *skb)
{
    struct iphdr *iph = skb->nh.iph;
    __u32 daddr = iph->daddr;

	//init table is not initialed
    if (!initialed)
    {
        if (mc_table_init())
        {
            goto drop;
        }
    }

    if (!MULTICAST(iph->daddr))
    {
        goto drop;
    }

	//deliver WAN data packet
    if (iph->protocol == IPPROTO_UDP)
    {
        if (skb->dev->ifindex == eth1_index)
        {
            return find_data_path(skb, iph->daddr);
        }
        else
        {
            debugk("drop lan data pack\n");
            goto drop;
        }
    }

	// to IP header
	__skb_pull(skb, skb->nh.iph->ihl*4);
    skb->h.raw = skb->data;

    struct igmphdr *ih = skb->h.igmph;

//  printk("ih type=%d daddr=%d group=%d\n", ih->type, daddr, ih->group);

    if (iph->protocol == IPPROTO_IGMP)
    {
        debugk("heard IGMP pack\n");

		//copy from igmp.c, drop broken packet
        if (!pskb_may_pull(skb, sizeof(struct igmphdr)))
        {
            goto drop;
        }

        if (skb->dev->ifindex == br0_index)
        {
            debugk("heard lan IGMP pack\n");
            switch (ih->type)
            {
            case IGMP_HOST_MEMBERSHIP_REPORT:
            case IGMPV2_HOST_MEMBERSHIP_REPORT:
                return lan_heard_igmp_report(skb, ih->group);
                break;
			case IGMPV3_HOST_MEMBERSHIP_REPORT:
				/*send v2 query to br0*/
				debugk("heard IGMP v3 report\n");
				general_query_send("br0");
				break;
                    
            case IGMP_HOST_LEAVE_MESSAGE:
                return lan_heard_igmp_leave(skb, ih->group);
                break;
                
            default:
                break;
            }
        }
        else if (skb->dev->ifindex == eth1_index)
        {
            debugk("heard wan IGMP pack\n");
            switch (ih->type)
            {
            case IGMP_HOST_MEMBERSHIP_REPORT:
            case IGMPV2_HOST_MEMBERSHIP_REPORT:
                return wan_heard_igmp_report(skb, ih->group);
                break;
                    
            case IGMP_HOST_MEMBERSHIP_QUERY:
                return wan_heard_igmp_query(skb, ih, daddr);
                break;
                
            default:
                break;
            }
            
        }
    }

drop:
    kfree_skb(skb);
    return -1;
}

/*
 * solve igmp report packet, may add/modify items
 */
int lan_heard_igmp_report(struct sk_buff *skb, __u32 group)
{
    struct mc_entry *mcp;

    debugk("heard lan igmp report\n");
    if (!table.groups)
    {
		//move to mc_table_init(), yzt 2010-02-21
        //table.generl_query_timer.expires = jiffies + QUERY_INTERVAL;
        //add_timer(&table.generl_query_timer);
        table.report_checking_timer.expires = jiffies + GROUP_MEMBER_SHIP_INTERVAL;
        add_timer(&table.report_checking_timer);
    }
    else if ((mcp = hlist_sort(group)) != NULL)
    {
		//modify port infomation
        mcp->membership_flag |= skb->nfmark;
        mcp->reported = 1;
        goto end;
    }
    else if (table.groups == MAX_GROUP_ENTRIES)
    {
        printk("Ooops, groups full, why defines so little table?\n");
        goto end;
    }
    
    mcp = (struct mc_entry *)table.empty_list.first;
    mcp->mc_addr = group;
	//save port information
    mcp->membership_flag = skb->nfmark;
    mcp->reported = 1;
    spin_lock_bh(table.lock);
    __hlist_del(table.empty_list.first);
    hlist_add_head((struct hlist_node *) mcp, &table.mc_hash[HASH256(group)]);
    table.groups++;
    spin_unlock_bh(table.lock);
    debugk("some tips added to mc_table\n");

	eth1_dev = dev_get_by_name("eth1");
    tp_send_igmp_report(eth1_dev, group);
	dev_put(eth1_dev);
    
end:
    kfree_skb(skb);
    return 0;
}

/*
 * solve igmp leave packet, may remove items
 */
int lan_heard_igmp_leave(struct sk_buff *skb, __u32 group)
{
    struct mc_entry *mcp;

    debugk("heard lan igmp leave\n");
    mcp = hlist_sort(group);
    if (mcp)
    {
		//modify port infomation
        mcp->membership_flag &= (~skb->nfmark);

		//if no port alive, delete item
        if (!mcp->membership_flag)
        {
            if (timer_pending(&mcp->wan_qr_timer))
            {
                del_timer(&mcp->wan_qr_timer);
            }
            
            spin_lock_bh(table.lock);
            __hlist_del((struct hlist_node *)mcp);
            hlist_add_head((struct hlist_node *)mcp, &table.empty_list);
            table.groups--;
            spin_unlock_bh(table.lock);
			//send leave packet to wan

			eth1_dev = dev_get_by_name("eth1");
            tp_send_igmp_leave(eth1_dev, group);
			dev_put(eth1_dev);
			
            mcp->mc_addr = 0;			
			if(mcp->dst)
			{
				dst_release(mcp->dst);
				mcp->dst = NULL;
			}
			
            debugk("some tips removed from mc_table\n");

			//if              
            if (!table.groups)
            {
				//keep g-q timer, yzt 2010-02-21
                //del_timer(&table.generl_query_timer);
                del_timer(&table.report_checking_timer);
            }
        }
    }

    kfree_skb(skb);
    return 0;    
}

/*
 * solve wan query packet, set timer for all alive items
 */
int wan_heard_igmp_query(struct sk_buff *skb, struct igmphdr *ih, __u32 daddr)
{
    int i;
    struct mc_entry *mcp;

    debugk("heard wan igmp query\n");
	//if empty do not resolve
    if (table.groups)
    {
		//general query
        if (!ih->group && daddr == IGMP_ALL_HOST_ADDR)
        {
            for (i = 0; i < MAX_GROUP_ENTRIES; i++)
            {
                mcp = &table.entry[i];
                if (mcp->mc_addr)
                {
                    mcp->wan_qr_timer.expires = jiffies + (jiffies % QUERY_RESPONSE_INTERVAL_NUM) * HZ;
					if (!timer_pending(&mcp->wan_qr_timer))
					{
                    	add_timer(&mcp->wan_qr_timer);
					}
                }
            }
        }
        else if (ih->group == daddr)
        {
			// g-s query
            mcp = hlist_sort(daddr);
            if (mcp)
            {
                mcp->wan_qr_timer.expires = jiffies + (jiffies % ih->code) * HZ / 10;
				if (!timer_pending(&mcp->wan_qr_timer))
				{
                	add_timer(&mcp->wan_qr_timer);
				}
            }
        }
    }

    kfree_skb(skb);
    return 0;
}

/*
 * solve wan report packet, stop the q-r timer
 */
int wan_heard_igmp_report(struct sk_buff *skb, __u32 group)
{
    struct mc_entry *mcp;

    debugk("heard wan igmp report\n");
    mcp = hlist_sort(group); 
    if (mcp)
    {
        if (timer_pending(&mcp->wan_qr_timer))
        {
            del_timer(&mcp->wan_qr_timer);
        }
    }

    kfree_skb(skb);
    return 0;    
}

/*
 * solve wan data packet, find the out way, set port information
 */
int find_data_path(struct sk_buff *skb, __u32 group)
{
    struct mc_entry *mcp;
	struct rtable *rt;
//	struct flowi fl;

    debugk("heard data pack\n");
    mcp = hlist_sort(group); 
    if (mcp)
    {
        skb->nfmark = mcp->membership_flag;
        debugk("deliver data packet\n");		
//the following didn't initial all members in struct, may cause wrong hash result, may cause find null route
/*		fl.oif = br0_dev->ifindex;
		fl.nl_u.ip4_u.daddr = group;
		fl.proto = IPPROTO_UDP;
*/
	struct flowi fl = { .oif = br0_index,
                .nl_u = { .ip4_u = { .daddr = skb->nh.iph->daddr } },
                .proto = skb->nh.iph->protocol };

		if(!mcp->dst)//if no route backup
		{
		    if (ip_route_output_key(&rt, &fl))//get route info
		    {
		        printk("Ooops, static route udp out error!\n");
				//ip_rt_put(rt);
				kfree(skb);
		        return -1;
		    }
			else if (rt->rt_src == 0)
			{
				debugk("no route source\n");
				//ip_rt_put(rt);
				//dst_free(mcp->dst);
				kfree(skb);
				return -1;
			}
			else
			{
				mcp->dst = &rt->u.dst;//backup route info			
			}
		}
		
	    skb->dst = mcp->dst;

#if 0	//if have wlan member, send data to br0, yzt 2010-02-21//do not use now
#define WLAN_IGMP_MARK 0xffff0000
		if(WLAN_IGMP_MARK != (skb->nfmark & WLAN_IGMP_MARK))
		{
			printk("nfmark:0x%x", skb->nfmark);
			//change back to original dev, do not send to wireless
	    	skb->dev = eth0_dev;
	    	skb->dst->dev = eth0_dev;
		}
#endif

		mcp->dst->lastuse = jiffies;
		dst_hold(mcp->dst);
		mcp->dst->__use++;
		//RT_CACHE_STAT_INC(out_hit);
	    return ip_output(skb);		
    }
    else
    {
        debugk("no tips of that group, drop\n");
        kfree_skb(skb);
        return -1;
    }
}

/*
 *	send multicast packet //not used now, igmp and udp packets are solved seperately 
 */
 /*
int tp_send_mc(struct sk_buff *skb, struct net_device *dev)
{
    struct rtable *rt;

    struct flowi fl = { .oif = dev->ifindex,
                .nl_u = { .ip4_u = { .daddr = skb->nh.iph->daddr } },
                .proto = skb->nh.iph->protocol };

	//eth0 don't have static route, use br0 instead
    if (dev->ifindex == eth0_dev->ifindex)
    {
        fl.oif = br0_dev->ifindex;
    }
    
    if (ip_route_output_key(&rt, &fl))
    {
        printk("Ooops, static route don't kown this output interface!\n");
		kfree(skb);
        return -1;
    }
    
    if (rt->rt_src == 0)
    {
        debugk("route source\n");
        ip_rt_put(rt);
		kfree(skb);
		return -1;
	}
    
    skb->dst = (struct dst_entry*) rt;
	//change back to original dev, do not send to wireless
    skb->dev = dev;
    skb->dst->dev = dev;

    return ip_output(skb);
}
*/

