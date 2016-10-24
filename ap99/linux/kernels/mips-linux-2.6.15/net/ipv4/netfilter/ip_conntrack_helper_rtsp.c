/*!
 * RTSP extension for IP connection tracking
 * (C) 2003 by Tom Marshall <tmarshall@real.com>
 * based on ip_conntrack_irc.c
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 * Module load syntax:
 *   insmod ip_conntrack_rtsp.o ports=port1,port2,...port<MAX_PORTS>
 *                              max_outstanding=n setup_timeout=secs
 *
 * If no ports are specified, the default will be port 554.
 *
 * With max_outstanding you can define the maximum number of not yet
 * answered SETUP requests per RTSP session (default 8).
 * With setup_timeout you can specify how long the system waits for
 * an expected data channel (default 300 seconds).
 */
/*!
 * history 
 *         from Broadcom xDSL platform 
 * modification
 *         modify struct rtsp_data_ports to contain all the TCP/UDP conntrack
 *         modify struct ip_ct_rtsp_expect to contain the server_port(if needed)
 *         modify help_in to deal with SETUP reply message correctly
 *         modify save_ct to be suitable for rtp_expect and rtt_expect
 *         modify rtsp_pause_timeout to refresh TCP/UDP conntrack correctly
 *         add rtsp_teardown_timeout to destroy TCP/UDP conntrack asap.
 *         modify rtsp_client_to_nat_pmap to fix a bug that a same UDP port maybe take two rtsp_data_ports
 *         modify rtsp_nat_to_client_pmap to be suitble for rtp_expect and rtt_expect
 *         add rtsp_data_ports_release to release the map from ip_conntrack_core.c
 * 		   add rtt_expect for those players that actively transmit a UDP packet first
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/netfilter.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <net/checksum.h>
#include <net/tcp.h>

#include <linux/netfilter_ipv4/lockhelp.h>
#include <linux/netfilter_ipv4/ip_conntrack_helper.h>
#include <linux/netfilter_ipv4/ip_conntrack_rtsp.h>
#include <linux/netfilter_ipv4/ip_nat_rtsp.h>

#include <linux/ctype.h>
#define NF_NEED_STRNCASECMP
#define NF_NEED_STRTOU16
#define NF_NEED_STRTOU32
#define NF_NEED_NEXTLINE
#include <linux/netfilter_helpers.h>
#define NF_NEED_MIME_NEXTLINE
#include <linux/netfilter_mime.h>

MODULE_AUTHOR("Tom Marshall <tmarshall@real.com>");
MODULE_DESCRIPTION("RTSP connection tracking module");
MODULE_LICENSE("GPL");

#define MAX_PORTS 8

/*!
 * To enable debugging, replace the line below with #define IP_NF_RTSP_DEBUG 1
 */
#undef IP_NF_RTSP_DEBUG
#define INFOP(args...) printk(KERN_INFO args)
#if defined(IP_NF_RTSP_DEBUG)
#define DEBUGP(args...) printk(args)
#define DEBUGP_DETAIL(args...) printk("%s:%s(%d) ", __FILE__, __FUNCTION__, __LINE__); \
					           printk(args)
static int debug_mode = 3;
module_param(debug_mode, int,      0600);
MODULE_PARM_DESC(debug_mode, "debug rtsp_data_ports");
 #define RTSP_ASSERT(x)						\
	do {								\
		if (!(x))						\
			printk("RTSP_ASSERT: %s:%s:%u\n",		\
				   __FUNCTION__, __FILE__, __LINE__);	\
	} while(0)
#else
#define DEBUGP(args...)
#define DEBUGP_DETAIL(args...)
#define RTSP_ASSERT(x)
#endif

static unsigned int ports[MAX_PORTS];
static unsigned int ports_c;
module_param_array(ports, uint, &ports_c, 0400);
MODULE_PARM_DESC(ports, "port numbers of RTSP servers");

static int max_outstanding = 8;
module_param(max_outstanding, int, 0600);
MODULE_PARM_DESC(max_outstanding, "max number of outstanding SETUP requests per RTSP session");

static int setup_timeout = 300;  /*!< 5 minutes */
module_param(setup_timeout, int, 0600);
MODULE_PARM_DESC(setup_timeout, "timeout on for unestablished data channels");

unsigned int (*ip_nat_rtsp_hook)(struct ip_conntrack* ct,
		 struct ip_conntrack_expect* exp,
		 enum ip_conntrack_info ctinfo,
		 struct sk_buff** pskb) = NULL;
EXPORT_SYMBOL_GPL(ip_nat_rtsp_hook);

static char *rtsp_buffer;  /*!< This is slow, but it's simple. --RR */

DECLARE_LOCK(ip_rtsp_lock);
DECLARE_LOCK(ip_rtsp_data_ports_lock);

/*!
 * Max mappings we will allow for one RTSP connection (for RTP, the number
 * of allocated ports is twice this value).  Note that SMIL burns a lot of
 * ports so keep this reasonably high.  If this is too low, you will see a
 * lot of "no free client map entries" messages.
 * we changed the value from 16 to 64, if 16, it means that maybe only 8 clients will
 * be able to play the RTSP stream
 */
#define MAX_PORT_MAPS 64
#define UDP_PORT_START  7000
#define UDP_PORT_END    0
static u_int16_t g_tr_port = UDP_PORT_START;

#define PAUSE_TIMEOUT      (5 * HZ)
/*! 
 * after TEARDOWN, some other messages will still transmit, and the TCP connection will be closed in 
 * about 5ms, to destroy all the TCP/UDP connections asap, we use this timer
 */
#define TEARDOWN_TIMEOUT   (HZ / 5)
#define RTSP_PAUSE_TIMEOUT (6 * HZ)


#define SKIP_WSPACE(ptr,len,off) while(off < len && isspace(*(ptr+off))) { off++; }
/*!
 \structure _rtsp_data_ports
 \description to hold the mappings from client to NAT vice versa. If we
              mangle UDP ports in the outgoing SETUP message, we must properly
              mangle them in the return direction so that the client will
              process the packets appropriately.
 */
struct _rtsp_data_ports {
    u_int32_t           client_ip;
    u_int16_t           client_tcp_port;
    u_int16_t           client_udp_lo;
    u_int16_t           client_udp_hi;
    portblock_t         pbtype;
    u_int16_t           nat_udp_lo;
    u_int16_t           nat_udp_hi;
    struct timer_list   pause_timeout;
    struct ip_conntrack *ct_master;   /*!< main conntrack and also used to test if this map is in use */
    struct ip_conntrack *ct_loexpect; /*!< udp conntrack low */
    struct ip_conntrack *ct_hiexpect; /*!< udp conntrack high */
	struct sk_buff      *skb;         /*!< lmc added this member for the ip_ct_refresh function */
    int                 timeout_active;
    /*int                 in_use;*/   /*!< no longer used, we use the ct_master instead of it */
} rtsp_data_ports[MAX_PORT_MAPS];

/*!
  \description  Both rtp_expect and rtt_expect can call this function, so this function can not find out a map just
                from ct information, we added a parameter 'port', and the save_ct will find out a map dependent on it.
  \param[in]    ct expected UDP contrack
  \param[in]    port find out the rtsp_data_ports index with it
  \return       none
 */
