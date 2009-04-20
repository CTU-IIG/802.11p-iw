/*
 * nl80211 userspace tool
 *
 * Copyright 2007, 2008	Johannes Berg <johannes@sipsolutions.net>
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
                     
#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>  
#include <netlink/msg.h>
#include <netlink/attr.h>

#include "nl80211.h"
#include "iw.h"

#ifndef CONFIG_LIBNL20
/* libnl 2.0 compatibility code */

static inline struct nl_handle *nl_socket_alloc(void)
{
	return nl_handle_alloc();
}

static inline void nl_socket_free(struct nl_sock *h)
{
	nl_handle_destroy(h);
}

static inline int __genl_ctrl_alloc_cache(struct nl_sock *h, struct nl_cache **cache)
{
	struct nl_cache *tmp = genl_ctrl_alloc_cache(h);
	if (!tmp)
		return -ENOMEM;
	*cache = tmp;
	return 0;
}
#define genl_ctrl_alloc_cache __genl_ctrl_alloc_cache
#endif /* CONFIG_LIBNL20 */

static int debug = 0;

static int nl80211_init(struct nl80211_state *state)
{
	int err;

	state->nl_sock = nl_socket_alloc();
	if (!state->nl_sock) {
		fprintf(stderr, "Failed to allocate netlink socket.\n");
		return -ENOMEM;
	}

	if (genl_connect(state->nl_sock)) {
		fprintf(stderr, "Failed to connect to generic netlink.\n");
		err = -ENOLINK;
		goto out_handle_destroy;
	}

	if (genl_ctrl_alloc_cache(state->nl_sock, &state->nl_cache)) {
		fprintf(stderr, "Failed to allocate generic netlink cache.\n");
		err = -ENOMEM;
		goto out_handle_destroy;
	}

	state->nl80211 = genl_ctrl_search_by_name(state->nl_cache, "nl80211");
	if (!state->nl80211) {
		fprintf(stderr, "nl80211 not found.\n");
		err = -ENOENT;
		goto out_cache_free;
	}

	return 0;

 out_cache_free:
	nl_cache_free(state->nl_cache);
 out_handle_destroy:
	nl_socket_free(state->nl_sock);
	return err;
}

static void nl80211_cleanup(struct nl80211_state *state)
{
	genl_family_put(state->nl80211);
	nl_cache_free(state->nl_cache);
	nl_socket_free(state->nl_sock);
}

__COMMAND(NULL, NULL, "", NULL, 0, 0, 0, CIB_NONE, NULL);
__COMMAND(NULL, NULL, "", NULL, 1, 0, 0, CIB_NONE, NULL);

static int cmd_size;

static void usage(const char *argv0)
{
	struct cmd *cmd;

	fprintf(stderr, "Usage:\t%s [options] command\n", argv0);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "\t--debug\t\tenable netlink debugging\n");
	fprintf(stderr, "\t--version\tshow version\n");
	fprintf(stderr, "Commands:\n");
	fprintf(stderr, "\thelp\n");
	fprintf(stderr, "\tevent\n");
	for (cmd = &__start___cmd; cmd < &__stop___cmd;
	     cmd = (struct cmd *)((char *)cmd + cmd_size)) {
		if (!cmd->handler || cmd->hidden)
			continue;
		switch (cmd->idby) {
		case CIB_NONE:
			fprintf(stderr, "\t");
			break;
		case CIB_PHY:
			fprintf(stderr, "\tphy <phyname> ");
			break;
		case CIB_NETDEV:
			fprintf(stderr, "\tdev <devname> ");
			break;
		}
		if (cmd->section)
			fprintf(stderr, "%s ", cmd->section);
		fprintf(stderr, "%s", cmd->name);
		if (cmd->args)
			fprintf(stderr, " %s", cmd->args);
		fprintf(stderr, "\n");
	}
}

static void version(void)
{
	printf("iw version %s\n", iw_version);
}

static int phy_lookup(char *name)
{
	char buf[200];
	int fd, pos;

	snprintf(buf, sizeof(buf), "/sys/class/ieee80211/%s/index", name);

	fd = open(buf, O_RDONLY);
	if (fd < 0)
		return -1;
	pos = read(fd, buf, sizeof(buf) - 1);
	if (pos < 0)
		return -1;
	buf[pos] = '\0';
	return atoi(buf);
}

