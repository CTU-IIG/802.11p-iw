#include <linux/nl80211.h>
#include <net/if.h>
#include <errno.h>
#include <string.h>

#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <netlink/msg.h>
#include <netlink/attr.h>

#include "iw.h"

enum plink_state {
	LISTEN,
	OPN_SNT,
	OPN_RCVD,
	CNF_RCVD,
	ESTAB,
	HOLDING,
	BLOCKED
};

enum plink_actions {
	PLINK_ACTION_UNDEFINED,
	PLINK_ACTION_OPEN,
	PLINK_ACTION_BLOCK,
};


static int wait_handler(struct nl_msg *msg, void *arg)
{
	int *finished = arg;

	*finished = 1;
	return NL_STOP;
}

static int error_handler(struct sockaddr_nl *nla, struct nlmsgerr *err,
			 void *arg)
{
	fprintf(stderr, "nl80211 error %d\n", err->error);
	exit(err->error);
}

static int print_mpath_handler(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *pinfo[NL80211_MPATH_INFO_MAX + 1];
	char dst[20], next_hop[20], dev[20];
	static struct nla_policy mpath_policy[NL80211_MPATH_INFO_MAX + 1] = {
		[NL80211_MPATH_INFO_FRAME_QLEN] = { .type = NLA_U32 },
		[NL80211_MPATH_INFO_DSN] = { .type = NLA_U32 },
		[NL80211_MPATH_INFO_METRIC] = { .type = NLA_U32 },
		[NL80211_MPATH_INFO_EXPTIME] = { .type = NLA_U32 },
		[NL80211_MPATH_INFO_DISCOVERY_TIMEOUT] = { .type = NLA_U32 },
		[NL80211_MPATH_INFO_DISCOVERY_RETRIES] = { .type = NLA_U8 },
		[NL80211_MPATH_INFO_FLAGS] = { .type = NLA_U8 },
	};

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	/*
	 * TODO: validate the interface and mac address!
	 * Otherwise, there's a race condition as soon as
	 * the kernel starts sending mpath notifications.
	 */

	if (!tb[NL80211_ATTR_MPATH_INFO]) {
		fprintf(stderr, "mpath info missing!");
		return NL_SKIP;
	}
	if (nla_parse_nested(pinfo, NL80211_MPATH_INFO_MAX,
			     tb[NL80211_ATTR_MPATH_INFO],
			     mpath_policy)) {
		fprintf(stderr, "failed to parse nested attributes!");
		return NL_SKIP;
	}

	mac_addr_n2a(dst, nla_data(tb[NL80211_ATTR_MAC]));
	mac_addr_n2a(next_hop, nla_data(tb[NL80211_ATTR_MPATH_NEXT_HOP]));
	if_indextoname(nla_get_u32(tb[NL80211_ATTR_IFINDEX]), dev);
	printf("%s %s %s", dst, next_hop, dev);
	if (pinfo[NL80211_MPATH_INFO_DSN])
		printf("\t%u",
			nla_get_u32(pinfo[NL80211_MPATH_INFO_DSN]));
	if (pinfo[NL80211_MPATH_INFO_METRIC])
		printf("\t%u",
			nla_get_u32(pinfo[NL80211_MPATH_INFO_METRIC]));
	if (pinfo[NL80211_MPATH_INFO_FRAME_QLEN])
		printf("\t%u",
			nla_get_u32(pinfo[NL80211_MPATH_INFO_FRAME_QLEN]));
	if (pinfo[NL80211_MPATH_INFO_EXPTIME])
		printf("\t%u",
			nla_get_u32(pinfo[NL80211_MPATH_INFO_EXPTIME]));
	if (pinfo[NL80211_MPATH_INFO_DISCOVERY_TIMEOUT])
		printf("\t%u",
		nla_get_u32(pinfo[NL80211_MPATH_INFO_DISCOVERY_TIMEOUT]));
	if (pinfo[NL80211_MPATH_INFO_DISCOVERY_RETRIES])
		printf("\t%u",
		nla_get_u8(pinfo[NL80211_MPATH_INFO_DISCOVERY_RETRIES]));
	if (pinfo[NL80211_MPATH_INFO_FLAGS])
		printf("\t0x%x",
			nla_get_u8(pinfo[NL80211_MPATH_INFO_FLAGS]));

	printf("\n");
	return NL_SKIP;
}

