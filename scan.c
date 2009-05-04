#include <net/if.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <netlink/msg.h>
#include <netlink/attr.h>

#include "nl80211.h"
#include "iw.h"

#define WLAN_CAPABILITY_ESS		(1<<0)
#define WLAN_CAPABILITY_IBSS		(1<<1)
#define WLAN_CAPABILITY_CF_POLLABLE	(1<<2)
#define WLAN_CAPABILITY_CF_POLL_REQUEST	(1<<3)
#define WLAN_CAPABILITY_PRIVACY		(1<<4)
#define WLAN_CAPABILITY_SHORT_PREAMBLE	(1<<5)
#define WLAN_CAPABILITY_PBCC		(1<<6)
#define WLAN_CAPABILITY_CHANNEL_AGILITY	(1<<7)
#define WLAN_CAPABILITY_SPECTRUM_MGMT	(1<<8)
#define WLAN_CAPABILITY_QOS		(1<<9)
#define WLAN_CAPABILITY_SHORT_SLOT_TIME	(1<<10)
#define WLAN_CAPABILITY_APSD		(1<<11)
#define WLAN_CAPABILITY_DSSS_OFDM	(1<<13)

static unsigned char wifi_oui[3]      = { 0x00, 0x50, 0xf2 };
static unsigned char ieee80211_oui[3] = { 0x00, 0x0f, 0xac };

struct scan_params {
	bool unknown;
};

static int handle_scan(struct nl80211_state *state,
		       struct nl_cb *cb,
		       struct nl_msg *msg,
		       int argc, char **argv)
{
	struct nl_msg *ssids = NULL;
	int err = -ENOBUFS;

	ssids = nlmsg_alloc();
	if (!ssids)
		return -ENOMEM;
	NLA_PUT(ssids, 1, 0, "");
	nla_put_nested(msg, NL80211_ATTR_SCAN_SSIDS, ssids);

	err = 0;
 nla_put_failure:
	nlmsg_free(ssids);
	return err;
}
COMMAND(scan, trigger, NULL,
	NL80211_CMD_TRIGGER_SCAN, 0, CIB_NETDEV, handle_scan);

typedef void (*printfn)(unsigned char type, unsigned char len, unsigned char *data);

static void tab_on_first(bool *first)
{
	if (!*first)
		printf("\t");
	else
		*first = false;
}

static void print_ssid(unsigned char type, unsigned char len, unsigned char *data)
{
	int i;
	printf("\tSSID: ");
	for (i=0; i<len; i++) {
		if (isprint(data[i]))
			printf("%c", data[i]);
		else
			printf("\\x%.2x", data[i]);
	}
	printf("\n");
}

static void print_supprates(unsigned char type, unsigned char len, unsigned char *data)
{
	int i;

	if (type == 1)
		printf("\tSupported rates: ");
	else
		printf("\tExtended supported rates: ");

	for (i=0; i<len; i++) {
		int r = data[i] & 0x7f;
		printf("%d.%d%s ", r/2, 5*(r&1), data[i] & 0x80 ? "*":"");
	}
	printf("\n");
}

static void print_ds(unsigned char type, unsigned char len, unsigned char *data)
{
	printf("\tDS Parameter set: channel %d\n", data[0]);
}

static void print_ign(unsigned char type, unsigned char len, unsigned char *data)
{
	/* ignore for now, not too useful */
}

static void print_country(unsigned char type, unsigned char len, unsigned char *data)
{
	int i;

	printf("\tCountry: %.*s", 2, data);
	switch (data[2]) {
	case 'I':
		printf(" (indoor)");
		break;
	case 'O':
		printf(" (outdoor)");
		break;
	case ' ':
		printf(" (in/outdoor)");
		break;
	default:
		printf(" (invalid environment)");
		break;
	}
	printf(", data:");
	for(i=0; i<len-3; i++)
		printf(" %.02x", data[i + 3]);
	printf("\n");
}

static void print_erp(unsigned char type, unsigned char len, unsigned char *data)
{
	if (data[0] == 0x00)
		return;

	printf("\tERP:");
	if (data[0] & 0x01)
		printf(" NonERP_Present");
	if (data[0] & 0x02)
		printf(" Use_Protection");
	if (data[0] & 0x04)
		printf(" Barker_Preamble_Mode");
	printf("\n");
}

