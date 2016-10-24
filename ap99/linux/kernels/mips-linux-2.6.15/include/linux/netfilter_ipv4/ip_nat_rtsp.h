#ifndef _IP_NAT_RTSP_H
#define _IP_NAT_RTSP_H
/* RTSP extension for UDP NAT alteration. */

#ifndef __KERNEL__
#error Only in kernel.
#endif

/* Protects RTSP part of conntracks */
DECLARE_LOCK_EXTERN(ip_rtp_lock);

extern unsigned int (*ip_nat_rtsp_hook)(struct ip_conntrack* ct,
		 struct ip_conntrack_expect* exp,
		 enum ip_conntrack_info ctinfo,
		 struct sk_buff** pskb);

#endif /* _IP_NAT_RTSP_H */
