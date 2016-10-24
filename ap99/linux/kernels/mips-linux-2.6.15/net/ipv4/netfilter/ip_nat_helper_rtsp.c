/*!
 * RTSP extension for TCP NAT alteration
 * (C) 2003 by Tom Marshall <tmarshall@real.com>
 * based on ip_nat_irc.c
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 * Module load syntax:
 *      insmod ip_nat_rtsp.o 
 *                           stunaddr=<address>
 *                           destaction=[auto|strip|none]
 *
 * If no ports are specified, the default will be port 554 only.
 *
 * stunaddr specifies the address used to detect that a client is using STUN.
 * If this address is seen in the destination parameter, it is assumed that
 * the client has already punched a UDP hole in the firewall, so we don't
 * mangle the client_port.  If none is specified, it is autodetected.  It
 * only needs to be set if you have multiple levels of NAT.  It should be
 * set to the external address that the STUN clients detect.  Note that in
 * this case, it will not be possible for clients to use UDP with servers
 * between the NATs.
 *
 * If no destaction is specified, auto is used.
 *   destaction=auto:  strip destination parameter if it is not stunaddr.
 *   destaction=strip: always strip destination parameter (not recommended).
 *   destaction=none:  do not touch destination parameter (not recommended).
 */
/*!
 * history 
 * 		from Broadcom xDSL platform 
 * modification
 * 		cancel the module parameter 'ports' because we no longer use the ip_nat_helper
 * 		modify help_in to deal with SETUP reply message correctly
 */

#include <linux/module.h>
#include <linux/netfilter_ipv4.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/kernel.h>
#include <net/tcp.h>
#include <linux/netfilter_ipv4/ip_nat.h>
#include <linux/netfilter_ipv4/ip_nat_helper.h>
#include <linux/netfilter_ipv4/ip_nat_rule.h>
#include <linux/netfilter_ipv4/ip_conntrack_rtsp.h>
#include <linux/netfilter_ipv4/ip_nat_rtsp.h>
#include <linux/netfilter_ipv4/ip_conntrack_helper.h>

#include <linux/inet.h>
#include <linux/ctype.h>
#define NF_NEED_STRNCASECMP
#define NF_NEED_STRTOU16
#include <linux/netfilter_helpers.h>
#define NF_NEED_MIME_NEXTLINE
#include <linux/netfilter_mime.h>

MODULE_AUTHOR("Tom Marshall <tmarshall@real.com>");
MODULE_DESCRIPTION("RTSP network address translation module");
MODULE_LICENSE("GPL");
/*!
 * To enable debugging, replace the line below with #define IP_NF_RTSP_DEBUG 1
 */
#undef IP_NF_RTSP_DEBUG
#define INFOP(args...) printk(args)
#ifdef IP_NF_RTSP_DEBUG
#define DEBUGP(args...) printk(args)
#define DEBUGP_DETAIL(args...) printk("%s:%s(%d) ", __FILE__, __FUNCTION__, __LINE__); \
					           printk(args)
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

#define DSTACT_AUTO     0
#define DSTACT_STRIP    1
#define DSTACT_NONE     2
#define MAX_NAT_PORTS   16

static char*        stunaddr = NULL;
module_param(stunaddr, charp, 0600);
MODULE_PARM_DESC(stunaddr, "Address for detecting STUN");

static char*        destaction = NULL;
module_param(destaction, charp, 0600);
MODULE_PARM_DESC(destaction, "Action for destination parameter (auto/strip/none)");

static u_int32_t    extip = 0;
static int          dstact = 0;

#define SKIP_WSPACE(ptr,len,off) while(off < len && isspace(*(ptr+off))) { off++; }

/*!< helper functions */

static void *
rtsp_nat_find_char(void *str, int ch, size_t len)
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

static void
get_skb_tcpdata(struct sk_buff* skb, char** pptcpdata, uint* ptcpdatalen)
{
    struct iphdr*   iph  = (struct iphdr*)skb->nh.iph;
    struct tcphdr*  tcph = (struct tcphdr*)((char*)iph + iph->ihl*4);

    *pptcpdata = (char*)tcph + tcph->doff*4;
    *ptcpdatalen = ((char*)skb->h.raw + skb->len) - *pptcpdata;
}

/*!< nat functions */

/*!
 * Mangle the "Transport:" header:
 *   - Replace all occurences of "client_port=<spec>"
 *   - Handle destination parameter
 *
 * In:
 *   ct, ctinfo = conntrack context
 *   pskb       = packet
 *   tranoff    = Transport header offset from TCP data
 *   tranlen    = Transport header length (incl. CRLF)
 *   rport_lo   = replacement low  port (host endian)
 *   rport_hi   = replacement high port (host endian)
 *
 * Returns packet size difference.
 *
 * Assumes that a complete transport header is present, ending with CR or LF
 */