static void print_cipher(unsigned char *data)
{
	if (memcmp(data, wifi_oui, 3) == 0) {
		switch (data[3]) {
		case 0x00:
			printf("Use group cipher suite");
			break;
		case 0x01:
			printf("WEP-40");
			break;
		case 0x02:
			printf("TKIP");
			break;
		case 0x04:
			printf("CCMP");
			break;
		case 0x05:
			printf("WEP-104");
			break;
		default:
			printf("Unknown (%.02x-%.02x-%.02x:%d)",
				data[0], data[1] ,data[2], data[3]);
			break;
		}
	} else if (memcmp(data, ieee80211_oui, 3) == 0) {
		switch (data[3]) {
		case 0x00:
			printf("Use group cipher suite");
			break;
		case 0x01:
			printf("WEP-40");
			break;
		case 0x02:
			printf("TKIP");
			break;
		case 0x04:
			printf("CCMP");
			break;
		case 0x05:
			printf("WEP-104");
			break;
		case 0x06:
			printf("AES-128-CMAC");
			break;
		default:
			printf("Unknown (%.02x-%.02x-%.02x:%d)",
				data[0], data[1] ,data[2], data[3]);
			break;
		}
	} else
		printf("Unknown (%.02x-%.02x-%.02x:%d)",
			data[0], data[1] ,data[2], data[3]);
}

static void print_auth(unsigned char *data)
{
	if (memcmp(data, wifi_oui, 3) == 0) {
		switch (data[3]) {
		case 0x01:
			printf("IEEE 802.1X");
			break;
		case 0x02:
			printf("PSK");
			break;
		default:
			printf("Unknown (%.02x-%.02x-%.02x:%d)",
				data[0], data[1] ,data[2], data[3]);
			break;
		}
	} else if (memcmp(data, ieee80211_oui, 3) == 0) {
		switch (data[3]) {
		case 0x01:
			printf("IEEE 802.1X");
			break;
		case 0x02:
			printf("PSK");
			break;
		default:
			printf("Unknown (%.02x-%.02x-%.02x:%d)",
				data[0], data[1] ,data[2], data[3]);
			break;
		}
	} else
		printf("Unknown (%.02x-%.02x-%.02x:%d)",
			data[0], data[1] ,data[2], data[3]);
}

static void print_rsn_ie(const char *ie,
			 const char *defcipher, const char *defauth,
			 unsigned char len, unsigned char *data)
{
	bool first = true;
	__u16 version, count, capa;
	int i;

	printf("\t%s:", ie);

	if (len < 2) {
		printf(" <too short> data:");
		for(i = 0; i < len; i++)
			printf(" %.02x", data[i]);
		printf("\n");
		return;
	}

	version = data[0] + (data[1] << 8);
	tab_on_first(&first);
	printf("\t * Version: %d\n", version);

	data += 2;
	len -= 2;

	if (len < 4) {
		tab_on_first(&first);
		printf("\t * Group cipher: %s\n", defcipher);
		printf("\t * Pairwise ciphers: %s\n", defcipher);
		return;
	}

	tab_on_first(&first);
	printf("\t * Group cipher: ");
	print_cipher(data);
	printf("\n");

	data += 4;
	len -= 4;

	if (len < 2) {
		tab_on_first(&first);
		printf("\t * Pairwise ciphers: %s\n", defcipher);
		return;
	}

	count = data[0] | (data[1] << 8);
	tab_on_first(&first);
	printf("\t * Pairwise ciphers:");
	for (i=0; i<count; i++) {
		printf(" ");
		print_cipher(data + 2 + (i * 4));
	}
	printf("\n");

	data += 2 + (count * 4);
	len -= 2 + (count * 4);

	if (len < 2) {
		tab_on_first(&first);
		printf("\t * Authentication suites: %s\n", defauth);
		return;
	}

	count = data[0] | (data[1] << 8);
	tab_on_first(&first);
	printf("\t * Authentication suites:");
	for (i=0; i<count; i++) {
		printf(" ");
		print_auth(data + 2 + (i * 4));
	}
	printf("\n");

	data += 2 + (count * 4);
	len -= 2 + (count * 4);

	if (len < 2)
		return;

	capa = data[0] | (data[1] << 8);
	tab_on_first(&first);
	printf("\t * Capabilities: 0x%.4x\n", capa);
}