static int error_handler(struct sockaddr_nl *nla, struct nlmsgerr *err,
			 void *arg)
{
	int *ret = arg;
	*ret = err->error;
	return NL_STOP;
}

static int finish_handler(struct nl_msg *msg, void *arg)
{
	int *ret = arg;
	*ret = 0;
	return NL_SKIP;
}

static int ack_handler(struct nl_msg *msg, void *arg)
{
	int *ret = arg;
	*ret = 0;
	return NL_STOP;
}

int handle_cmd(struct nl80211_state *state, enum id_input idby,
	       int argc, char **argv)
{
	struct cmd *cmd, *match = NULL;
	struct nl_cb *cb;
	struct nl_msg *msg;
	int devidx = 0;
	int err, o_argc;
	const char *command, *section;
	char *tmp, **o_argv;
	enum command_identify_by command_idby = CIB_NONE;

	if (argc <= 1 && idby != II_NONE)
		return 1;

	o_argc = argc;
	o_argv = argv;

	switch (idby) {
	case II_PHY_IDX:
		command_idby = CIB_PHY;
		devidx = strtoul(*argv + 4, &tmp, 0);
		if (*tmp != '\0')
			return 1;
		argc--;
		argv++;
		break;
	case II_PHY_NAME:
		command_idby = CIB_PHY;
		devidx = phy_lookup(*argv);
		argc--;
		argv++;
		break;
	case II_NETDEV:
		command_idby = CIB_NETDEV;
		devidx = if_nametoindex(*argv);
		if (devidx == 0)
			devidx = -1;
		argc--;
		argv++;
		break;
	default:
		break;
	}

	if (devidx < 0)
		return -errno;

	section = command = *argv;
	argc--;
	argv++;

	for (cmd = &__start___cmd; cmd < &__stop___cmd;
	     cmd = (struct cmd *)((char *)cmd + cmd_size)) {
		if (!cmd->handler)
			continue;
		if (cmd->idby != command_idby)
			continue;
		if (cmd->section) {
			if (strcmp(cmd->section, section))
				continue;
			/* this is a bit icky ... */
			if (command == section) {
				if (argc <= 0) {
					if (match)
						break;
					return 1;
				}
				command = *argv;
				argc--;
				argv++;
			}
		} else if (section != command)
			continue;
		if (strcmp(cmd->name, command))
			continue;
		if (argc && !cmd->args)
			continue;

		match = cmd;
	}

	cmd = match;

	if (!cmd)
		return 1;

	if (!cmd->cmd) {
		argc = o_argc;
		argv = o_argv;
		return cmd->handler(state, NULL, NULL, argc, argv);
	}

	msg = nlmsg_alloc();
	if (!msg) {
		fprintf(stderr, "failed to allocate netlink message\n");
		return 2;
	}

	cb = nl_cb_alloc(debug ? NL_CB_DEBUG : NL_CB_DEFAULT);
	if (!cb) {
		fprintf(stderr, "failed to allocate netlink callbacks\n");
		err = 2;
		goto out_free_msg;
	}

	genlmsg_put(msg, 0, 0, genl_family_get_id(state->nl80211), 0,
		    cmd->nl_msg_flags, cmd->cmd, 0);

	switch (command_idby) {
	case CIB_PHY:
		NLA_PUT_U32(msg, NL80211_ATTR_WIPHY, devidx);
		break;
	case CIB_NETDEV:
		NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, devidx);
		break;
	default:
		break;
	}

	err = cmd->handler(state, cb, msg, argc, argv);
	if (err)
		goto out;

	err = nl_send_auto_complete(state->nl_sock, msg);
	if (err < 0)
		goto out;

	err = 1;

	nl_cb_err(cb, NL_CB_CUSTOM, error_handler, &err);
	nl_cb_set(cb, NL_CB_FINISH, NL_CB_CUSTOM, finish_handler, &err);
	nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, ack_handler, &err);

	while (err > 0)
		nl_recvmsgs(state->nl_sock, cb);
 out:
	nl_cb_put(cb);
 out_free_msg:
	nlmsg_free(msg);
	return err;
 nla_put_failure:
	fprintf(stderr, "building message failed\n");
	return 2;
}

