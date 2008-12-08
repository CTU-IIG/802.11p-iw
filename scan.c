#include <net/if.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <netlink/msg.h>
#include <netlink/attr.h>

#include "nl80211.h"
#include "iw.h"

static int handle_scan(struct nl_cb *cb,
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

static void print_vendor(unsigned char type, unsigned char len, unsigned char *data)
{
	int i;

	printf("\tVendor specific: OUI %.2x:%.2x:%.2x, data: ",
		data[0], data[1], data[2]);
	for (i=3; i<len; i++)
		printf("\\x%.2x", data[i]);
	printf("\n");
}

static const printfn ieprinters[] = {
	[0] = print_ssid,
	[1] = print_supprates,
	[3] = print_ds,
	[5] = print_ign,
	[50] = print_supprates,
	[221] = print_vendor,
};

static void print_ies(unsigned char *ie, int ielen)
{
	while (ielen >= 2 && ielen >= ie[1]) {
		if (ie[0] < ARRAY_SIZE(ieprinters) && ieprinters[ie[0]]) {
			ieprinters[ie[0]](ie[0], ie[1], ie + 2);
		} else {
			int i;

			printf("\tUnknown IE (%d): ", ie[0]);
			for (i=0; i<ie[1]; i++)
				printf("\\x%.2x", ie[2+i]);
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

	if (bss[NL80211_BSS_TSF])
		printf("\tTSF: %llu usec\n",
			(unsigned long long)nla_get_u64(bss[NL80211_BSS_TSF]));
	if (bss[NL80211_BSS_FREQUENCY])
		printf("\tfreq: %d\n",
			nla_get_u32(bss[NL80211_BSS_FREQUENCY]));
	if (bss[NL80211_BSS_BEACON_INTERVAL])
		printf("\tbeacon interval: %d\n",
			nla_get_u16(bss[NL80211_BSS_BEACON_INTERVAL]));
	if (bss[NL80211_BSS_CAPABILITY])
		printf("\tcapability: 0x%.4x\n",
			nla_get_u16(bss[NL80211_BSS_CAPABILITY]));
	if (bss[NL80211_BSS_INFORMATION_ELEMENTS])
		print_ies(nla_data(bss[NL80211_BSS_INFORMATION_ELEMENTS]),
			  nla_len(bss[NL80211_BSS_INFORMATION_ELEMENTS]));

	return NL_SKIP;
}



static int handle_scan_dump(struct nl_cb *cb,
			    struct nl_msg *msg,
			    int argc, char **argv)
{
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, print_bss_handler, NULL);
	return 0;
}
COMMAND(scan, dump, NULL,
	NL80211_CMD_GET_SCAN, NLM_F_DUMP, CIB_NETDEV, handle_scan_dump);
