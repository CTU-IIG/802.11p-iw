#include <stdint.h>
#include <stdbool.h>
#include <net/if.h>
#include <errno.h>
#include "iw.h"

static int no_seq_check(struct nl_msg *msg, void *arg)
{
	return NL_OK;
}

struct print_event_args {
	bool frame, time;
};

static void print_frame(struct print_event_args *args, struct nlattr *attr)
{
	uint8_t *frame;
	size_t len;
	int i;
	char macbuf[6*3];
	uint16_t tmp;

	if (!attr)
		printf(" [no frame]");

	frame = nla_data(attr);
	len = nla_len(attr);

	if (len < 26) {
		printf(" [invalid frame: ");
		goto print_frame;
	}

	mac_addr_n2a(macbuf, frame + 10);
	printf(" %s -> ", macbuf);
	mac_addr_n2a(macbuf, frame + 4);
	printf("%s", macbuf);

	switch (frame[0] & 0xfc) {
	case 0x10: /* assoc resp */
	case 0x30: /* reassoc resp */
		/* status */
		tmp = (frame[27] << 8) + frame[26];
		printf(" status: %d: %s", tmp, get_status_str(tmp));
		break;
	case 0x00: /* assoc req */
	case 0x20: /* reassoc req */
		break;
	case 0xb0: /* auth */
		/* status */
		tmp = (frame[29] << 8) + frame[28];
		printf(" status: %d: %s", tmp, get_status_str(tmp));
		break;
		break;
	case 0xa0: /* disassoc */
	case 0xc0: /* deauth */
		/* reason */
		tmp = (frame[25] << 8) + frame[24];
		printf(" reason %d: %s", tmp, get_reason_str(tmp));
		break;
	}

	if (!args->frame)
		return;

	printf(" [frame:");

 print_frame:
	for (i = 0; i < len; i++)
		printf(" %.02x", frame[i]);
	printf("]");
}

static int print_event(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *tb[NL80211_ATTR_MAX + 1], *nst;
	struct print_event_args *args = arg;
	char ifname[100];
	char macbuf[6*3];
	__u8 reg_type;
	int rem_nst;

	if (args->time) {
		struct timeval tv;
		gettimeofday(&tv, NULL);
		printf("%ld.%06u: ", (long) tv.tv_sec, (unsigned int) tv.tv_usec);
	}

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (tb[NL80211_ATTR_IFINDEX] && tb[NL80211_ATTR_WIPHY]) {
		if_indextoname(nla_get_u32(tb[NL80211_ATTR_IFINDEX]), ifname);
		printf("%s (phy #%d): ", ifname, nla_get_u32(tb[NL80211_ATTR_WIPHY]));
	} else if (tb[NL80211_ATTR_IFINDEX]) {
		if_indextoname(nla_get_u32(tb[NL80211_ATTR_IFINDEX]), ifname);
		printf("%s: ", ifname);
	} else if (tb[NL80211_ATTR_WIPHY]) {
		printf("phy #%d: ", nla_get_u32(tb[NL80211_ATTR_WIPHY]));
	}

	switch (gnlh->cmd) {
	case NL80211_CMD_NEW_WIPHY:
		printf("renamed to %s\n", nla_get_string(tb[NL80211_ATTR_WIPHY_NAME]));
		break;
	case NL80211_CMD_TRIGGER_SCAN:
		printf("scan started\n");
		break;
	case NL80211_CMD_NEW_SCAN_RESULTS:
		printf("scan finished:");
	case NL80211_CMD_SCAN_ABORTED:
		if (gnlh->cmd == NL80211_CMD_SCAN_ABORTED)
			printf("scan aborted:");
		if (tb[NL80211_ATTR_SCAN_FREQUENCIES]) {
			nla_for_each_nested(nst, tb[NL80211_ATTR_SCAN_FREQUENCIES], rem_nst)
				printf(" %d", nla_get_u32(nst));
			printf(",");
		}
		if (tb[NL80211_ATTR_SCAN_SSIDS]) {
			nla_for_each_nested(nst, tb[NL80211_ATTR_SCAN_SSIDS], rem_nst) {
				printf(" \"");
				print_ssid_escaped(nla_len(nst), nla_data(nst));
				printf("\"");
			}
		}
		printf("\n");
		break;
	case NL80211_CMD_REG_CHANGE:
		printf("regulatory domain change: ");

		reg_type = nla_get_u8(tb[NL80211_ATTR_REG_TYPE]);

		switch (reg_type) {
		case NL80211_REGDOM_TYPE_COUNTRY:
			printf("set to %s by %s request",
			       nla_get_string(tb[NL80211_ATTR_REG_ALPHA2]),
			       reg_initiator_to_string(nla_get_u8(tb[NL80211_ATTR_REG_INITIATOR])));
			if (tb[NL80211_ATTR_WIPHY])
				printf(" on phy%d", nla_get_u32(tb[NL80211_ATTR_WIPHY]));
			break;
		case NL80211_REGDOM_TYPE_WORLD:
			printf("set to world roaming by %s request",
			       reg_initiator_to_string(nla_get_u8(tb[NL80211_ATTR_REG_INITIATOR])));
			break;
		case NL80211_REGDOM_TYPE_CUSTOM_WORLD:
			printf("custom world roaming rules in place on phy%d by %s request",
			       nla_get_u32(tb[NL80211_ATTR_WIPHY]),
			       reg_initiator_to_string(nla_get_u32(tb[NL80211_ATTR_REG_INITIATOR])));
			break;
		case NL80211_REGDOM_TYPE_INTERSECTION:
			printf("intersection used due to a request made by %s",
			       reg_initiator_to_string(nla_get_u32(tb[NL80211_ATTR_REG_INITIATOR])));
			if (tb[NL80211_ATTR_WIPHY])
				printf(" on phy%d", nla_get_u32(tb[NL80211_ATTR_WIPHY]));
			break;
		default:
			printf("unknown source (upgrade this utility)");
			break;
		}

		printf("\n");
		break;
	case NL80211_CMD_JOIN_IBSS:
		mac_addr_n2a(macbuf, nla_data(tb[NL80211_ATTR_MAC]));
		printf("IBSS %s joined\n", macbuf);
		break;
	case NL80211_CMD_AUTHENTICATE:
		printf("auth");
		if (tb[NL80211_ATTR_FRAME])
			print_frame(args, tb[NL80211_ATTR_FRAME]);
		else if (tb[NL80211_ATTR_TIMED_OUT])
			printf(": timed out");
		else
			printf(": unknown event");
		printf("\n");
		break;
	case NL80211_CMD_ASSOCIATE:
		printf("assoc");
		if (tb[NL80211_ATTR_FRAME])
			print_frame(args, tb[NL80211_ATTR_FRAME]);
		else if (tb[NL80211_ATTR_TIMED_OUT])
			printf(": timed out");
		else
			printf(": unknown event");
		printf("\n");
		break;
	case NL80211_CMD_DEAUTHENTICATE:
		printf("deauth");
		print_frame(args, tb[NL80211_ATTR_FRAME]);
		printf("\n");
		break;
	case NL80211_CMD_DISASSOCIATE:
		printf("disassoc");
		print_frame(args, tb[NL80211_ATTR_FRAME]);
		printf("\n");
		break;
	default:
		printf("unknown event %d\n", gnlh->cmd);
		break;
	}

	return NL_SKIP;
}

