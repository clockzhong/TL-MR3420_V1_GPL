#ifndef _NET80211_IEEE80211_MULTICLIENT_H_
#define _NET80211_IEEE80211_MULTICLIENT_H_

#ifdef	CLIENT3_SUPPORT

#define IEEE80211_CLIENT_HASHSIZE	32

#define   IEEE80211_ARP_HASH(addr)		\
	addr % IEEE80211_CLIENT_HASHSIZE
#define   IEEE80211_UID_HASH(addr)		\
	(*(addr+3)) % IEEE80211_CLIENT_HASHSIZE
	
#define	MAP_ENTRY_AGEOUT_TIME	 30000

#define ARP_HW_ETHER       0x0001

#define PPPOE_CODE_PADI			0x09
#define PPPOE_CODE_PADO			0x07
#define PPPOE_CODE_PADR			0x19
#define PPPOE_CODE_PADS			0x65
#define PPPOE_CODE_PADT			0xa7
#define PPPOE_TAG_ID_HOST_UNIQ	0x0103
#define PPPOE_TAG_ID_AC_COOKIE	0x0104

#define PPPOE_DIS_UID_LEN		6

#ifndef  __ATTRIB_PACK
#define __ATTRIB_PACK __attribute__ ((packed))
#endif

struct ieee80211_arp_packet
{
	u_int16_t	  ar_hrd;
	u_int16_t	  ar_pro;
	u_int8_t   ar_hln;
	u_int8_t   ar_pln;
	u_int16_t	  ar_op;

	u_int8_t   ar_sha[IEEE80211_ADDR_LEN];
	u_int32_t  ar_sip;
	u_int8_t   ar_tha[IEEE80211_ADDR_LEN];
	u_int32_t  ar_dip;
} __ATTRIB_PACK;

struct ieee80211_arp_entry {
	LIST_ENTRY(ieee80211_arp_entry)	arp_hash;
	u_int32_t			ipaddr;
	u_int8_t			peermac[IEEE80211_ADDR_LEN];
	unsigned long		lasttime;
} ;

struct ieee80211_arp_table {
	ATH_LIST_HEAD(, ieee80211_arp_entry) at_hash[IEEE80211_CLIENT_HASHSIZE];	
};

struct ieee80211_pppoe_entry {
	LIST_ENTRY(ieee80211_pppoe_entry)	pppoe_hash;
	u_int16_t			sessionID;
	u_int8_t			servmac[IEEE80211_ADDR_LEN];
	u_int8_t			peermac[IEEE80211_ADDR_LEN];
	unsigned long		lasttime;
};

struct ieee80211_pppoe_table {
	ATH_LIST_HEAD(, ieee80211_pppoe_entry) pppoe_hash[IEEE80211_CLIENT_HASHSIZE];	
};

struct ieee80211_uid_entry
{
	LIST_ENTRY(ieee80211_uid_entry)	uid_hash;
	u_int8_t 			isServer;
	u_int8_t 			uIDAddByUs;				 // If the host-uniq or AC-cookie is add by our driver, set it as 1, else set as 0.
	u_int8_t 			uIDStr[PPPOE_DIS_UID_LEN]; // String used for identify who sent this pppoe packet in discovery stage.
	u_int8_t			 peermac[IEEE80211_ADDR_LEN];	 // Mac address associated to this uid string.
	unsigned long 		lasttime;
};

struct ieee80211_uid_table {
	ATH_LIST_HEAD(, ieee80211_uid_entry) uid_hash[IEEE80211_CLIENT_HASHSIZE];	
};


int		   
ieee80211_update_arp_table(struct ieee80211_arp_table * , u_int32_t, const u_int8_t *);
struct ieee80211_uid_entry *
ieee80211_update_uid_table(struct ieee80211_uid_table *ut , u_int8_t *mac_in, u_int8_t *mac_out,
	u_int8_t *taginfo, u_int32_t taglen, u_int32_t isServer);