static int handle_mpath_get(struct nl80211_state *state,
			    struct nl_msg *msg,
			    int argc, char **argv)
{
	struct nl_cb *cb = NULL;
	int ret = -1;
	int err;
	int finished = 0;
	unsigned char dst[ETH_ALEN];

	if (argc < 1)
		return -1;

	if (mac_addr_a2n(dst, argv[0])) {
		fprintf(stderr, "invalid mac address\n");
		return 1;
	}
	argc--;
	argv++;

	if (argc)
		return -1;

	NLA_PUT(msg, NL80211_ATTR_MAC, ETH_ALEN, dst);

	cb = nl_cb_alloc(NL_CB_CUSTOM);
	if (!cb)
		goto out;

	if (nl_send_auto_complete(state->nl_handle, msg) < 0)
		goto out;

	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, print_mpath_handler, NULL);
	nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, wait_handler, &finished);
	nl_cb_err(cb, NL_CB_CUSTOM, error_handler, NULL);

	err = nl_recvmsgs(state->nl_handle, cb);

	if (!finished)
		err = nl_wait_for_ack(state->nl_handle);

	if (err < 0)
		goto out;

	ret = 0;

 out:
	nl_cb_put(cb);
 nla_put_failure:
	return ret;
}
COMMAND(mpath, get, "<MAC address>",
	NL80211_CMD_GET_MPATH, 0, CIB_NETDEV, handle_mpath_get);
COMMAND(mpath, del, "<MAC address>",
	NL80211_CMD_DEL_MPATH, 0, CIB_NETDEV, handle_mpath_get);

static int handle_mpath_set(struct nl80211_state *state,
			    struct nl_msg *msg,
			    int argc, char **argv)
{
	struct nl_cb *cb = NULL;
	int ret = -1;
	int err;
	int finished = 0;
	unsigned char dst[ETH_ALEN];
	unsigned char next_hop[ETH_ALEN];

	if (argc < 3)
		return -1;

	if (mac_addr_a2n(dst, argv[0])) {
		fprintf(stderr, "invalid destination mac address\n");
		return 1;
	}
	argc--;
	argv++;

	if (strcmp("next_hop", argv[0]) != 0)
		return -1;
	argc--;
	argv++;

	if (mac_addr_a2n(next_hop, argv[0])) {
		fprintf(stderr, "invalid next hop mac address\n");
		return 1;
	}
	argc--;
	argv++;

	if (argc)
		return -1;

	msg = nlmsg_alloc();
	if (!msg)
		goto out;

	NLA_PUT(msg, NL80211_ATTR_MAC, ETH_ALEN, dst);
	NLA_PUT(msg, NL80211_ATTR_MPATH_NEXT_HOP, ETH_ALEN, next_hop);

	cb = nl_cb_alloc(NL_CB_CUSTOM);
	if (!cb)
		goto out;

	if (nl_send_auto_complete(state->nl_handle, msg) < 0)
		goto out;

	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, print_mpath_handler, NULL);
	nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, wait_handler, &finished);
	nl_cb_err(cb, NL_CB_CUSTOM, error_handler, NULL);

	err = nl_recvmsgs(state->nl_handle, cb);

	if (!finished)
		err = nl_wait_for_ack(state->nl_handle);

	if (err < 0)
		goto out;

	ret = 0;

 out:
	nl_cb_put(cb);
 nla_put_failure:
	return ret;
}
COMMAND(mpath, new, "<destination MAC address> next_hop <next hop MAC address>",
	NL80211_CMD_NEW_MPATH, 0, CIB_NETDEV, handle_mpath_set);
COMMAND(mpath, set, "<destination MAC address> next_hop <next hop MAC address>",
	NL80211_CMD_SET_MPATH, 0, CIB_NETDEV, handle_mpath_set);

static int handle_mpath_dump(struct nl80211_state *state,
			     struct nl_msg *msg,
			     int argc, char **argv)
{
	struct nl_cb *cb = NULL;
	int ret = 1;
	int err;
	int finished = 0;

	cb = nl_cb_alloc(NL_CB_CUSTOM);
	if (!cb)
		goto out;

	if (nl_send_auto_complete(state->nl_handle, msg) < 0)
		goto out;

	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, print_mpath_handler, NULL);
	nl_cb_set(cb, NL_CB_FINISH, NL_CB_CUSTOM, wait_handler, &finished);

	err = nl_recvmsgs(state->nl_handle, cb);

	if (err < 0)
		goto out;

	ret = 0;

 out:
	nl_cb_put(cb);
	return ret;
}
COMMAND(mpath, dump, NULL,
	NL80211_CMD_GET_MPATH, NLM_F_DUMP, CIB_NETDEV, handle_mpath_dump);
