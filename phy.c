#include <stdbool.h>
#include <errno.h>
#include <net/if.h>

#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <netlink/msg.h>
#include <netlink/attr.h>

#include "nl80211.h"
#include "iw.h"

static int handle_name(struct nl80211_state *state,
		       struct nl_cb *cb,
		       struct nl_msg *msg,
		       int argc, char **argv)
{
	if (argc != 1)
		return 1;

	NLA_PUT_STRING(msg, NL80211_ATTR_WIPHY_NAME, *argv);

	return 0;
 nla_put_failure:
	return -ENOBUFS;
}
COMMAND(set, name, "<new name>", NL80211_CMD_SET_WIPHY, 0, CIB_PHY, handle_name,
	"Rename this wireless device.");

static int handle_freqchan(struct nl_msg *msg, bool chan,
			   int argc, char **argv)
{
	static const struct {
		const char *name;
		unsigned int val;
	} htmap[] = {
		{ .name = "HT20", .val = NL80211_CHAN_HT20, },
		{ .name = "HT40+", .val = NL80211_CHAN_HT40PLUS, },
		{ .name = "HT40-", .val = NL80211_CHAN_HT40MINUS, },
	};
	unsigned int htval = NL80211_CHAN_NO_HT;
	unsigned int freq;
	int i;

	if (!argc || argc > 2)
		return 1;

	if (argc == 2) {
		for (i = 0; i < ARRAY_SIZE(htmap); i++) {
			if (strcasecmp(htmap[i].name, argv[1]) == 0) {
				htval = htmap[i].val;
				break;
			}
		}
		if (htval == NL80211_CHAN_NO_HT)
			return 1;
	}

	freq = strtoul(argv[0], NULL, 10);
	if (chan)
		freq = ieee80211_channel_to_frequency(freq);

	NLA_PUT_U32(msg, NL80211_ATTR_WIPHY_FREQ, freq);
	NLA_PUT_U32(msg, NL80211_ATTR_WIPHY_CHANNEL_TYPE, htval);

	return 0;
 nla_put_failure:
	return -ENOBUFS;
}

static int handle_freq(struct nl80211_state *state,
		       struct nl_cb *cb, struct nl_msg *msg,
		       int argc, char **argv)
{
	return handle_freqchan(msg, false, argc, argv);
}
COMMAND(set, freq, "<freq> [HT20|HT40+|HT40-]",
	NL80211_CMD_SET_WIPHY, 0, CIB_PHY, handle_freq,
	"Set frequency/channel the hardware is using, including HT\n"
	"configuration.");
COMMAND(set, freq, "<freq> [HT20|HT40+|HT40-]",
	NL80211_CMD_SET_WIPHY, 0, CIB_NETDEV, handle_freq, NULL);

static int handle_chan(struct nl80211_state *state,
		       struct nl_cb *cb, struct nl_msg *msg,
		       int argc, char **argv)
{
	return handle_freqchan(msg, true, argc, argv);
}
COMMAND(set, channel, "<channel> [HT20|HT40+|HT40-]",
	NL80211_CMD_SET_WIPHY, 0, CIB_PHY, handle_chan, NULL);
COMMAND(set, channel, "<channel> [HT20|HT40+|HT40-]",
	NL80211_CMD_SET_WIPHY, 0, CIB_NETDEV, handle_chan, NULL);

static int handle_fragmentation(struct nl80211_state *state,
				struct nl_cb *cb, struct nl_msg *msg,
				int argc, char **argv)
{
	unsigned int frag;

	if (argc != 1)
		return 1;

	if (strcmp("off", argv[0]) == 0)
		frag = -1;
	else
		frag = strtoul(argv[0], NULL, 10);

	NLA_PUT_U32(msg, NL80211_ATTR_WIPHY_FRAG_THRESHOLD, frag);

	return 0;
 nla_put_failure:
	return -ENOBUFS;
}
COMMAND(set, frag, "<fragmentation threshold|off>",
	NL80211_CMD_SET_WIPHY, 0, CIB_PHY, handle_fragmentation,
	"Set fragmentation threshold.");

static int handle_rts(struct nl80211_state *state,
		      struct nl_cb *cb, struct nl_msg *msg,
		      int argc, char **argv)
{
	unsigned int rts;

	if (argc != 1)
		return 1;

	if (strcmp("off", argv[0]) == 0)
		rts = -1;
	else
		rts = strtoul(argv[0], NULL, 10);

	NLA_PUT_U32(msg, NL80211_ATTR_WIPHY_RTS_THRESHOLD, rts);

	return 0;
 nla_put_failure:
	return -ENOBUFS;
}
COMMAND(set, rts, "<rts threshold|off>",
	NL80211_CMD_SET_WIPHY, 0, CIB_PHY, handle_rts,
	"Set rts threshold.");

static int handle_netns(struct nl80211_state *state,
			struct nl_cb *cb,
			struct nl_msg *msg,
			int argc, char **argv)
{
	char *end;

	if (argc != 1)
		return 1;

	NLA_PUT_U32(msg, NL80211_ATTR_PID,
		    strtoul(argv[0], &end, 10)); 

	if (*end != '\0')
		return 1;

	return 0;
 nla_put_failure:
	return -ENOBUFS;
}
COMMAND(set, netns, "<pid>",
	NL80211_CMD_SET_WIPHY_NETNS, 0, CIB_PHY, handle_netns,
	"Put this wireless device into a different network namespace");