int		   
ieee80211_update_pppoe_table(struct ieee80211_pppoe_table *pt , 
	u_int16_t sid, const u_int8_t *servmac, const u_int8_t *macaddr);
u_int8_t *   
ieee80211_find_arpmap(struct ieee80211_arp_table * , const u_int32_t);
u_int8_t *   
ieee80211_find_pppoemap(struct ieee80211_pppoe_table * , const u_int16_t, const u_int8_t *);
struct ieee80211_uid_entry * 
ieee80211_find_uidmap(struct ieee80211_uid_table * ut, const u_int8_t *taginfo, u_int32_t taglen);

void	ieee80211_updateSendIpPacket(struct sk_buff *skb, struct net_device *dev);
struct sk_buff *ieee80211_updateSendArpPacket(struct sk_buff *skb, struct net_device *dev);
struct sk_buff *ieee80211_updateSendPppoeDPacket(struct sk_buff *skb, struct net_device *dev);
void 	ieee80211_updateSendPppoeSPacket(struct sk_buff *skb, struct net_device *dev);
void ieee80211_updateRecvIpPacket(struct ieee80211_node *ni, struct sk_buff *skb);
void ieee80211_updateRecvArpPacket(struct ieee80211_node *ni, struct sk_buff *skb);
void ieee80211_updateRecvPppoeDPacket(struct ieee80211_node *ni, struct sk_buff *skb);
void ieee80211_updateRecvPppoeSPacket(struct ieee80211_node *ni, struct sk_buff *skb);

void ieee80211_check_nowds(struct ieee80211_node *ni, struct sk_buff *skb);
void ieee80211_check_wds(struct ieee80211_node *ni, struct sk_buff *skb);

void ieee80211_multiclient_attach(struct ieee80211com *ic);
void ieee80211_multiclient_detach(struct ieee80211com *ic);

/*
                  1                 2               3             4
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |  VER  | TYPE  |      CODE     |          SESSION_ID           |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |            LENGTH             |           payload             ~
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

	VER = 0x1, TYPE =0x1
	
PPPoE Discovery Stage(Ethernet protocol type = 0x8863):
	PADI:
		DESTINATION_ADDR = 0xffffffff
		CODE = 0x09, SESSION_ID = 0x0000
		LENGTH = payload length

	PADO:
		DESTINATION_ADDR = Unicast Ethernet address of sender
		CODE = 0x07, SESSION_ID = 0x0000
		LENGTH = payload length
		NEcessary TAGS: AC-NAME(0x0102), Sevice-Name(0x0101), and other service names.

		Note: if the PPPoE server cannot serve the PADI it MUST NOT respond with a PADO

	
	PADR:
		DESTINATION_ADDR = unicast Ethernet address 
		CODE = 0x19, SESSION_ID = 0x0000
		LENGTH = payload length
		Necessary TAGS: Service-Name(0x0101)
		Optional TAGS: ....

	PADS:
		If success:
			DESTINATION_ADDR = unicast Ethernet address 
			CODE = 0x65, SESSION_ID = unique value for this pppoe session.(16 bits)
			LENGHT - payload length
			Necessary TAGS: Service-Name(0x0101)

		if failed:
			SESSION_ID = 0x0000
			Necessary TAGS: Service-Name-Error(0x0201).

	PADT:
		DESTINATION_ADDR = unicast Ethernet address
		CODE = 0xa7, SESSION_ID = previous assigned 16 bits session ID.
		Necessary TAGS: NO.

PPPoE Session Stage(Ethernet protocol type = 0x8864):
	PPP data:
		DESTINATION_ADDR = unicast Ethernet address
		CODE = 0x00, 
	LCP:
		DESTINATION_ADDR = unicast Ethernet address
		CODE = 0x00, 

*/

#endif /*CLIENT3_SUPPORT*/

#endif /*NET80211_IEEE80211_MULTICLIENT_H*/