static void
save_ct(struct ip_conntrack *ct, u_int16_t port)
{
    int i    = 0;
#if defined(IP_NF_RTSP_DEBUG)
    struct ip_conntrack_tuple *tp = &ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple;
#endif
	DEBUGP_DETAIL("ct     %p ", ct ); DUMP_TUPLE(tp);
	DEBUGP_DETAIL("       %9s", "");  DUMP_TUPLE(&ct->tuplehash[IP_CT_DIR_REPLY].tuple);
	DEBUGP_DETAIL("master %p ", ct->master); DUMP_TUPLE(&ct->master->tuplehash[IP_CT_DIR_ORIGINAL].tuple);
	DEBUGP_DETAIL("       %9s", "");         DUMP_TUPLE(&ct->master->tuplehash[IP_CT_DIR_REPLY].tuple);
	DEBUGP_DETAIL("port %hu\n", port);

    for (i = 0; i < MAX_PORT_MAPS; i++)
    {
        if (!rtsp_data_ports[i].ct_master)
        {
            continue;
        }
        if (rtsp_data_ports[i].nat_udp_lo == port ||
			rtsp_data_ports[i].nat_udp_hi == port)
        {
			RTSP_ASSERT(rtsp_data_ports[i].ct_master == ct->master);
			DEBUGP_DETAIL("rtsp_data_port[%d] nat %hu-%hu tp dst %hu master %p ", i, 
				rtsp_data_ports[i].nat_udp_lo, rtsp_data_ports[i].nat_udp_hi, port, ct->master);
			if (rtsp_data_ports[i].nat_udp_lo == port)
			{
	            rtsp_data_ports[i].ct_loexpect = ct;
				DEBUGP("loexpect %p\n", ct);
			}
			else
			{
	            rtsp_data_ports[i].ct_hiexpect = ct;
				DEBUGP("hiexpect %p\n", ct);
			}
            /*rtsp_data_ports[i].ct_master = ct->master;*/
            break;
        }
    }
}

static void
rtsp_pause_timeout(unsigned long data)
{
    int  index = (int) data;
    struct _rtsp_data_ports *rtsp_data = &rtsp_data_ports[index];
    struct ip_conntrack *ct = rtsp_data->ct_master;
 	DEBUGP_DETAIL("%p refresh\n", ct);

    if (ct) {
        rtsp_data->pause_timeout.expires = jiffies + PAUSE_TIMEOUT;
        rtsp_data->pause_timeout.function = rtsp_pause_timeout;
        rtsp_data->pause_timeout.data = data;
        rtsp_data->timeout_active = 1;
		/*! 
		 * we refresh all the UDP connection also to destroy them asap.,otherwise,after a long pausing time,
		 * the UDP connections will be destroyed.
		 */
		if (rtsp_data->ct_loexpect)
		{
			ip_ct_refresh(rtsp_data->ct_loexpect, rtsp_data->skb, RTSP_PAUSE_TIMEOUT);
		}
		if (rtsp_data->ct_hiexpect)
		{
			ip_ct_refresh(rtsp_data->ct_hiexpect, rtsp_data->skb, RTSP_PAUSE_TIMEOUT);
		}
			
        ip_ct_refresh(ct, rtsp_data->skb, RTSP_PAUSE_TIMEOUT);
        add_timer(&rtsp_data->pause_timeout);
    }
}

 static void
 rtsp_teardown_timeout(unsigned long data)
 {
	 int  index = (int) data;
	 struct _rtsp_data_ports *rtsp_data = &rtsp_data_ports[index];
	 DEBUGP_DETAIL("rtsp_data_ports[%d] released ct_master %p ct_loexpect %p ct_hiexpect %p\n", index, 
		 rtsp_data_ports[index].ct_master, rtsp_data_ports[index].ct_loexpect, rtsp_data_ports[index].ct_hiexpect);
 
	 if (rtsp_data->ct_master) {
	 	/*!
	 	 * now,refresh all the conntracks to release them asap.
	 	 */
		 LOCK_BH(&ip_rtsp_data_ports_lock);
		 if (rtsp_data->ct_loexpect)
		 {
			 ip_ct_refresh(rtsp_data->ct_loexpect, rtsp_data->skb, 1);
		 }
		 if (rtsp_data->ct_hiexpect)
		 {
			 ip_ct_refresh(rtsp_data->ct_hiexpect, rtsp_data->skb, 1);
		 }
		 ip_ct_refresh(rtsp_data->ct_master, rtsp_data->skb, 1);
 		 memset(rtsp_data, 0, sizeof(struct _rtsp_data_ports));
		 UNLOCK_BH(&ip_rtsp_data_ports_lock);
	 }
 }
 
static void
ip_conntrack_rtsp_proc_play(struct ip_conntrack *ct
#if defined(IP_NF_RTSP_DEBUG)
	, const struct iphdr *iph
#endif	
	)
{
    int i    = 0;
#if defined(IP_NF_RTSP_DEBUG)
    struct tcphdr *tcph = (void *)iph + iph->ihl * 4;
#endif
    for (i = 0; i < MAX_PORT_MAPS; i++)
    {
         if (rtsp_data_ports[i].ct_master == ct)
        {
            DEBUGP_DETAIL("Found index %d client %u.%u.%u.%u:%hu UDP %hu-%hu\n", i,
                    NIPQUAD(iph->saddr), tcph->source, rtsp_data_ports[i].client_udp_lo,
                    rtsp_data_ports[i].client_udp_hi);
            if (rtsp_data_ports[i].timeout_active)
            {
                del_timer(&rtsp_data_ports[i].pause_timeout);
                rtsp_data_ports[i].timeout_active = 0;
            }
        }
    }
}

static void
ip_conntrack_rtsp_proc_pause(struct ip_conntrack *ct
#if defined(IP_NF_RTSP_DEBUG)
	, const struct iphdr *iph
#endif	
	)
{
    int i    = 0;
#if defined(IP_NF_RTSP_DEBUG)
    struct tcphdr *tcph = (void *)iph + iph->ihl * 4;
#endif
    for (i = 0; i < MAX_PORT_MAPS; i++)
    {
        if (rtsp_data_ports[i].ct_master == ct)
        {
            DEBUGP_DETAIL("Found index %d client info SRC IP %u.%u.%u.%u TCP PORT %hu UDP PORTS (%hu-%hu)\n", i,
                    NIPQUAD(iph->saddr), tcph->source, rtsp_data_ports[i].client_udp_lo,
                    rtsp_data_ports[i].client_udp_hi);
			DEBUGP_DETAIL("ct %p rtsp_data_ports.timeout_active %d rtsp_data_ports.ct %p\n", ct, 
				rtsp_data_ports[i].timeout_active, rtsp_data_ports[i].ct_master);
            if (rtsp_data_ports[i].timeout_active != 0)
            {
                break;
            }
            rtsp_data_ports[i].pause_timeout.expires = jiffies + PAUSE_TIMEOUT;
            rtsp_data_ports[i].pause_timeout.function = rtsp_pause_timeout;
            rtsp_data_ports[i].pause_timeout.data = (unsigned long)i;
            add_timer(&rtsp_data_ports[i].pause_timeout);
            rtsp_data_ports[i].timeout_active = 1;
            /*rtsp_data_ports[i].ct_master = ct; */  /*!< actually, this is unnecessary */
            ip_ct_refresh(ct, rtsp_data_ports[i].skb, RTSP_PAUSE_TIMEOUT);
        }
    }
}