static void print_rsn(unsigned char type, unsigned char len, unsigned char *data)
{
	print_rsn_ie("RSN", "CCMP", "IEEE 802.1X", len, data);
}

static void print_capabilities(unsigned char type, unsigned char len, unsigned char *data)
{
	int i;

	printf("\tExtended capabilties:");
	for(i=0; i<len; i++)
		printf(" %.02x", data[i]);
	printf("\n");
}

static const printfn ieprinters[] = {
	[0] = print_ssid,
	[1] = print_supprates,
	[3] = print_ds,
	[5] = print_ign,
	[7] = print_country,
	[42] = print_erp,
	[48] = print_rsn,
	[50] = print_supprates,
	[127] = print_capabilities,
};

static void print_wifi_wpa(unsigned char type, unsigned char len, unsigned char *data)
{
	print_rsn_ie("WPA", "TKIP", "IEEE 802.1X", len, data);
}

static void print_wifi_wmm(unsigned char type, unsigned char len, unsigned char *data)
{
	int i;

	printf("\tWMM ");
	switch (data[0]) {
	case 0x00:
		printf("information:");
		break;
	case 0x01:
		printf("parameter:");
		break;
	default:
		printf("type %d:", data[0]);
		break;
	}

	for(i=0; i<len-1; i++)
		printf(" %.02x", data[i + 1]);
	printf("\n");
}

static void print_wifi_wps(unsigned char type, unsigned char len, unsigned char *data)
{
	bool first = true;
	__u16 subtype, sublen;

	printf("\tWPS:");

	while (len >= 4) {
		subtype = (data[0] << 8) + data[1];
		sublen = (data[2] << 8) + data[3];
		if (sublen > len)
			break;

		switch (subtype) {
		case 0x104a:
			tab_on_first(&first);
			printf("\t * Version: %d.%d\n", data[4] >> 4, data[4] & 0xF);
			break;
		case 0x1011:
			tab_on_first(&first);
			printf("\t * Device name: %.*s\n", sublen, data + 4);
			break;
		case 0x1021:
			tab_on_first(&first);
			printf("\t * Manufacturer: %.*s\n", sublen, data + 4);
			break;
		case 0x1023:
			tab_on_first(&first);
			printf("\t * Model: %.*s\n", sublen, data + 4);
			break;
		case 0x1057: {
			__u16 val = (data[4] << 8) | data[5];
			tab_on_first(&first);
			printf("\t * AP setup locked: 0x%.4x\n", val);
			break;
		}
		case 0x1008: {
			__u16 meth = (data[4] << 8) + data[5];
			bool comma = false;
			tab_on_first(&first);
			printf("\t * Config methods:");
#define T(bit, name) do {		\
	if (meth & (1<<bit)) {		\
		if (comma)		\
			printf(",");	\
		comma = true;		\
		printf(" " name);	\
	} } while (0)
			T(0, "USB");
			T(1, "Ethernet");
			T(2, "Label");
			T(3, "Display");
			T(4, "Ext. NFC");
			T(5, "Int. NFC");
			T(6, "NFC Intf.");
			T(7, "PBC");
			T(8, "Keypad");
			printf("\n");
			break;
#undef T
		}
		default:
			break;
		}

		data += sublen + 4;
		len -= sublen + 4;
	}

	if (len != 0) {
		printf("\t\t * bogus tail data (%d):", len);
		while (len) {
			printf(" %.2x", *data);
			data++;
			len--;
		}
		printf("\n");
	}
}

static const printfn wifiprinters[] = {
	[1] = print_wifi_wpa,
	[2] = print_wifi_wmm,
	[4] = print_wifi_wps,
};

static void print_vendor(unsigned char len, unsigned char *data,
			 struct scan_params *params)
{
	int i;

	if (len < 3) {
		printf("\tVendor specific: <too short> data:");
		for(i = 0; i < len; i++)
			printf(" %.02x", data[i]);
		printf("\n");
		return;
	}

	if (len >= 4 && memcmp(data, wifi_oui, 3) == 0) {
		if (data[3] < ARRAY_SIZE(wifiprinters) && wifiprinters[data[3]])
			return wifiprinters[data[3]](data[3], len - 4, data + 4);
		if (!params->unknown)
			return;
		printf("\tWiFi OUI %#.2x, data:", data[3]);
		for(i = 0; i < len - 4; i++)
			printf(" %.02x", data[i + 4]);
		printf("\n");
		return;
	}

