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

static int print_sta_handler(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *sinfo[NL80211_STA_INFO_MAX + 1];
	char mac_addr[20], state_name[10], dev[20];
	static struct nla_policy stats_policy[NL80211_STA_INFO_MAX + 1] = {
		[NL80211_STA_INFO_INACTIVE_TIME] = { .type = NLA_U32 },
		[NL80211_STA_INFO_RX_BYTES] = { .type = NLA_U32 },
		[NL80211_STA_INFO_TX_BYTES] = { .type = NLA_U32 },
		[NL80211_STA_INFO_LLID] = { .type = NLA_U16 },
		[NL80211_STA_INFO_PLID] = { .type = NLA_U16 },
		[NL80211_STA_INFO_PLINK_STATE] = { .type = NLA_U8 },
	};

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	/*
	 * TODO: validate the interface and mac address!
	 * Otherwise, there's a race condition as soon as
	 * the kernel starts sending station notifications.
	 */

	if (!tb[NL80211_ATTR_STA_INFO]) {
		fprintf(stderr, "sta stats missing!");
		return NL_SKIP;
	}
	if (nla_parse_nested(sinfo, NL80211_STA_INFO_MAX,
			     tb[NL80211_ATTR_STA_INFO],
			     stats_policy)) {
		fprintf(stderr, "failed to parse nested attributes!");
		return NL_SKIP;
	}

	mac_addr_n2a(mac_addr, nla_data(tb[NL80211_ATTR_MAC]));
	if_indextoname(nla_get_u32(tb[NL80211_ATTR_IFINDEX]), dev);
	printf("Station %s (on %s)", mac_addr, dev);

	if (sinfo[NL80211_STA_INFO_INACTIVE_TIME])
		printf("\n\tinactive time:\t%d ms",
			nla_get_u32(sinfo[NL80211_STA_INFO_INACTIVE_TIME]));
	if (sinfo[NL80211_STA_INFO_RX_BYTES])
		printf("\n\trx bytes:\t%d",
			nla_get_u32(sinfo[NL80211_STA_INFO_RX_BYTES]));
	if (sinfo[NL80211_STA_INFO_TX_BYTES])
		printf("\n\ttx bytes:\t%d",
			nla_get_u32(sinfo[NL80211_STA_INFO_TX_BYTES]));
	if (sinfo[NL80211_STA_INFO_LLID])
		printf("\n\tmesh llid:\t%d",
			nla_get_u16(sinfo[NL80211_STA_INFO_LLID]));
	if (sinfo[NL80211_STA_INFO_PLID])
		printf("\n\tmesh plid:\t%d",
			nla_get_u16(sinfo[NL80211_STA_INFO_PLID]));
	if (sinfo[NL80211_STA_INFO_PLINK_STATE]) {
		switch (nla_get_u16(sinfo[NL80211_STA_INFO_PLINK_STATE])) {
		case LISTEN:
			strcpy(state_name, "LISTEN");
			break;
		case OPN_SNT:
			strcpy(state_name, "OPN_SNT");
			break;
		case OPN_RCVD:
			strcpy(state_name, "OPN_RCVD");
			break;
		case CNF_RCVD:
			strcpy(state_name, "CNF_RCVD");
			break;
		case ESTAB:
			strcpy(state_name, "ESTAB");
			break;
		case HOLDING:
			strcpy(state_name, "HOLDING");
			break;
		case BLOCKED:
			strcpy(state_name, "BLOCKED");
			break;
		default:
			strcpy(state_name, "UNKNOWN");
			break;
		}
		printf("\n\tmesh plink:\t%s", state_name);
	}

	printf("\n");
	return NL_SKIP;
}

static int handle_station_get(struct nl80211_state *state,
			      struct nl_msg *msg,
			      int argc, char **argv)
{
	struct nl_cb *cb = NULL;
	int ret = 1;
	int err;
	int finished = 0;
	unsigned char mac_addr[ETH_ALEN];

	if (argc < 1)
		return -1;

	if (mac_addr_a2n(mac_addr, argv[0])) {
		fprintf(stderr, "invalid mac address\n");
		return 1;
	}

	argc--;
	argv++;

	if (argc)
		return -1;

	NLA_PUT(msg, NL80211_ATTR_MAC, ETH_ALEN, mac_addr);

	cb = nl_cb_alloc(NL_CB_CUSTOM);
	if (!cb)
		goto out;

	if (nl_send_auto_complete(state->nl_handle, msg) < 0)
		goto out;

	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, print_sta_handler, NULL);
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
COMMAND(station, get, "<MAC address>",
	NL80211_CMD_GET_STATION, 0, CIB_NETDEV, handle_station_get);
COMMAND(station, del, "<MAC address>",
	NL80211_CMD_DEL_STATION, 0, CIB_NETDEV, handle_station_get);

static int handle_station_set(struct nl80211_state *state,
			      struct nl_msg *msg,
			      int argc, char **argv)
{
	struct nl_cb *cb = NULL;
	int ret = 1;
	int err;
	int finished = 0;
	unsigned char plink_action;
	unsigned char mac_addr[ETH_ALEN];

	if (argc < 3)
		return -1;

	if (mac_addr_a2n(mac_addr, argv[0])) {
		fprintf(stderr, "invalid mac address\n");
		return 1;
	}
	argc--;
	argv++;

	if (strcmp("plink_action", argv[0]) != 0)
		return 1;
	argc--;
	argv++;

	if (strcmp("open", argv[0]) == 0)
		plink_action = PLINK_ACTION_OPEN;
	else if (strcmp("block", argv[0]) == 0)
		plink_action = PLINK_ACTION_BLOCK;
	else {
		fprintf(stderr, "plink action not supported\n");
		return 1;
	}
	argc--;
	argv++;

	if (argc)
		return -1;

	NLA_PUT(msg, NL80211_ATTR_MAC, ETH_ALEN, mac_addr);
	NLA_PUT_U8(msg, NL80211_ATTR_STA_PLINK_ACTION, plink_action);

	cb = nl_cb_alloc(NL_CB_CUSTOM);
	if (!cb)
		goto out;

	if (nl_send_auto_complete(state->nl_handle, msg) < 0)
		goto out;

	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, print_sta_handler, NULL);
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
COMMAND(station, set, "<MAC address> plink_action <open|block>",
	NL80211_CMD_SET_STATION, 0, CIB_NETDEV, handle_station_set);

static int handle_station_dump(struct nl80211_state *state,
			       struct nl_msg *msg,
			       int argc, char **argv)
{
	struct nl_cb *cb = NULL;
	int ret = 1;
	int err;
	int finished = 0;

	if (argc)
		return -1;

	cb = nl_cb_alloc(NL_CB_CUSTOM);
	if (!cb)
		goto out;

	if (nl_send_auto_complete(state->nl_handle, msg) < 0)
		goto out;

	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, print_sta_handler, NULL);
	nl_cb_set(cb, NL_CB_FINISH, NL_CB_CUSTOM, wait_handler, &finished);

	err = nl_recvmsgs(state->nl_handle, cb);

	if (err < 0)
		goto out;

	ret = 0;

 out:
	nl_cb_put(cb);
	return ret;
}
COMMAND(station, dump, NULL,
	NL80211_CMD_SET_STATION, NLM_F_DUMP, CIB_NETDEV, handle_station_dump);