/*!
 \description  Maps client ports that are overlapping with other client UDP transport to
               new NAT ports that will be tracked and converted back to client assigned UDP ports.
 \param[in,out] prtspexp used to find out a rtsp_data_ports, also returned the NAT port
 \param[in]     iph used to find out a rtsp_data_ports
 \param[in]     ct main contrack
 \param[in]     skb
 \return        0-no free map 1-map succeed 2-already mapped
 */
static int
rtsp_client_to_nat_pmap(struct ip_ct_rtsp_expect *prtspexp, const struct iphdr *iph,
                        struct ip_conntrack *ct, struct sk_buff *skb)
{
    int i  = 0;
    int rc = 0;
    struct tcphdr *tcph   = (void *)iph + iph->ihl * 4;

    DEBUGP_DETAIL("IP %u.%u.%u.%u:%hu->%u.%u.%u.%u:%hu\n", NIPQUAD(iph->saddr), tcph->source, 
           NIPQUAD(iph->daddr), tcph->dest);

    for (i = 0; i < MAX_PORT_MAPS; i++) 
	{
        if (rtsp_data_ports[i].ct_master) 
		{
            DEBUGP_DETAIL("Index %d used IP %u.%u.%u.%u CLIENT %hu-%hu NAT %hu-%hu\n", i,
                   NIPQUAD(rtsp_data_ports[i].client_ip),
                   rtsp_data_ports[i].client_udp_lo, rtsp_data_ports[i].client_udp_hi,
                   rtsp_data_ports[i].nat_udp_lo, rtsp_data_ports[i].nat_udp_hi);
            if (ntohl(iph->saddr) == rtsp_data_ports[i].client_ip &&
                ntohs(tcph->source) == rtsp_data_ports[i].client_tcp_port &&
                ntohs(prtspexp->loport) == rtsp_data_ports[i].client_udp_lo &&
                (ntohs(prtspexp->hiport) == rtsp_data_ports[i].client_udp_hi ||
                 prtspexp->hiport == prtspexp->loport)
                )
            {
                prtspexp->loport  = rtsp_data_ports[i].nat_udp_lo;
				if (prtspexp->hiport == prtspexp->loport)
				{
					prtspexp->hiport = rtsp_data_ports[i].nat_udp_lo;
				}
				else
				{
	                prtspexp->hiport  = rtsp_data_ports[i].nat_udp_hi;
				}
                return rc = 2;
            }
        }
    }
	/*!
	 * some players such as Media Player Classic always use the same client_port, if rtsp_data_port[2] was allocated 
	 * for the first SETUP, before the second SETUP arrives, maybe rtsp_data_ports[0](allocated for another connection)
	 * was released already, thus the second SETUP will be allcated rtsp_data_ports[0],
	 * so we must do the entire loop to find out whether the UDP port has been mapped or not
	 */
    for (i = 0; i < MAX_PORT_MAPS; i++) 
	{
        if (! rtsp_data_ports[i].ct_master) 
        {
	        rtsp_data_ports[i].client_ip       = ntohl(iph->saddr);
	        rtsp_data_ports[i].client_tcp_port = ntohs(tcph->source);
	        rtsp_data_ports[i].client_udp_lo   = ntohs(prtspexp->loport);
	        rtsp_data_ports[i].client_udp_hi   = ntohs(prtspexp->hiport);
	        rtsp_data_ports[i].pbtype          = prtspexp->pbtype;
	        rtsp_data_ports[i].skb             = skb;
	        rtsp_data_ports[i].ct_master       = ct;
	        /*rtsp_data_ports[i].in_use          = 1;*/ /*!< now, we use the ct_master to test if this map is used */
	        init_timer(&rtsp_data_ports[i].pause_timeout);
	        prtspexp->loport  = rtsp_data_ports[i].nat_udp_lo = g_tr_port++;
			if (prtspexp->loport != prtspexp->hiport)
			{
				prtspexp->hiport  = rtsp_data_ports[i].nat_udp_hi = g_tr_port;
			}
			else
			{
				prtspexp->hiport  = rtsp_data_ports[i].nat_udp_hi = prtspexp->loport;
			}
			g_tr_port ++;  /*!< even the hiport maybe equal to loport, we still increment g_tr_port twice to guarantee the mangled port will be even-odd formula */
			if (UDP_PORT_END == g_tr_port)
			{
				g_tr_port = UDP_PORT_START;
			}
			DEBUGP_DETAIL("Mapped Index %d IP %u.%u.%u.%u CLIENT %hu-%hu NAT %hu-%hu\n", i,
				   NIPQUAD(rtsp_data_ports[i].client_ip),
				   rtsp_data_ports[i].client_udp_lo, rtsp_data_ports[i].client_udp_hi,
				   rtsp_data_ports[i].nat_udp_lo, rtsp_data_ports[i].nat_udp_hi);
	        return rc = 1;
        }
    }
    return rc;
}

/*!
  \description Performs NAT to client port mapping. Incoming UDP ports are looked up and
               appropriate client ports are extracted from the table and returned.
               Return client_udp_port or 0 when no matches found.
               we added a paramter manip, this parameter will be used for the rtt_expect,
               the rtt_expect wants to know how to do the NAT for the first UDP packet
               from the client to the server
  \param[in]   nat_port  find out the rtsp_data_ports with it
  \param[out]  manip return the client_ip and NAT port for rtt_expect's use
  \return      client_port or 0
 */
static u_int16_t
rtsp_nat_to_client_pmap(u_int16_t nat_port, struct ip_conntrack_manip *manip)
{
    int           i       = 0;
    u_int16_t     tr_port = 0;

    for (i = 0; i < MAX_PORT_MAPS; i++) {
        if (!rtsp_data_ports[i].ct_master) {
            continue;
        }
        /*!
         * Check if the UDP ports match any of our NAT ports and return
         * the client UDP ports.
         */
        DEBUGP_DETAIL("Searching index %d NAT %hu map NAT %hu-%hu CLIENT %hu-%hu\n", i,
               ntohs(nat_port), rtsp_data_ports[i].nat_udp_lo, rtsp_data_ports[i].nat_udp_hi,
               rtsp_data_ports[i].client_udp_lo, rtsp_data_ports[i].client_udp_hi);
        if (ntohs(nat_port) == rtsp_data_ports[i].nat_udp_lo ||
            ntohs(nat_port) == rtsp_data_ports[i].client_udp_lo) 
        {
            tr_port = rtsp_data_ports[i].client_udp_lo;
            DEBUGP_DETAIL("Found index %d NAT_PORT %hu tr_port %hu\n", i,
                   nat_port, tr_port);
			if (manip)
			{
				manip->ip = rtsp_data_ports[i].client_ip;
				manip->u.all = rtsp_data_ports[i].nat_udp_lo;
			}
			return tr_port;
        } else if (ntohs(nat_port) == rtsp_data_ports[i].nat_udp_hi ||
                   ntohs(nat_port) == rtsp_data_ports[i].client_udp_hi) 
		{
            tr_port = rtsp_data_ports[i].client_udp_hi;
            DEBUGP_DETAIL("Found index %d NAT_PORT %hu tr_port %hu\n", i,
                   nat_port, tr_port);
			if (manip)
			{
				manip->ip = rtsp_data_ports[i].client_ip;
				manip->u.all = rtsp_data_ports[i].nat_udp_hi;
			}
            return tr_port;
        }
    }
    return tr_port;
}