	if (!params->unknown)
		return;

	printf("\tVendor specific: OUI %.2x:%.2x:%.2x, data:",
		data[0], data[1], data[2]);
	for (i = 3; i < len; i++)
		printf(" %.2x", data[i]);
	printf("\n");
}

static void print_ies(unsigned char *ie, int ielen, struct scan_params *params)
{
	while (ielen >= 2 && ielen >= ie[1]) {
		if (ie[0] < ARRAY_SIZE(ieprinters) && ieprinters[ie[0]]) {
			ieprinters[ie[0]](ie[0], ie[1], ie + 2);
		} else if (ie[0] == 221 /* vendor */) {
			print_vendor(ie[1], ie + 2, params);
		} else if (params->unknown) {
			int i;

			printf("\tUnknown IE (%d):", ie[0]);
			for (i=0; i<ie[1]; i++)
				printf(" %.2x", ie[2+i]);
			printf("\n");
		}
		ielen -= ie[1] + 2;
		ie += ie[1] + 2;
	}
}

static int print_bss_handler(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *bss[NL80211_BSS_MAX + 1];
	char mac_addr[20], dev[20];
	static struct nla_policy bss_policy[NL80211_BSS_MAX + 1] = {
		[NL80211_BSS_TSF] = { .type = NLA_U64 },
		[NL80211_BSS_FREQUENCY] = { .type = NLA_U32 },
		[NL80211_BSS_BSSID] = { },
		[NL80211_BSS_BEACON_INTERVAL] = { .type = NLA_U16 },
		[NL80211_BSS_CAPABILITY] = { .type = NLA_U16 },
		[NL80211_BSS_INFORMATION_ELEMENTS] = { },
		[NL80211_BSS_SIGNAL_MBM] = { .type = NLA_U32 },
		[NL80211_BSS_SIGNAL_UNSPEC] = { .type = NLA_U8 },
	};

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (!tb[NL80211_ATTR_BSS]) {
		fprintf(stderr, "bss info missing!");
		return NL_SKIP;
	}
	if (nla_parse_nested(bss, NL80211_BSS_MAX,
			     tb[NL80211_ATTR_BSS],
			     bss_policy)) {
		fprintf(stderr, "failed to parse nested attributes!");
		return NL_SKIP;
	}

	if (!bss[NL80211_BSS_BSSID])
		return NL_SKIP;

	mac_addr_n2a(mac_addr, nla_data(bss[NL80211_BSS_BSSID]));
	if_indextoname(nla_get_u32(tb[NL80211_ATTR_IFINDEX]), dev);
	printf("BSS %s (on %s)\n", mac_addr, dev);

	if (bss[NL80211_BSS_TSF]) {
		unsigned long long tsf;
		tsf = (unsigned long long)nla_get_u64(bss[NL80211_BSS_TSF]);
		printf("\tTSF: %llu usec (%llud, %.2lld:%.2llu:%.2llu)\n",
			tsf, tsf/1000/1000/60/60/24, (tsf/1000/1000/60/60) % 24,
			(tsf/1000/1000/60) % 60, (tsf/1000/1000) % 60);
	}
	if (bss[NL80211_BSS_FREQUENCY])
		printf("\tfreq: %d\n",
			nla_get_u32(bss[NL80211_BSS_FREQUENCY]));
	if (bss[NL80211_BSS_BEACON_INTERVAL])
		printf("\tbeacon interval: %d\n",
			nla_get_u16(bss[NL80211_BSS_BEACON_INTERVAL]));
	if (bss[NL80211_BSS_CAPABILITY]) {
		__u16 capa = nla_get_u16(bss[NL80211_BSS_CAPABILITY]);
		printf("\tcapability:");
		if (capa & WLAN_CAPABILITY_ESS)
			printf(" ESS");
		if (capa & WLAN_CAPABILITY_IBSS)
			printf(" IBSS");
		if (capa & WLAN_CAPABILITY_PRIVACY)
			printf(" Privacy");
		if (capa & WLAN_CAPABILITY_SHORT_PREAMBLE)
			printf(" ShortPreamble");
		if (capa & WLAN_CAPABILITY_PBCC)
			printf(" PBCC");
		if (capa & WLAN_CAPABILITY_CHANNEL_AGILITY)
			printf(" ChannelAgility");
		if (capa & WLAN_CAPABILITY_SPECTRUM_MGMT)
			printf(" SpectrumMgmt");
		if (capa & WLAN_CAPABILITY_QOS)
			printf(" QoS");
		if (capa & WLAN_CAPABILITY_SHORT_SLOT_TIME)
			printf(" ShortSlotTime");
		if (capa & WLAN_CAPABILITY_APSD)
			printf(" APSD");
		if (capa & WLAN_CAPABILITY_DSSS_OFDM)
			printf(" DSSS-OFDM");
		printf(" (0x%.4x)\n", capa);
	}
	if (bss[NL80211_BSS_SIGNAL_MBM]) {
		int s = nla_get_u32(bss[NL80211_BSS_SIGNAL_MBM]);
		printf("\tsignal: %d.%.2d dBm\n", s/100, s%100);
	}
	if (bss[NL80211_BSS_SIGNAL_UNSPEC]) {
		unsigned char s = nla_get_u8(bss[NL80211_BSS_SIGNAL_UNSPEC]);
		printf("\tsignal: %d/100\n", s);
	}
	if (bss[NL80211_BSS_INFORMATION_ELEMENTS])
		print_ies(nla_data(bss[NL80211_BSS_INFORMATION_ELEMENTS]),
			  nla_len(bss[NL80211_BSS_INFORMATION_ELEMENTS]),
			  arg);

	return NL_SKIP;
}