static int
rtsp_mangle_tran(struct ip_conntrack* ct, enum ip_conntrack_info ctinfo,
                 struct ip_conntrack_expect* exp,
                 struct sk_buff** pskb, uint tranoff, uint tranlen)
{
    char*       ptcp;
    uint        tcplen;
    char*       ptran;
    char        rbuf1[16];      /*!< Replacement buffer (one port) */
    uint        rbuf1len;       /*!< Replacement len (one port) */
    char        rbufa[16];      /*!< Replacement buffer (all ports) */
    uint        rbufalen;       /*!< Replacement len (all ports) */
    u_int32_t   newip;
    u_int16_t   loport, hiport;
    uint        off = 0;
    uint        diff;           /*!< Number of bytes we removed */

    struct ip_ct_rtsp_expect* prtspexp = &exp->help.exp_rtsp_info;
    struct ip_conntrack_tuple t;

    char        szextaddr[15+1];
    uint        extaddrlen;
    int         is_stun;

    get_skb_tcpdata(*pskb, &ptcp, &tcplen);
    ptran = ptcp+tranoff;

    if (tranoff+tranlen > tcplen || tcplen-tranoff < tranlen ||
        tranlen < 10 || !iseol(ptran[tranlen-1]) ||
        nf_strncasecmp(ptran, "Transport:", 10) != 0)
    {
        INFOP("sanity check failed\n");
        return 0;
    }
    off += 10;
    SKIP_WSPACE(ptcp+tranoff, tranlen, off);

    int dir = CTINFO2DIR(ctinfo);
    newip = ct->tuplehash[!dir].tuple.dst.ip;

    extaddrlen = extip ? sprintf(szextaddr, "%u.%u.%u.%u", NIPQUAD(extip))
                       : sprintf(szextaddr, "%u.%u.%u.%u", NIPQUAD(newip));
    DEBUGP_DETAIL("stunaddr=%s (%s)\n", szextaddr, (extip?"forced":"auto"));
    /*DEBUGP_DETAIL("Found Transport message %s\n", ptran);*/

    rbuf1len = rbufalen = 0;
    switch (prtspexp->pbtype)
    {
    case pb_single:
        loport = prtspexp->loport;
        DEBUGP_DETAIL("PB_SINGLE: LO_PORT %hu\n", loport);
        if (loport != 0)
        {
            rbuf1len = sprintf(rbuf1, "%hu", loport);
            rbufalen = sprintf(rbufa, "%hu", loport);
        }
        break;
		
    case pb_range:
        loport = prtspexp->loport;
        DEBUGP_DETAIL("PB_RANGE: LO_PORT %hu\n", loport);
        if (loport != 0)
        {
            rbuf1len = sprintf(rbuf1, "%hu", loport);
            rbufalen = sprintf(rbufa, "%hu-%hu", loport, loport+1);
            DEBUGP_DETAIL("MANGLING to ports (%hu-%hu) rbuf1 %s rbufa %s\n", prtspexp->loport, prtspexp->loport+1,
                    rbuf1, rbufa);
        }
        break;
		
    case pb_discon:
        DEBUGP_DETAIL("PB_DISCON:n");
		t = exp->tuple;
		t.dst.ip = newip;
        for (loport = prtspexp->loport; loport != 0; loport++) /*!< XXX: improper wrap? */
        {
            DEBUGP_DETAIL("Original UDP PORT value is %hu exp loport %hu hiport %hu\n", t.dst.u.udp.port,
                    prtspexp->loport, prtspexp->hiport);
            // Do not transpose the ports yet. If you do, you better register a helper
            // to mangle them correctly when you receive packets on those ports.
            //t.dst.u.udp.port = htons(loport);
            if (ip_conntrack_change_expect(exp, &t) == 0)
            {
                DEBUGP_DETAIL("using port %hu (1 of 2)\n", loport);
                break;
            }
        }
		DEBUGP_DETAIL("exp  "); DUMP_TUPLE(&exp->tuple);
		DEBUGP_DETAIL("new  "); DUMP_TUPLE(&t);
		DEBUGP_DETAIL("mask "); DUMP_TUPLE(&exp->mask);
		DEBUGP_DETAIL("ct_"); DUMP_TUPLE(&exp->ct_tuple);
		DEBUGP_DETAIL("dir %d newip %u.%u.%u.%u\n", dir, NIPQUAD(newip));
        for (hiport = prtspexp->hiport; hiport != 0; hiport++) /*!< XXX: improper wrap? */
        {
			if (dir)
			{
	            t.src.u.udp.port = htons(hiport);
			}
			else
			{
	            t.dst.u.udp.port = htons(hiport);
			}
            if (ip_conntrack_change_expect(exp, &t) == 0)
            {
                DEBUGP_DETAIL("using port %hu (2 of 2)\n", hiport);
                break;
            }
        }
        if (loport != 0 && hiport != 0)
        {
            rbuf1len = sprintf(rbuf1, "%hu", loport);
            if (hiport == loport+1)
            {
                rbufalen = sprintf(rbufa, "%hu-%hu", loport, hiport);
                DEBUGP_DETAIL("Ports %hu-%hu\n", loport, hiport);
            }
            else
            {
                rbufalen = sprintf(rbufa, "%hu/%hu", loport, hiport);
                DEBUGP_DETAIL("ports %hu-%hu\n", loport, hiport);
            }
        }
        break;
		
    default:
        /*!< oops */
        break;
    }