static void
rtp_expect(struct ip_conntrack *ct, struct ip_conntrack_expect *this)
{
    u_int16_t nat_port = ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.dst.u.udp.port;
    u_int16_t orig_port = 0;
    DEBUGP_DETAIL("ct orig  "); DUMP_TUPLE(&ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple);
    DEBUGP_DETAIL("ct reply "); DUMP_TUPLE(&ct->tuplehash[IP_CT_DIR_REPLY].tuple);
    orig_port = rtsp_nat_to_client_pmap(nat_port, NULL);
    DEBUGP_DETAIL("nat port:%hu, UDP client port %hu\n", nat_port, orig_port);

    save_ct(ct, nat_port);

    struct ip_conntrack *master = master_ct(ct);
    RTSP_ASSERT(master);
    DEBUGP_DETAIL("master orig  "); DUMP_TUPLE(&master->tuplehash[IP_CT_DIR_ORIGINAL].tuple);
    DEBUGP_DETAIL("master reply "); DUMP_TUPLE(&master->tuplehash[IP_CT_DIR_REPLY].tuple);
	DEBUGP_DETAIL("master %p ct %p \n", master, ct);
    DEBUGP_DETAIL("srcip=%u.%u.%u.%u, dstip=%u.%u.%u.%u, CLIENT %hu\n",
           NIPQUAD(ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.ip), 
           NIPQUAD(master->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.ip), orig_port);

    struct ip_nat_range range;
    /*!
     * We don't want to manip the per-protocol, just the IPs. Actually we
     * did manipulate the UDP ports
     */
    range.flags = IP_NAT_RANGE_MAP_IPS;
    range.min_ip = range.max_ip = master->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.ip;
    range.flags |= IP_NAT_RANGE_PROTO_SPECIFIED;
    range.min.udp.port = range.max.udp.port = orig_port;

    ip_nat_setup_info(ct, &range, NF_IP_PRE_ROUTING);
}

/*! 
 \description   rtt_expect is added for the player which transmit the first UDP packet after it got the server port
                we will do NAT for this packet. Coreplayer or Media Player Classic will all do this.
 \param[in]     ct expected conntrack
 \param[in]     this not used 
 \return        none
 */
static void
rtt_expect(struct ip_conntrack *ct, struct ip_conntrack_expect *this)
{
    u_int16_t orig_port = ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.u.udp.port;
	struct ip_conntrack_manip manip;
    rtsp_nat_to_client_pmap(orig_port, &manip);
#ifdef IP_NF_RTSP_DEBUG
    struct ip_conntrack *master = master_ct(ct);
    RTSP_ASSERT(master);
	DEBUGP_DETAIL("master %p ct %p orig port %hu nat %u.%u.%u.%u:%hu\n", master, ct, orig_port, NIPQUAD(manip.ip), manip.u.all);
#endif
	save_ct(ct, manip.u.all);

    struct ip_nat_range range;

    range.flags = IP_NAT_RANGE_MAP_IPS;
    range.min_ip = range.max_ip = manip.ip;
    range.flags |= IP_NAT_RANGE_PROTO_SPECIFIED;
    range.min.udp.port = range.max.udp.port = manip.u.all;

    ip_nat_setup_info(ct, &range, NF_IP_POST_ROUTING);
}

extern void (*ip_conntrack_rtsp_data_ports_release_hook)(struct ip_conntrack *ct);

/*!
 \description  This function will be called from ip_conntrack_core.c . 
               if the client exited abnormally, the rtsp_data_ports can't be release anymore because 
               no TEARDOWN message will be received. so we release it from the conntrack information.
 \param[in]    ct main conntrack
 \return       none
 */
void
ip_conntrack_rtsp_data_ports_release(struct ip_conntrack *ct)
{
	int i;
#if defined(IP_NF_RTSP_DEBUG)
	u_int32_t ip = ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.ip;
	int port = ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.u.all;
	DEBUGP_DETAIL("%u.%u.%u.%u:%hu\n", NIPQUAD(ip), port);
#endif
	LOCK_BH(&ip_rtsp_data_ports_lock);
    for (i = 0; i < MAX_PORT_MAPS; i++) 
	{
		if (rtsp_data_ports[i].ct_master)
		{
			if (rtsp_data_ports[i].ct_loexpect == ct)
			{
				DEBUGP_DETAIL("rtsp_data_ports[%d] ct_loexpect %p destroyed\n", i, ct);
				ip_ct_refresh(rtsp_data_ports[i].ct_loexpect, rtsp_data_ports[i].skb, 1);
				rtsp_data_ports[i].ct_loexpect = NULL;
			}
			else if (rtsp_data_ports[i].ct_hiexpect == ct)
			{
				DEBUGP_DETAIL("rtsp_data_ports[%d] ct_hiexpect %p destroyed\n", i, ct);
				ip_ct_refresh(rtsp_data_ports[i].ct_hiexpect, rtsp_data_ports[i].skb, 1);
				rtsp_data_ports[i].ct_hiexpect = NULL;
			}
			else if (rtsp_data_ports[i].ct_master == ct)
			{
				DEBUGP_DETAIL("rtsp_data_ports[%d] released ct_master %p ct_loexpect %p ct_hiexpect %p\n", i,
					rtsp_data_ports[i].ct_master, rtsp_data_ports[i].ct_loexpect, rtsp_data_ports[i].ct_hiexpect);
				if (rtsp_data_ports[i].timeout_active)
				{
					del_timer(&rtsp_data_ports[i].pause_timeout);
					rtsp_data_ports[i].timeout_active = 0;
				}
				if (rtsp_data_ports[i].ct_loexpect)
				{
					ip_ct_refresh(rtsp_data_ports[i].ct_loexpect, rtsp_data_ports[i].skb, 1);
				}
				if (rtsp_data_ports[i].ct_hiexpect)
				{
					ip_ct_refresh(rtsp_data_ports[i].ct_hiexpect, rtsp_data_ports[i].skb, 1);
				}
				ip_ct_refresh(rtsp_data_ports[i].ct_master, rtsp_data_ports[i].skb, 1);
				memset(&rtsp_data_ports[i], 0, sizeof(struct _rtsp_data_ports));
			}
		}
    }
	UNLOCK_BH(&ip_rtsp_data_ports_lock);
}

static void
ip_conntrack_rtsp_proc_teardown(struct ip_conntrack *ct
#if defined(IP_NF_RTSP_DEBUG)
	,struct iphdr *iph
#endif	
	)
{
    int i    = 0;
#if defined(IP_NF_RTSP_DEBUG)
    struct tcphdr *tcph = (void *)iph + iph->ihl * 4;
#endif

    for (i = 0; i < MAX_PORT_MAPS; i++)
    {
        if (rtsp_data_ports[i].ct_master == ct)
        {
            DEBUGP_DETAIL("Found index %d client info SRC IP %u.%u.%u.%u TCP PORT %hu UDP PORTS (%hu-%hu)\n", i,
                    NIPQUAD(iph->saddr), tcph->source, rtsp_data_ports[i].client_udp_lo,
                    rtsp_data_ports[i].client_udp_hi);
            if (rtsp_data_ports[i].timeout_active)
            {
                del_timer(&rtsp_data_ports[i].pause_timeout);
            }

			rtsp_data_ports[i].pause_timeout.expires = jiffies + TEARDOWN_TIMEOUT;
			/*!
			 * after TEARDOWN,some other messages still transmitted between client and server, so we wait for some time
			 */
			rtsp_data_ports[i].pause_timeout.function = rtsp_teardown_timeout;
			rtsp_data_ports[i].pause_timeout.data = (unsigned long)i;
			add_timer(&rtsp_data_ports[i].pause_timeout);
			rtsp_data_ports[i].timeout_active = 1;
            /*memset(&rtsp_data_ports[i], 0, sizeof(struct _rtsp_data_ports));*/ /*!< the releasing work will be done in the rtsp_teardown_timeout */
            /*break;*/ /*!< maybe more than one map belong to this ct */
        }
    }
}

