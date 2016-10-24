/*
 * RTSP extension for IP connection tracking.
 * (C) 2003 by Tom Marshall <tmarshall@real.com>
 * based on ip_conntrack_irc.h
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */
#ifndef _IP_CONNTRACK_RTSP_H
#define _IP_CONNTRACK_RTSP_H

#define IP_NF_RTSP_VERSION "0.01"

/* port block types */
typedef enum {
    pb_single,  /* client_port=x */
    pb_range,   /* client_port=x-y */
    pb_discon   /* client_port=x/y (rtspbis) */
} portblock_t;

/* We record seq number and length of rtsp headers here, all in host order. */

/*
 * This structure is per expected connection.  It is a member of struct
 * ip_conntrack_expect.  The TCP SEQ for the conntrack expect is stored
 * there and we are expected to only store the length of the data which
 * needs replaced.  If a packet contains multiple RTSP messages, we create
 * one expected connection per message.
 *
 * We use these variables to mark the entire header block.  This may seem
 * like overkill, but the nature of RTSP requires it.  A header may appear
 * multiple times in a message.  We must treat two Transport headers the
 * same as one Transport header with two entries.
 */
/*!
 * if the rtt_expect cares about the server port, replace the line below with
 * #define IP_NF_RTSP_RTT_CARE_SERVER_PORT 1 
 */
#undef IP_NF_RTSP_RTT_CARE_SERVER_PORT
struct ip_ct_rtsp_expect
{
    u_int32_t   len;        /* length of header block */
    portblock_t pbtype;     /* Type of port block that was requested */
    u_int16_t   loport;     /* Port that was requested, low or first */
    u_int16_t   hiport;     /* Port that was requested, high or second */
#if defined(IP_NF_RTSP_RTT_CARE_SERVER_PORT)
    u_int16_t   loserver;   /* Port that was requested, low or first */
    u_int16_t   hiserver;   /* Port that was requested, high or second */
#endif
#if 0
    uint        method;     /* RTSP method */
    uint        cseq;       /* CSeq from request */
#endif
};

/* This structure exists only once per master */
struct ip_ct_rtsp_master
{
	int      already_mapped; /* the second SETUP message uses the same client_port as the first,we needn't expect again */
};


#ifdef __KERNEL__

#include <linux/netfilter_ipv4/lockhelp.h>

#define RTSP_PORT   554

/* Protects rtsp part of conntracks */
DECLARE_LOCK_EXTERN(ip_rtsp_lock);

#endif /* __KERNEL__ */

#endif /* _IP_CONNTRACK_RTSP_H */
