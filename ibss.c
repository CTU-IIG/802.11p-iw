#include <errno.h>

#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <netlink/msg.h>
#include <netlink/attr.h>

#include "nl80211.h"
#include "iw.h"

SECTION(ibss);

static int join_ibss(struct nl80211_state *state,
		     struct nl_cb *cb,
		     struct nl_msg *msg,
		     int argc, char **argv)
{
	char *end;
	unsigned char abssid[6];

	if (argc < 2)
		return 1;

	/* SSID */
	NLA_PUT(msg, NL80211_ATTR_SSID, strlen(argv[0]), argv[0]);
	argv++;
	argc--;

	/* freq */
	NLA_PUT_U32(msg, NL80211_ATTR_WIPHY_FREQ,
		    strtoul(argv[0], &end, 10));
	if (*end != '\0')
		return 1;
	argv++;
	argc--;

	if (argc && strcmp(argv[0], "fixed-freq") == 0) {
		NLA_PUT_FLAG(msg, NL80211_ATTR_FREQ_FIXED);
		argv++;
		argc--;
	}

	if (argc) {
		if (mac_addr_a2n(abssid, argv[0]) == 0) {
			NLA_PUT(msg, NL80211_ATTR_MAC, 6, abssid);
			argv++;
			argc--;
		}
	}

	if (!argc)
		return 0;

	if (strcmp(*argv, "key") != 0 && strcmp(*argv, "keys") != 0)
		return 1;

	argv++;
	argc--;

	return parse_keys(msg, argv, argc);
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
	NL80211_CMD_LEAVE_IBSS, 0, CIB_NETDEV, leave_ibss,
	"Leave the current IBSS cell.");
COMMAND(ibss, join, "<SSID> <freq in MHz> [fixed-freq] [<fixed bssid>] [key d:0:abcde]",
	NL80211_CMD_JOIN_IBSS, 0, CIB_NETDEV, join_ibss,
	"Join the IBSS cell with the given SSID, if it doesn't exist create\n"
	"it on the given frequency. When fixed frequency is requested, don't\n"
	"join/create a cell on a different frequency. When a fixed BSSID is\n"
	"requested use that BSSID and do not adopt another cell's BSSID even\n"
	"if it has higher TSF and the same SSID.");