static void *
find_char(void *str, int ch, size_t len)
{
    unsigned char *pStr = NULL;
    if (len != 0) 
	{
        pStr = str;
    }
    do 
	{
        if (*pStr++ == ch) 
		{
            return ((void *)(pStr - 1));
        }
    } while (--len != 0);
    return (NULL);
}

/*!
 * Parse an RTSP packet.
 *
 * Returns zero if parsing failed.
 *
 * Parameters:
 *  IN      ptcp        tcp data pointer
 *  IN      tcplen      tcp data len
 *  IN/OUT  ptcpoff     points to current tcp offset
 *  OUT     phdrsoff    set to offset of rtsp headers
 *  OUT     phdrslen    set to length of rtsp headers
 */
static int
rtsp_parse_message(char* ptcp, uint tcplen, uint* ptcpoff,
                   uint* phdrsoff, uint* phdrslen)
{
    uint    entitylen = 0;
    uint    lineoff;
    uint    linelen;

    if (!nf_nextline(ptcp, tcplen, ptcpoff, &lineoff, &linelen))
    {
        return 0;
    }

    *phdrsoff = *ptcpoff;
    while (nf_mime_nextline(ptcp, tcplen, ptcpoff, &lineoff, &linelen))
    {
        if (linelen == 0)
        {
            if (entitylen > 0)
            {
                *ptcpoff += min(entitylen, tcplen - *ptcpoff);
            }
            break;
        }
        if (lineoff+linelen > tcplen)
        {
            INFOP("!! overrun !!\n");
            break;
        }
#if 0   /*!< lmc deleted, these parameters are useless now */
        if (nf_strncasecmp(ptcp+lineoff, "CSeq:", 5) == 0)
        {
            *pcseqoff = lineoff;
            *pcseqlen = linelen;
        }
#endif		
        if (nf_strncasecmp(ptcp+lineoff, "Content-Length:", 15) == 0)
        {
            uint off = lineoff+15;
            SKIP_WSPACE(ptcp+lineoff, linelen, off);
            nf_strtou32(ptcp+off, &entitylen);
        }
    }
    *phdrslen = (*ptcpoff) - (*phdrsoff);

    return 1;
}

/*!
 * Find lo/hi client ports (if any) in transport header
 * In:
 *   ptcp, tcplen = packet
 *   tranoff, tranlen = buffer to search
 *
 * Out:
 *   pport_lo, pport_hi = lo/hi ports (host endian)
 *
 * Returns nonzero if any client ports found
 *
 * Note: it is valid (and expected) for the client to request multiple
 * transports, so we need to parse the entire line.
 */
static int
rtsp_parse_transport(char* ptran, uint tranlen,
                     struct ip_ct_rtsp_expect* prtspexp)
{
    int     rc = 0;
    uint    off = 0;

    if (tranlen < 10 || !iseol(ptran[tranlen-1]) ||
        nf_strncasecmp(ptran, "Transport:", 10) != 0)
    {
        INFOP("sanity check failed\n");
        return 0;
    }
    DEBUGP_DETAIL("tran='%.*s'\n", (int)tranlen, ptran);
    off += 10;
    SKIP_WSPACE(ptran, tranlen, off);

    /*!< Transport: tran;field;field=val,tran;field;field=val,... */
    while (off < tranlen)
    {
        const char* pparamend;
        uint        nextparamoff;

        pparamend = find_char(ptran+off, ',', tranlen-off);
        pparamend = (pparamend == NULL) ? ptran+tranlen : pparamend+1;
        nextparamoff = pparamend-ptran;

        while (off < nextparamoff)
        {
            const char* pfieldend;
            uint        nextfieldoff;

            pfieldend = find_char(ptran+off, ';', nextparamoff-off);
            nextfieldoff = (pfieldend == NULL) ? nextparamoff : pfieldend-ptran+1;

            if (strncmp(ptran+off, "client_port=", 12) == 0)
            {
                u_int16_t   port;
                uint        numlen;

                off += 12;
                numlen = nf_strtou16(ptran+off, &port);
                off += numlen;
                if (prtspexp->loport != 0 && prtspexp->loport != port)
                {
                    DEBUGP_DETAIL("multiple ports found, port %hu ignored\n", port);
                }
                else
                {
                    prtspexp->loport = prtspexp->hiport = port;
                    DEBUGP_DETAIL("DASH or SLASH 0x%x\n", ptran[off]);
                    if (ptran[off] == '-')
                    {
                        off++;
                        numlen = nf_strtou16(ptran+off, &port);
                        off += numlen;
                        prtspexp->pbtype = pb_range;
                        prtspexp->hiport = port;

                        // If we have a range, assume rtp:
                        // loport must be even, hiport must be loport+1
                        if ((prtspexp->loport & 0x0001) != 0 ||
                            prtspexp->hiport != prtspexp->loport+1)
                        {
                            DEBUGP_DETAIL("incorrect range: %hu-%hu, correcting\n",
                                   prtspexp->loport, prtspexp->hiport);
                            prtspexp->loport &= 0xfffe;
                            prtspexp->hiport = prtspexp->loport+1;
                        }
                    }
                    else if (ptran[off] == '/')
                    {
                        off++;
                        numlen = nf_strtou16(ptran+off, &port);
                        off += numlen;
                        prtspexp->pbtype = pb_discon;
                        prtspexp->hiport = port;
                    }
                    rc = 1;
                }
            }
			/*!
			 * we added the following source code to get the server_port, and we will insert a expectaion 
			 * for the first UDP packet from the client_port to the server_port
			 */
#if defined(IP_NF_RTSP_RTT_CARE_SERVER_PORT)
            if (strncmp(ptran+off, "server_port=", 12) == 0)
            {
                u_int16_t   port;
                uint        numlen;

                off += 12;
                numlen = nf_strtou16(ptran+off, &port);
                off += numlen;
                {
 					prtspexp->loserver = prtspexp->hiserver = port;
                    DEBUGP_DETAIL("DASH or SLASH 0x%x\n", ptran[off]);
                    if (ptran[off] == '-')
                    {
                        off++;
                        numlen = nf_strtou16(ptran+off, &port);
                        off += numlen;
 						prtspexp->hiserver = port;

                    }
                    else if (ptran[off] == '/')
                    {
                        off++;
                        numlen = nf_strtou16(ptran+off, &port);
                        off += numlen;
 						prtspexp->hiserver = port;
                    }
                }
            }
#endif
            /*!
             * Note we don't look for the destination parameter here.
             * If we are using NAT, the NAT module will handle it.  If not,
             * and the client is sending packets elsewhere, the expectation
             * will quietly time out.
             */

            off = nextfieldoff;
        }

        off = nextparamoff;
    }

    return rc;
}

/*!< conntrack functions */