static struct scan_params scan_params;

static int handle_scan_dump(struct nl80211_state *state,
			    struct nl_cb *cb,
			    struct nl_msg *msg,
			    int argc, char **argv)
{
	if (argc > 1)
		return 1;

	scan_params.unknown = false;
	if (argc == 1 && !strcmp(argv[0], "-u"))
		scan_params.unknown = true;

	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, print_bss_handler,
		  &scan_params);
	return 0;
}
COMMAND(scan, dump, "[-u]",
	NL80211_CMD_GET_SCAN, NLM_F_DUMP, CIB_NETDEV, handle_scan_dump);

static int handle_scan_combined(struct nl80211_state *state,
				struct nl_cb *cb,
				struct nl_msg *msg,
				int argc, char **argv)
{
	static char *trig_argv[] = {
		NULL,
		"scan",
		"trigger",
	};
	static char *dump_argv[] = {
		NULL,
		"scan",
		"dump",
		NULL,
	};
	static const __u32 cmds[] = {
		NL80211_CMD_NEW_SCAN_RESULTS,
		NL80211_CMD_SCAN_ABORTED,
	};
	int dump_argc, err;

	trig_argv[0] = argv[0];
	err = handle_cmd(state, II_NETDEV, ARRAY_SIZE(trig_argv), trig_argv);
	if (err)
		return err;

	/*
	 * WARNING: DO NOT COPY THIS CODE INTO YOUR APPLICATION
	 *
	 * This code has a bug, which requires creating a separate
	 * nl80211 socket to fix:
	 * It is possible for a NL80211_CMD_NEW_SCAN_RESULTS or
	 * NL80211_CMD_SCAN_ABORTED message to be sent by the kernel
	 * before (!) we listen to it, because we only start listening
	 * after we send our scan request.
	 *
	 * Doing it the other way around has a race condition as well,
	 * if you first open the events socket you may get a notification
	 * for a previous scan.
	 *
	 * The only proper way to fix this would be to listen to events
	 * before sending the command, and for the kernel to send the
	 * scan request along with the event, so that you can match up
	 * whether the scan you requested was finished or aborted (this
	 * may result in processing a scan that another application
	 * requested, but that doesn't seem to be a problem).
	 *
	 * Alas, the kernel doesn't do that (yet).
	 */

	if (listen_events(state, ARRAY_SIZE(cmds), cmds) ==
					NL80211_CMD_SCAN_ABORTED) {
		printf("scan aborted!\n");
		return 0;
	}

	if (argc == 3 && !strcmp(argv[2], "-u")) {
		dump_argc = 4;
		dump_argv[3] = "-u";
	} else
		dump_argc = 3;

	dump_argv[0] = argv[0];
	return handle_cmd(state, II_NETDEV, dump_argc, dump_argv);
}
TOPLEVEL(scan, "[-u]", 0, 0, CIB_NETDEV, handle_scan_combined);