    if (rbuf1len == 0)
    {
        DEBUGP_DETAIL("Cannot get replacement ports\n");
        return 0;   /*!< cannot get replacement port(s) */
    }

    /*!< Transport: tran;field;field=val,tran;field;field=val,... */
    while (off < tranlen)
    {
        uint        saveoff;
        const char* pparamend;
        uint        nextparamoff;

        pparamend = rtsp_nat_find_char(ptran+off, ',', tranlen-off);
        pparamend = (pparamend == NULL) ? ptran+tranlen : pparamend+1;
        nextparamoff = pparamend-ptran;
		DEBUGP_DETAIL("off %d nextfieldoff ?? nextparamoff %d tranlen %d\n", off, nextparamoff, tranlen);

        /*!
         * We pass over each param twice.  On the first pass, we look for a
         * destination= field.  It is handled by the security policy.  If it
         * is present, allowed, and equal to our external address, we assume
         * that STUN is being used and we leave the client_port= field alone.
         */
        is_stun = 0;
        saveoff = off;
        while (off < nextparamoff)
        {
            const char* pfieldend;
            uint        nextfieldoff;

            pfieldend = rtsp_nat_find_char(ptran+off, ';', nextparamoff-off);
            nextfieldoff = (pfieldend == NULL) ? nextparamoff : pfieldend-ptran+1;
			DEBUGP_DETAIL("off %d nextfieldoff %d \n", off, nextfieldoff);

            if (dstact != DSTACT_NONE && strncmp(ptran+off, "destination=", 12) == 0)
            {
                if (strncmp(ptran+off+12, szextaddr, extaddrlen) == 0)
                {
                    is_stun = 1;
                }
                if (dstact == DSTACT_STRIP || (dstact == DSTACT_AUTO && !is_stun))
                {
                    diff = nextfieldoff-off;
                    if (!ip_nat_mangle_tcp_packet(pskb, ct, ctinfo,
                                                         off, diff, NULL, 0))
                    {
                        /*!< mangle failed, all we can do is bail */
                        DEBUGP_DETAIL("mangle failed bailing out now\n");
                        return 0;
                    }
                    get_skb_tcpdata(*pskb, &ptcp, &tcplen);
                    ptran = ptcp+tranoff;
                    tranlen -= diff;
                    nextparamoff -= diff;
                    nextfieldoff -= diff;
                }
            }

            off = nextfieldoff;
        }
        if (is_stun)
        {
            continue;
        }
        off = saveoff;
        while (off < nextparamoff)
        {
            const char* pfieldend;
            uint        nextfieldoff;

            pfieldend = rtsp_nat_find_char(ptran+off, ';', nextparamoff-off);
            nextfieldoff = (pfieldend == NULL) ? nextparamoff : pfieldend-ptran+1;

			DEBUGP_DETAIL("off %d nextfieldoff %d \n", off, nextfieldoff);
            if (strncmp(ptran+off, "client_port=", 12) == 0)
            {
                u_int16_t   port;
                uint        numlen;
                uint        origoff;
                uint        origlen;
                char*       rbuf    = rbuf1;
                uint        rbuflen = rbuf1len;

                off += 12;
                origoff = (ptran-ptcp)+off;
                origlen = 0;
                numlen = nf_strtou16(ptran+off, &port);
                off += numlen;
                origlen += numlen;
                DEBUGP_DETAIL("Checking port %hu expect port %hu rbufa %s rbufalen %d\n",
                       port, prtspexp->loport, rbufa, rbufalen);
                if (ptran[off] == '-' || ptran[off] == '/')
                {
                    off++;
                    origlen++;
                    numlen = nf_strtou16(ptran+off, &port);
                    off += numlen;
                    origlen += numlen;
                    rbuf = rbufa;
                    rbuflen = rbufalen;
                }

                /*!
                 * note we cannot just memcpy() if the sizes are the same.
                 * the mangle function does skb resizing, checks for a
                 * cloned skb, and updates the checksums.
                 *
                 * parameter 4 below is offset from start of tcp data.
                 */
                diff = origlen-rbuflen;
                DEBUGP_DETAIL("Begin mangling: rbuf %s\n", rbuf);
                if (!ip_nat_mangle_tcp_packet(pskb, ct, ctinfo,
                                          origoff, origlen, rbuf, rbuflen))
                {
                    /*!< mangle failed, all we can do is bail */
                    DEBUGP_DETAIL("MANGLE Failed\n");
                    return 0;
                }
                get_skb_tcpdata(*pskb, &ptcp, &tcplen);
                ptran = ptcp+tranoff;
                tranlen -= diff;
                nextparamoff -= diff;
                nextfieldoff -= diff;
                DEBUGP_DETAIL("After mangle nextfieldoff %d nextparamoff %d next '%.*s'\n",
                       nextfieldoff, nextparamoff, tranlen-nextparamoff, ptran+nextparamoff);
                break;
            }
            off = nextfieldoff;
        }

		off = nextparamoff;
    }