/*!
 \description outbound packet: client->server
 \param[in]	  skb
 \param[in]	  iph
 \param[in]	  pdata
 \param[in]	  datalen
 \param[in]	  ct
 \param[in]	  ctinfo
 \return 	  NF_ACCEPT or NF_DROP
 */
static int
help_out(struct sk_buff *skb, struct iphdr* iph, char* pdata, size_t datalen,
                struct ip_conntrack* ct, enum ip_conntrack_info ctinfo)
{
    int dir = CTINFO2DIR(ctinfo);   /*!< = IP_CT_DIR_ORIGINAL */
    uint    dataoff = 0;
    struct tcphdr *tcph = (void *)iph + iph->ihl * 4;

    struct ip_conntrack_expect exp;
	int rc;

     while (dataoff < datalen)
    {
        uint    cmdoff = dataoff;
        uint    hdrsoff = 0;
        uint    hdrslen = 0;
#if 0   /*!< lmc deleted these two parameters because they are useless */
        uint    cseqoff = 0;
        uint    cseqlen = 0;
#endif		
        uint    lineoff = 0;
        uint    linelen = 0;
        uint    off;
        uint    port = 0;
        struct  ip_conntrack_expect *new_exp = NULL;
        struct  ip_conntrack_expect *exp_succeeded = NULL;
        int     ret = 0;

        if (!rtsp_parse_message(pdata, datalen, &dataoff,
                                &hdrsoff, &hdrslen))
        {
            break;      /*!< not a valid message */
        }

        if (strncmp(pdata+cmdoff, "PLAY ", 5) == 0)
        {
            ip_conntrack_rtsp_proc_play(ct
#if defined(IP_NF_RTSP_DEBUG)
				, iph
#endif				
				);
            continue;
        }

        if (strncmp(pdata+cmdoff, "PAUSE ", 6) == 0)
        {
            ip_conntrack_rtsp_proc_pause(ct
#if defined(IP_NF_RTSP_DEBUG)
				, iph
#endif				
				);
            continue;
        }

        if (strncmp(pdata+cmdoff, "TEARDOWN ", 9) == 0)
        {
            ip_conntrack_rtsp_proc_teardown(ct
#if defined(IP_NF_RTSP_DEBUG)
				, iph
#endif				
				);   /*!< TEARDOWN message */
            continue;
        }

        if (strncmp(pdata+cmdoff, "SETUP ", 6) != 0)
        {
			continue;   /*!< not a SETUP message */
        }

        memset(&exp, 0, sizeof(exp));

		DEBUGP_DETAIL("src:%u.%u.%u.%u, dst:%u.%u.%u.%u\n", 
			NIPQUAD(skb->nh.iph->saddr), NIPQUAD(skb->nh.iph->daddr));

        off = 0;
        
        while (nf_mime_nextline(pdata+hdrsoff, hdrslen, &off,
                                &lineoff, &linelen))
        {
            if (linelen == 0)
            {
                break;
            }
            if (off > hdrsoff+hdrslen)
            {
                INFOP("!! overrun !!");
                break;
            }

            if (nf_strncasecmp(pdata+hdrsoff+lineoff, "Transport:", 10) == 0)
            {
                rc = rtsp_parse_transport(pdata+hdrsoff+lineoff, linelen,
                                     &exp.help.exp_rtsp_info);
            }
        }

        DEBUGP_DETAIL("found a setup message\n");

        if (exp.help.exp_rtsp_info.loport == 0)
        {
            DEBUGP_DETAIL("no udp transports found\n");
            continue;   /*!< no udp transports found */
        }

        DEBUGP_DETAIL("udp transport found, ports=(type:%d,port:%hu-%hu)\n",
              (int)exp.help.exp_rtsp_info.pbtype,
              exp.help.exp_rtsp_info.loport,
              exp.help.exp_rtsp_info.hiport);

       /*!
         * Translate the original ports to the NAT ports and note them
         * down to translate back in the return direction.
         */
        if (!(ret = rtsp_client_to_nat_pmap(&exp.help.exp_rtsp_info, iph, ct, skb)))
        {
            DEBUGP_DETAIL("Dropping the packet. No more space in the mapping table\n");
            return NF_DROP;
        }
		if (2 == ret)
		{
			ct->help.ct_rtsp_info.already_mapped = 1;
			/*!< already mapped,we needn't expect the related again,just call nat to mangle the client_port */
			DEBUGP_DETAIL("we needn't expect related again,just do the SETUP mangle\n");
			exp_succeeded = &exp;
			exp_succeeded->seq = ntohl(tcph->seq) + hdrsoff; /*!< mark all the headers */
			exp_succeeded->help.exp_rtsp_info.len = hdrslen;
			exp_succeeded->tuple = ct->tuplehash[!dir].tuple;
			exp_succeeded->mask.src.ip  = 0xffffffff;
			exp_succeeded->mask.dst.ip  = 0xffffffff;
			exp_succeeded->mask.dst.u.udp.port  = 0xffff;
			exp_succeeded->mask.dst.protonum	= 0xff;
		}
		else
		{
	        port = exp.help.exp_rtsp_info.loport;
	        while (port <= exp.help.exp_rtsp_info.hiport) 
			{
	            /*!
	             * Allocate expectation for tracking this connection
	             */
	            new_exp = ip_conntrack_expect_alloc(ct);  /*!< lmc added the ct parameter */
	            if (!new_exp) 
				{
	                INFOP("Failed to get a new expectation entry\n");
	                return NF_DROP;
	            }
	            memcpy(new_exp, &exp, sizeof(struct ip_conntrack_expect));
				/*!< lmc added the following two lines because the memcpy above destroys these two parameters */
				new_exp->master = ct;
				atomic_set(&new_exp->use, 1);
	            new_exp->seq = ntohl(tcph->seq) + hdrsoff; /*!< mark all the headers */
	            new_exp->help.exp_rtsp_info.len = hdrslen;

	            DEBUGP_DETAIL("Adding UDP port %hu,%hu\n", htons(port), ntohs(port));
				DEBUGP_DETAIL("orig  "); DUMP_TUPLE(&ct->tuplehash[dir].tuple);
				DEBUGP_DETAIL("reply "); DUMP_TUPLE(&ct->tuplehash[!dir].tuple);

	            new_exp->tuple = ct->tuplehash[!dir].tuple;
	            /*if (ret == 2) {
	                new_exp->tuple.dst.u.udp.port = htons(g_tr_port);
	                g_tr_port++;
	            } else*/
	            /*! 
	             * we commented the above source code, the process for this kind has been moved up
	             */
	                new_exp->tuple.dst.u.udp.port = htons(port);
	            new_exp->tuple.dst.protonum = IPPROTO_UDP;
	            new_exp->mask.src.ip  = 0xffffffff;
	            new_exp->mask.dst.ip  = 0xffffffff;
	            /*new_exp->mask.dst.u.udp.port  = (exp.help.exp_rtsp_info.pbtype == pb_range) ? 0xfffe : 0xffff;*/
	            new_exp->mask.dst.u.udp.port  = 0xffff;  /*!< we only expect the dst port matched */
	            new_exp->expectfn = rtp_expect;
	            new_exp->mask.dst.protonum  = 0xff;

	            DEBUGP_DETAIL("expect_related %u.%u.%u.%u:%u-%u.%u.%u.%u:%u\n",
	                    NIPQUAD(new_exp->tuple.src.ip),
	                    ntohs(new_exp->tuple.src.u.tcp.port),
	                    NIPQUAD(new_exp->tuple.dst.ip),
	                    ntohs(new_exp->tuple.dst.u.tcp.port));

	            /*!< pass the request off to the nat helper */
	            rc = ip_conntrack_expect_related(new_exp);  /*!< lmc deleted the ct parameter */
	            if (0 == rc)
	            {
	                DEBUGP_DETAIL("ip_conntrack_expect_related succeeded loport\n");
					exp_succeeded = new_exp;
	            }
	            else
	            {
					ip_conntrack_expect_put(new_exp);
	                DEBUGP_DETAIL("ip_conntrack_expect_related loport failed (%d)\n", rc);
				}
				if (pb_discon == exp.help.exp_rtsp_info.pbtype)
				{
					if (port == exp.help.exp_rtsp_info.loport &&
						port < exp.help.exp_rtsp_info.hiport)
					{
						port = exp.help.exp_rtsp_info.hiport;
					}
					else
					{
						break;
					}
				}
				else
				{
		            port++;
				}
	        }
		}
		
		if (ip_nat_rtsp_hook && exp_succeeded)
		{
			(ip_nat_rtsp_hook)(ct,
					 exp_succeeded,
					 ctinfo,
					 &skb);
		}
    }

