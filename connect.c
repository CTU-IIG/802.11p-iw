#include <errno.h>

#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <netlink/msg.h>
#include <netlink/attr.h>

#include "nl80211.h"
#include "iw.h"

static int iw_connect(struct nl80211_state *state,
		      struct nl_cb *cb,
		      struct nl_msg *msg,
		      int argc, char **argv)
{
	char *end;
	unsigned char abssid[6];
	int freq;

	if (argc < 1)
		return 1;

	/* SSID */
	NLA_PUT(msg, NL80211_ATTR_SSID, strlen(argv[0]), argv[0]);
	argv++;
	argc--;

	/* freq */
	if (argc) {
		freq = strtoul(argv[0], &end, 10);
		if (*end == '\0') {
			NLA_PUT_U32(msg, NL80211_ATTR_WIPHY_FREQ, freq);
			argv++;
			argc--;
		}
	}

	/* bssid */
	if (argc) {
		if (mac_addr_a2n(abssid, argv[0]))
			return 1;
		NLA_PUT(msg, NL80211_ATTR_MAC, 6, abssid);
		argv++;
		argc--;
	}

	if (argc)
		return 1;

	return 0;
 nla_put_failure:
	return -ENOSPC;
}

static int disconnect(struct nl80211_state *state,
		      struct nl_cb *cb,
		      struct nl_msg *msg,
		      int argc, char **argv)
{
	return 0;
}
TOPLEVEL(disconnect, NULL,
	NL80211_CMD_DISCONNECT, 0, CIB_NETDEV, disconnect,
	"Disconnect from the current network.");
TOPLEVEL(connect, "<SSID> <freq in MHz> [<bssid>]",
	NL80211_CMD_CONNECT, 0, CIB_NETDEV, iw_connect,
	"Join the network with the given SSID (and frequency, BSSID).");