    return 1;
}

static uint
help_out(struct ip_conntrack* ct, enum ip_conntrack_info ctinfo,
                struct ip_conntrack_expect* exp, struct sk_buff** pskb)
{
    char*   ptcp;
    uint    tcplen;
    uint    hdrsoff;
    uint    hdrslen;
    uint    lineoff;
    uint    linelen;
    uint    off;

    struct iphdr* iph = (struct iphdr*)(*pskb)->nh.iph;
    struct tcphdr* tcph = (struct tcphdr*)((void*)iph + iph->ihl*4);

    struct ip_ct_rtsp_expect* prtspexp = &exp->help.exp_rtsp_info;

    get_skb_tcpdata(*pskb, &ptcp, &tcplen);

    hdrsoff = exp->seq - ntohl(tcph->seq);
    hdrslen = prtspexp->len;
    off = hdrsoff;
    DEBUGP_DETAIL("orig "); DUMP_TUPLE(&ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple);
    DEBUGP_DETAIL("repl "); DUMP_TUPLE(&ct->tuplehash[IP_CT_DIR_REPLY].tuple);

    DEBUGP_DETAIL("SRC IP %u.%u.%u.%u DST IP %u.%u.%u.%u client_port:%hu-%hu\n",
           NIPQUAD(iph->saddr), NIPQUAD(iph->daddr), 
           prtspexp->loport, prtspexp->hiport);

    while (nf_mime_nextline(ptcp, hdrsoff+hdrslen, &off, &lineoff, &linelen))
    {
        if (linelen == 0)
        {
            break;
        }
        if (off > hdrsoff+hdrslen)
        {
            INFOP("!! overrun !!\n");
            break;
        }
        DEBUGP_DETAIL("hdr: len=%u, %.*s", linelen, (int)linelen, ptcp+lineoff);

        if (nf_strncasecmp(ptcp+lineoff, "Transport:", 10) == 0)
        {
            uint oldtcplen = tcplen;
            if (!rtsp_mangle_tran(ct, ctinfo, exp, pskb, lineoff, linelen))
            {
                break;
            }
            get_skb_tcpdata(*pskb, &ptcp, &tcplen);
            hdrslen -= (oldtcplen-tcplen);
            off -= (oldtcplen-tcplen);
            lineoff -= (oldtcplen-tcplen);
            linelen -= (oldtcplen-tcplen);
            DEBUGP_DETAIL("rep: len=%u, %.*s", linelen, (int)linelen, ptcp+lineoff);
        }
    }
    DEBUGP_DETAIL("SRC IP %u.%u.%u.%u:%hu DST IP %u.%u.%u.%u:%hu)\n",
           NIPQUAD(iph->saddr), tcph->source, NIPQUAD(iph->daddr), tcph->dest);

    return NF_ACCEPT;
}

/*!
 *  help_in is modified to deal with the SETUP reply message correctly,
 *  it will insert expect to the conntrack, these expectations will expect
 *  a UDP packet from the client
 */