    return NF_ACCEPT;
}

/*!
 \description  inbound packet: server->client . Because we changed the client_port in the SETUP message, 
               the client will get a incorrect client_port from the SETUP reply message. So we modified 
               this function to deal with the SETUP reply message correctly.
 \param[in]    skb
 \param[in]    iph
 \param[in]    pdata
 \param[in]    datalen
 \param[in]    ct
 \param[in]    ctinfo
 \return       NF_ACCEPT
 */
static int
	help_in(struct sk_buff *skb, struct iphdr* iph, char* pdata, size_t datalen,
					struct ip_conntrack* ct, enum ip_conntrack_info ctinfo)
{
    uint    dataoff = 0;
    struct tcphdr *tcph = (void *)iph + iph->ihl * 4;
    int dir = CTINFO2DIR(ctinfo);

    struct ip_conntrack_expect exp;
	struct ip_conntrack_expect *exp_succeeded = NULL;
	int rc;

    while (dataoff < datalen)
    {
        uint    hdrsoff = 0;
        uint    hdrslen = 0;
        uint    lineoff = 0;
        uint    linelen = 0;
        uint    off;

        if (!rtsp_parse_message(pdata, datalen, &dataoff,
                                &hdrsoff, &hdrslen))
        {
            break;      /*!< not a valid message */
        }

        memset(&exp, 0, sizeof(exp));
        off = 0;
		rc = 0;
        
        while (nf_mime_nextline(pdata+hdrsoff, hdrslen, &off,
                                &lineoff, &linelen))
        {
            if (linelen == 0)
            {
                break;
            }
            if (off > hdrsoff+hdrslen)
            {
                INFOP("!! overrun !!");
                break;
            }

            if (nf_strncasecmp(pdata+hdrsoff+lineoff, "Transport:", 10) == 0)
            {
				exp.tuple = ct->tuplehash[!dir].tuple;
				exp.tuple.dst.protonum = IPPROTO_UDP;
                rtsp_parse_transport(pdata+hdrsoff+lineoff, linelen,
                                     &exp.help.exp_rtsp_info);
 				exp.seq = ntohl(tcph->seq) + hdrsoff; /*!< mark all the headers */
				exp.help.exp_rtsp_info.len = hdrslen;
#if defined(IP_NF_RTSP_RTT_CARE_SERVER_PORT)
				DEBUGP_DETAIL("client port %hu-%hu server port %hu-%hu\n", 
					exp.help.exp_rtsp_info.loport, exp.help.exp_rtsp_info.hiport,
					exp.help.exp_rtsp_info.loserver, exp.help.exp_rtsp_info.hiserver);
#endif
				exp.help.exp_rtsp_info.loport = rtsp_nat_to_client_pmap(exp.help.exp_rtsp_info.loport, NULL);;
				exp.help.exp_rtsp_info.hiport = rtsp_nat_to_client_pmap(exp.help.exp_rtsp_info.hiport, NULL);
				DEBUGP_DETAIL("exp port [%hu-%hu]\n", exp.help.exp_rtsp_info.loport, exp.help.exp_rtsp_info.hiport);
				rc = 1;
				if (ct->help.ct_rtsp_info.already_mapped)
				{
					DEBUGP_DETAIL("already mapped, we needn't expect again \n");
					exp_succeeded = &exp;
					exp_succeeded->seq = ntohl(tcph->seq) + hdrsoff; /*!< mark all the headers */
					exp_succeeded->help.exp_rtsp_info.len = hdrslen;
					exp_succeeded->tuple = ct->tuplehash[!dir].tuple;
					exp_succeeded->mask.src.ip  = 0xffffffff;
					exp_succeeded->mask.dst.ip  = 0xffffffff;
					exp_succeeded->mask.src.u.udp.port  = 0xffff; /*!< the tuple/mask will be usefull in rtsp_mangle_tran */
#if defined(IP_NF_RTSP_RTT_CARE_SERVER_PORT)
					exp_succeeded->mask.dst.u.udp.port  = 0xffff;
#endif
					exp_succeeded->mask.dst.protonum	= 0xff;
				}
				else
				{
					u_int16_t port =	exp.help.exp_rtsp_info.loport;
#if defined(IP_NF_RTSP_RTT_CARE_SERVER_PORT)
					u_int16_t server =	exp.help.exp_rtsp_info.loserver;
#endif
					while (port <=	exp.help.exp_rtsp_info.hiport) 
					{
						/*!
						 * Allocate expectation for tracking this connection
						 */
						struct ip_conntrack_expect *new_exp = NULL;
						new_exp = ip_conntrack_expect_alloc(ct);  /*!< lmc added the ct parameter */
						if (!new_exp) 
						{
							INFOP("Failed to get a new expectation entry\n");
							return NF_DROP;
						}
						memcpy(new_exp, &exp, sizeof(struct ip_conntrack_expect));
						/*!< lmc added the following two lines because the memcpy above destroys these two parameters */
						new_exp->master = ct;
						atomic_set(&new_exp->use, 1);
						new_exp->seq = ntohl(tcph->seq) + hdrsoff; /*!< mark all the headers */
						new_exp->help.exp_rtsp_info.len = hdrslen;
					
						new_exp->tuple = ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple;
						new_exp->tuple.src.u.udp.port = htons(port);
						DEBUGP_DETAIL("exp "); DUMP_TUPLE(&new_exp->tuple);
						new_exp->tuple.dst.protonum = IPPROTO_UDP;
						new_exp->mask.src.ip  = 0xffffffff;
						new_exp->mask.dst.ip  = 0xffffffff;
						//new_exp->mask.dst.u.udp.port	= (exp.help.exp_rtsp_info.pbtype == pb_range) ? 0xfffe : 0xffff;
						new_exp->mask.src.u.udp.port  = 0xffff; /*!< we expect the src port matched */
#if defined(IP_NF_RTSP_RTT_CARE_SERVER_PORT)
						new_exp->tuple.dst.u.udp.port = htons(server++);
						new_exp->mask.dst.u.udp.port  = 0xffff; /*!< we also expect the dst port matched */
#endif
						new_exp->expectfn = rtt_expect;
						new_exp->mask.dst.protonum	= 0xff;
					
						DEBUGP_DETAIL("expect_related %u.%u.%u.%u:%u-%u.%u.%u.%u:%u\n",
								NIPQUAD(new_exp->tuple.src.ip),
								ntohs(new_exp->tuple.src.u.udp.port),
								NIPQUAD(new_exp->tuple.dst.ip),
								ntohs(new_exp->tuple.dst.u.udp.port));
					
						if (ip_conntrack_expect_related(new_exp) == 0)
						{
							DEBUGP_DETAIL("ip_conntrack_expect_related succeeded loport\n");
							exp_succeeded = new_exp;
						}
						else
						{
							ip_conntrack_expect_put(new_exp);
							DEBUGP_DETAIL("ip_conntrack_expect_related loport failed\n");
						}
						if (pb_discon == exp.help.exp_rtsp_info.pbtype)
						{
							if (port == exp.help.exp_rtsp_info.loport &&
								port < exp.help.exp_rtsp_info.hiport)
							{
								port = exp.help.exp_rtsp_info.hiport;
							}
							else
							{
								break;
							}
						}
						else
						{
							port++;
						}
					}
				}
            }
        }

        if (! rc)
		{
			continue;  /*!< only deal with SETUP reply message */
		}

       /*!
         * Translate the original ports to the NAT ports and note them
         * down to translate back in the return direction.
         */
		if (ip_nat_rtsp_hook && exp_succeeded)
		{
			(ip_nat_rtsp_hook)(ct,
					 exp_succeeded,
					 ctinfo,
					 &skb);
		}

    }

