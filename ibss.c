#include <errno.h>

#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <netlink/msg.h>
#include <netlink/attr.h>

#include "nl80211.h"
#include "iw.h"

static int join_ibss(struct nl80211_state *state,
		     struct nl_cb *cb,
		     struct nl_msg *msg,
		     int argc, char **argv)
{
	int oargc;
	char *bssid = NULL, *freq = NULL;
	unsigned char abssid[6];

	NLA_PUT(msg, NL80211_ATTR_SSID, strlen(argv[0]), argv[0]);

	argv++;
	argc--;

	oargc = argc;

	while (argc) {
		if (argc > 0 && strcmp(argv[0], "bssid") == 0) {
			bssid = argv[1];
			argv += 2;
			argc -= 2;
		}

		if (argc > 0 && strcmp(argv[0], "freq") == 0) {
			freq = argv[1];
			argv += 2;
			argc -= 2;
		}
		if (oargc == argc)
			return 1;
		oargc = argc;
	}

	if (bssid) {
		if (mac_addr_a2n(abssid, bssid))
			return 1;
		NLA_PUT(msg, NL80211_ATTR_MAC, 6, abssid);
	}

	if (freq) {
		char *end;
		NLA_PUT_U32(msg, NL80211_ATTR_WIPHY_FREQ,
			    strtoul(freq, &end, 10));
		if (*end != '\0')
			return 1;
	}

	return 0;
 nla_put_failure:
	return -ENOSPC;
}

static int leave_ibss(struct nl80211_state *state,
		      struct nl_cb *cb,
		      struct nl_msg *msg,
		      int argc, char **argv)
{
	return 0;
}
COMMAND(ibss, leave, NULL,
	NL80211_CMD_LEAVE_IBSS, 0, CIB_NETDEV, leave_ibss);
COMMAND(ibss, join, "<SSID> [bssid <bssid>] [freq <freq in MHz>]",
	NL80211_CMD_JOIN_IBSS, 0, CIB_NETDEV, join_ibss);