static uint
help_in(struct ip_conntrack* ct, enum ip_conntrack_info ctinfo,
                struct ip_conntrack_expect* exp, struct sk_buff** pskb)
{
    char*   ptcp;
    uint    tcplen;
    uint    hdrsoff;
    uint    hdrslen;
    uint    lineoff;
    uint    linelen;
    uint    off;

    struct iphdr* iph = (struct iphdr*)(*pskb)->nh.iph;
    struct tcphdr* tcph = (struct tcphdr*)((void*)iph + iph->ihl*4);

    struct ip_ct_rtsp_expect* prtspexp = &exp->help.exp_rtsp_info;

    get_skb_tcpdata(*pskb, &ptcp, &tcplen);

    hdrsoff = exp->seq - ntohl(tcph->seq);
    hdrslen = prtspexp->len;
    off = hdrsoff;

    while (nf_mime_nextline(ptcp, hdrsoff+hdrslen, &off, &lineoff, &linelen))
    {
        if (linelen == 0)
        {
            break;
        }
        if (off > hdrsoff+hdrslen)
        {
            INFOP("!! overrun !!\n");
            break;
        }
        DEBUGP_DETAIL("hdr: len=%u, %.*s", linelen, (int)linelen, ptcp+lineoff);

        if (nf_strncasecmp(ptcp+lineoff, "Transport:", 10) == 0)
        {
            uint oldtcplen = tcplen;
            if (!rtsp_mangle_tran(ct, ctinfo, exp, pskb, lineoff, linelen))
            {
                break;
            }
            get_skb_tcpdata(*pskb, &ptcp, &tcplen);
            hdrslen -= (oldtcplen-tcplen);
            off -= (oldtcplen-tcplen);
            lineoff -= (oldtcplen-tcplen);
            linelen -= (oldtcplen-tcplen);
            DEBUGP_DETAIL("rep: len=%u, %.*s", linelen, (int)linelen, ptcp+lineoff);
        }
    }

    return NF_ACCEPT;
}

static unsigned int
ip_nat_rtsp(struct ip_conntrack* ct,
     struct ip_conntrack_expect* exp,
     enum ip_conntrack_info ctinfo,
     struct sk_buff** pskb)
{
    int dir;
    int rc = NF_ACCEPT;
#ifdef IP_NF_RTSP_DEBUG
	struct iphdr*  iph	= (struct iphdr*)(*pskb)->nh.iph;
	struct tcphdr* tcph = (struct tcphdr*)((char*)iph + iph->ihl * 4);
#endif

    DEBUGP_DETAIL("SRC IP %u.%u.%u.%u:%hu DST IP %u.%u.%u.%u:%hu\n",
           NIPQUAD(iph->saddr), tcph->source,
          NIPQUAD(iph->daddr),  tcph->dest);
    if (ct == NULL || exp == NULL || pskb == NULL)
    {
        DEBUGP_DETAIL("!! null ptr (%p,%p,%p) !!\n", ct, exp, pskb);
        return NF_ACCEPT;
    }

    /*!
     * Only mangle things once: original direction in POST_ROUTING
     * and reply direction on PRE_ROUTING.
     */
    dir = CTINFO2DIR(ctinfo);

    switch (dir)
    {
    case IP_CT_DIR_ORIGINAL:
		rc = help_out(ct, ctinfo, exp, pskb);
        break;
		
    case IP_CT_DIR_REPLY:
        rc = help_in(ct, ctinfo, exp, pskb);
        break;
		
    default:
        /*!< oops */
        break;
    }

    return rc;
}

/*!< This function is intentionally _NOT_ defined as  __exit */
static void
fini(void)
{
	ip_nat_rtsp_hook = NULL;
	/*!< Make sure noone calls it, meanwhile. */
	synchronize_net();
    printk("ip_nat_rtsp v" IP_NF_RTSP_VERSION " exit\n");
}

static int __init
init(void)
{
    printk("ip_nat_rtsp v" IP_NF_RTSP_VERSION " loading\n");
	BUG_ON(ip_nat_rtsp_hook);
    if (stunaddr != NULL)
    {
        extip = in_aton(stunaddr);
    }
    if (destaction != NULL)
    {
        if (strcmp(destaction, "auto") == 0)
        {
            dstact = DSTACT_AUTO;
        }
        if (strcmp(destaction, "strip") == 0)
        {
            dstact = DSTACT_STRIP;
        }
        if (strcmp(destaction, "none") == 0)
        {
            dstact = DSTACT_NONE;
        }
    }
	ip_nat_rtsp_hook = ip_nat_rtsp;
	return 0;
}

NEEDS_CONNTRACK(rtsp);

module_init(init);
module_exit(fini);