    return NF_ACCEPT;
}

static int
help(struct sk_buff** pskb,
                struct ip_conntrack* ct, enum ip_conntrack_info ctinfo)
{
    uint dataoff;
    struct iphdr *iph = (*pskb)->nh.iph;
    struct tcphdr tcph;
    char* data;
    uint datalen;
	int rc;

	rc = NF_ACCEPT;

    /*!< Until there's been traffic both ways, don't look in packets. */
    if (ctinfo != IP_CT_ESTABLISHED && ctinfo != IP_CT_ESTABLISHED + IP_CT_IS_REPLY)
    {
        DEBUGP_DETAIL("conntrackinfo = %u\n", ctinfo);
        return NF_ACCEPT;
    }

    /*!< Not whole TCP header? */
    if (skb_copy_bits(*pskb, (*pskb)->nh.iph->ihl*4, &tcph, sizeof(tcph)) != 0)
    {
        return NF_ACCEPT;
    }

    /*!< No data? */
    dataoff = (*pskb)->nh.iph->ihl*4 + tcph.doff*4;
    if ( (*pskb)->nh.iph->ihl*4 + tcph.doff*4 >= (*pskb)->len)
    {
        return NF_ACCEPT;
    }

    LOCK_BH(&ip_rtsp_lock);
    skb_copy_bits(*pskb, dataoff, rtsp_buffer, (*pskb)->len - dataoff);
    data = rtsp_buffer;
    datalen = (*pskb)->len - dataoff;
    switch (CTINFO2DIR(ctinfo))
    {
    case IP_CT_DIR_ORIGINAL:
        rc = help_out(*pskb, iph, data, datalen, ct, ctinfo);  /*!< lmc added skb parameter for the ip_ct_refresh() */
        break;
		
    case IP_CT_DIR_REPLY:
        rc = help_in(*pskb, iph, data, datalen, ct, ctinfo);   /*!< lmc changed this function to deal with SETUP reply message */
        break;
		
    default:
        /*!< oops */
        break;
    }
    UNLOCK_BH(&ip_rtsp_lock);

    return rc;
}

static struct ip_conntrack_helper rtsp_helpers[MAX_PORTS];
static char rtsp_names[MAX_PORTS][sizeof("rtsp-65535")];

static void
fini(void)
{
    int i;
 	ip_conntrack_rtsp_data_ports_release_hook = NULL;

	synchronize_net();
	
    for (i = 0; i < ports_c; i++)
    {
        DEBUGP_DETAIL("unregistering port %d\n", ports[i]);
        ip_conntrack_helper_unregister(&rtsp_helpers[i]);
    }
    for (i = 0; i < MAX_PORT_MAPS; i++)
    {
        if (!rtsp_data_ports[i].ct_master)
        {
            continue;
        }
        if (rtsp_data_ports[i].timeout_active == 1) {
            del_timer(&rtsp_data_ports[i].pause_timeout);
        }
    }
	if (rtsp_buffer)
	{
		kfree(rtsp_buffer);
	}
}

static int __init
init(void)
{
    int i, ret;
    struct ip_conntrack_helper *hlpr;
    char *tmpname;

    printk("ip_conntrack_rtsp v" IP_NF_RTSP_VERSION " loading\n");

    if (max_outstanding < 8)
    {
        printk("ip_conntrack_rtsp: max_outstanding must be bigger than 8\n");
        return -EINVAL;
    }
    if (setup_timeout <= 0)
    {
        printk("ip_conntrack_rtsp: setup_timeout must be a positive integer\n");
        return -EINVAL;
    }

	rtsp_buffer = kmalloc(65536, GFP_KERNEL);
	if (!rtsp_buffer)
	{
		return -ENOMEM;
	}

    /*!< If no port given, default to standard rtsp port */
	if (0 == ports_c)
	{
		ports[ports_c++] = RTSP_PORT;
	}

    for (i = 0; i < MAX_PORT_MAPS; i++)
    {
        memset(&rtsp_data_ports[i], 0, sizeof(struct _rtsp_data_ports));
    }

    for (i = 0; i < ports_c; i++)
    {
        hlpr = &rtsp_helpers[i];
        memset(hlpr, 0, sizeof(struct ip_conntrack_helper));
        hlpr->tuple.src.u.tcp.port = htons(ports[i]);
        hlpr->tuple.dst.protonum = IPPROTO_TCP;
        hlpr->mask.src.u.tcp.port = 0xFFFF;
        hlpr->mask.dst.protonum = 0xFF;
        hlpr->max_expected = max_outstanding;
        hlpr->timeout =  setup_timeout;
        /*hlpr->flags = IP_CT_HELPER_F_REUSE_EXPECT;*/ /*!< lmc commented it,this flag no longer used */
        hlpr->me = THIS_MODULE;
        hlpr->help = help;

        tmpname = &rtsp_names[i][0];
        if (RTSP_PORT == ports[i])
        {
            sprintf(tmpname, "rtsp");
        }
        else
        {
            sprintf(tmpname, "rtsp-%d", ports[i]);
        }
        hlpr->name = tmpname;

        DEBUGP_DETAIL("port #%d: %d\n", i, ports[i]);

        ret = ip_conntrack_helper_register(hlpr);

        if (ret)
        {
            printk("ip_conntrack_rtsp: ERROR registering port %d\n", ports[i]);
            fini();
            return ret;
        }
    }
#ifdef IP_NF_RTSP_DEBUG
	if (debug_mode & 2)
#endif
	{
		ip_conntrack_rtsp_data_ports_release_hook = ip_conntrack_rtsp_data_ports_release;
	}
    return 0;
}

PROVIDES_CONNTRACK(rtsp);
EXPORT_SYMBOL(ip_rtsp_lock);

module_init(init);
module_exit(fini);
