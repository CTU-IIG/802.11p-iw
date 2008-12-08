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

static int handle_name(struct nl_cb *cb,
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
COMMAND(set, name, "<new name>", NL80211_CMD_SET_WIPHY, 0, CIB_PHY, handle_name);

static int handle_freqchan(struct nl_msg *msg, bool chan,
			   int argc, char **argv)
{
	static const struct {
		const char *name;
		unsigned int val;
	} htmap[] = {
		{ .name = "HT20", .val = NL80211_SEC_CHAN_DISABLED, },
		{ .name = "HT40+", .val = NL80211_SEC_CHAN_ABOVE, },
		{ .name = "HT40-", .val = NL80211_SEC_CHAN_BELOW, },
	};
	unsigned int htval = NL80211_SEC_CHAN_NO_HT;
	unsigned int freq;
	int i;

	if (!argc || argc > 2)
		return 1;

	if (argc == 2) {
		for (i = 0; i < sizeof(htmap)/sizeof(htmap[0]); i++) {
			if (strcasecmp(htmap[i].name, argv[1]) == 0) {
				htval = htmap[i].val;
				break;
			}
		}
		if (htval == NL80211_SEC_CHAN_NO_HT)
			return 1;
	}

	freq = strtoul(argv[0], NULL, 10);
	if (chan)
		freq = ieee80211_channel_to_frequency(freq);

	NLA_PUT_U32(msg, NL80211_ATTR_WIPHY_FREQ, freq);
	NLA_PUT_U32(msg, NL80211_ATTR_WIPHY_SEC_CHAN_OFFSET, htval);

	return 0;
 nla_put_failure:
	return -ENOBUFS;
}

static int handle_freq(struct nl_cb *cb, struct nl_msg *msg,
		       int argc, char **argv)
{
	return handle_freqchan(msg, false, argc, argv);
}
COMMAND(set, freq, "<freq> [HT20|HT40+|HT40-]",
	NL80211_CMD_SET_WIPHY, 0, CIB_PHY, handle_freq);
COMMAND(set, freq, "<freq> [HT20|HT40+|HT40-]",
	NL80211_CMD_SET_WIPHY, 0, CIB_NETDEV, handle_freq);

static int handle_chan(struct nl_cb *cb, struct nl_msg *msg,
		       int argc, char **argv)
{
	return handle_freqchan(msg, true, argc, argv);
}
COMMAND(set, channel, "<channel> [HT20|HT40+|HT40-]",
	NL80211_CMD_SET_WIPHY, 0, CIB_PHY, handle_chan);
COMMAND(set, channel, "<channel> [HT20|HT40+|HT40-]",
	NL80211_CMD_SET_WIPHY, 0, CIB_NETDEV, handle_chan);