static int no_seq_check(struct nl_msg *msg, void *arg)
{
	return NL_OK;
}

static int print_event(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	char ifname[100];
	char macbuf[6*3];
	__u8 reg_type;

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);
                          
	switch (gnlh->cmd) {
	case NL80211_CMD_NEW_WIPHY:
		printf("wiphy rename: phy #%d to %s\n",
		       nla_get_u32(tb[NL80211_ATTR_WIPHY]),
		       nla_get_string(tb[NL80211_ATTR_WIPHY_NAME]));
		break;
	case NL80211_CMD_NEW_SCAN_RESULTS:
		if_indextoname(nla_get_u32(tb[NL80211_ATTR_IFINDEX]), ifname);
		printf("scan finished on %s (phy #%d)\n",
		       ifname, nla_get_u32(tb[NL80211_ATTR_WIPHY]));
		break;
	case NL80211_CMD_SCAN_ABORTED:
		if_indextoname(nla_get_u32(tb[NL80211_ATTR_IFINDEX]), ifname);
		printf("scan aborted on %s (phy #%d)\n",
		       ifname, nla_get_u32(tb[NL80211_ATTR_WIPHY]));
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
		if_indextoname(nla_get_u32(tb[NL80211_ATTR_IFINDEX]), ifname);
		mac_addr_n2a(macbuf, nla_data(tb[NL80211_ATTR_MAC]));
		printf("IBSS %s joined on %s (phy #%d)\n",
		       macbuf, ifname, nla_get_u32(tb[NL80211_ATTR_WIPHY]));
		break;
	default:
		printf("unknown event: %d\n", gnlh->cmd);
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

__u32 listen_events(struct nl80211_state *state,
		    const int n_waits, const __u32 *waits)
{
	int mcid, ret;
	struct nl_cb *cb = nl_cb_alloc(debug ? NL_CB_DEBUG : NL_CB_DEFAULT);
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
		nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, print_event, NULL);
	}

	wait_ev.cmd = 0;

	while (!wait_ev.cmd)
		nl_recvmsgs(state->nl_sock, cb);

	nl_cb_put(cb);

	return wait_ev.cmd;
}

int main(int argc, char **argv)
{
	struct nl80211_state nlstate;
	int err;
	const char *argv0;

	/* calculate command size including padding */
	cmd_size = abs((long)&__cmd_NULL_NULL_1_CIB_NONE_0
	             - (long)&__cmd_NULL_NULL_0_CIB_NONE_0);
	/* strip off self */
	argc--;
	argv0 = *argv++;

	if (argc > 0 && strcmp(*argv, "--debug") == 0) {
		debug = 1;
		argc--;
		argv++;
	}

	if (argc > 0 && strcmp(*argv, "--version") == 0) {
		version();
		return 0;
	}

	if (argc == 0 || strcmp(*argv, "help") == 0) {
		usage(argv0);
		return 0;
	}

	err = nl80211_init(&nlstate);
	if (err)
		return 1;

	if (strcmp(*argv, "event") == 0) {
		if (argc != 1)
			err = 1;
		else
			err = listen_events(&nlstate, 0, NULL);
	} else if (strcmp(*argv, "dev") == 0 && argc > 1) {
		argc--;
		argv++;
		err = handle_cmd(&nlstate, II_NETDEV, argc, argv);
	} else if (strncmp(*argv, "phy", 3) == 0 && argc > 1) {
		if (strlen(*argv) == 3) {
			argc--;
			argv++;
			err = handle_cmd(&nlstate, II_PHY_NAME, argc, argv);
		} else if (*(*argv + 3) == '#')
			err = handle_cmd(&nlstate, II_PHY_IDX, argc, argv);
		else
			err = 1;
	} else
		err = handle_cmd(&nlstate, II_NONE, argc, argv);

	if (err == 1)
		usage(argv0);
	if (err < 0)
		fprintf(stderr, "command failed: %s (%d)\n", strerror(-err), err);

	nl80211_cleanup(&nlstate);

	return err;
}