struct wait_event {
	int n_cmds;
	const __u32 *cmds;
	__u32 cmd;
};

static int wait_event(struct nl_msg *msg, void *arg)
{
	struct wait_event *wait = arg;
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	int i;

	for (i = 0; i < wait->n_cmds; i++) {
		if (gnlh->cmd == wait->cmds[i]) {
			wait->cmd = gnlh->cmd;
		}
	}

	return NL_SKIP;
}

static __u32 __listen_events(struct nl80211_state *state,
			     const int n_waits, const __u32 *waits,
			     struct print_event_args *args)
{
	int mcid, ret;
	struct nl_cb *cb = nl_cb_alloc(iw_debug ? NL_CB_DEBUG : NL_CB_DEFAULT);
	struct wait_event wait_ev;

	if (!cb) {
		fprintf(stderr, "failed to allocate netlink callbacks\n");
		return -ENOMEM;
	}

	/* Configuration multicast group */
	mcid = nl_get_multicast_id(state->nl_sock, "nl80211", "config");
	if (mcid < 0)
		return mcid;

	ret = nl_socket_add_membership(state->nl_sock, mcid);
	if (ret)
		return ret;

	/* Scan multicast group */
	mcid = nl_get_multicast_id(state->nl_sock, "nl80211", "scan");
	if (mcid >= 0) {
		ret = nl_socket_add_membership(state->nl_sock, mcid);
		if (ret)
			return ret;
	}

	/* Regulatory multicast group */
	mcid = nl_get_multicast_id(state->nl_sock, "nl80211", "regulatory");
	if (mcid >= 0) {
		ret = nl_socket_add_membership(state->nl_sock, mcid);
		if (ret)
			return ret;
	}

	/* MLME multicast group */
	mcid = nl_get_multicast_id(state->nl_sock, "nl80211", "mlme");
	if (mcid >= 0) {
		ret = nl_socket_add_membership(state->nl_sock, mcid);
		if (ret)
			return ret;
	}

	/* no sequence checking for multicast messages */
	nl_cb_set(cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM, no_seq_check, NULL);

	if (n_waits && waits) {
		wait_ev.cmds = waits;
		wait_ev.n_cmds = n_waits;
		nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, wait_event, &wait_ev);
	} else {
		nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, print_event, args);
	}

	wait_ev.cmd = 0;

	while (!wait_ev.cmd)
		nl_recvmsgs(state->nl_sock, cb);

	nl_cb_put(cb);

	return wait_ev.cmd;
}

__u32 listen_events(struct nl80211_state *state,
		    const int n_waits, const __u32 *waits)
{
	return __listen_events(state, n_waits, waits, NULL);
}

static int print_events(struct nl80211_state *state,
			struct nl_cb *cb,
			struct nl_msg *msg,
			int argc, char **argv)
{
	struct print_event_args args;

	memset(&args, 0, sizeof(args));

	argc--;
	argv++;

	while (argc > 0) {
		if (strcmp(argv[0], "-f") == 0)
			args.frame = true;
		else if (strcmp(argv[0], "-t") == 0)
			args.time = true;
		else
			return 1;
		argc--;
		argv++;
	}

	if (argc)
		return 1;

	return __listen_events(state, 0, NULL, &args);
}
TOPLEVEL(event, "[-t] [-f]", 0, 0, CIB_NONE, print_events,
	"Monitor events from the kernel.\n"
	"-t - print timestamp\n"
	"-f - print full frame for auth/assoc etc.");
