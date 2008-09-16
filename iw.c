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
#include <linux/nl80211.h>

#include "iw.h"

int debug = 0;

static int nl80211_init(struct nl80211_state *state)
{
	int err;

	state->nl_handle = nl_handle_alloc();
	if (!state->nl_handle) {
		fprintf(stderr, "Failed to allocate netlink handle.\n");
		return -ENOMEM;
	}

	if (genl_connect(state->nl_handle)) {
		fprintf(stderr, "Failed to connect to generic netlink.\n");
		err = -ENOLINK;
		goto out_handle_destroy;
	}

	state->nl_cache = genl_ctrl_alloc_cache(state->nl_handle);
	if (!state->nl_cache) {
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
	nl_handle_destroy(state->nl_handle);
	return err;
}

static void nl80211_cleanup(struct nl80211_state *state)
{
	genl_family_put(state->nl80211);
	nl_cache_free(state->nl_cache);
	nl_handle_destroy(state->nl_handle);
}

static void usage(const char *argv0)
{
	struct cmd *cmd;

	fprintf(stderr, "Usage:\t%s [options] command\n", argv0);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "\t--debug\tenable netlink debugging\n");
	fprintf(stderr, "Commands:\n");
	for (cmd = &__start___cmd; cmd < &__stop___cmd; cmd++) {
		switch (cmd->idby) {
		case CIB_NONE:
			fprintf(stderr, "\t");
			/* fall through */
		case CIB_PHY:
			if (cmd->idby == CIB_PHY)
				fprintf(stderr, "\tphy <phyname> ");
			/* fall through */
		case CIB_NETDEV:
			if (cmd->idby == CIB_NETDEV)
				fprintf(stderr, "\tdev <devname> ");
			if (cmd->section)
				fprintf(stderr, "%s ", cmd->section);
			fprintf(stderr, "%s", cmd->name);
			if (cmd->args)
				fprintf(stderr, " %s", cmd->args);
			fprintf(stderr, "\n");
			break;
		}
	}
}

static int phy_lookup(char *name)
{
	char buf[200];
	int fd, pos;

	snprintf(buf, sizeof(buf), "/sys/class/ieee80211/%s/index", name);

	fd = open(buf, O_RDONLY);
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
	return NL_SKIP;
}

static int ack_handler(struct nl_msg *msg, void *arg)
{
	int *ret = arg;
	*ret = 0;
	return NL_STOP;
}

static int handle_cmd(struct nl80211_state *state,
		      enum command_identify_by idby,
		      int argc, char **argv)
{
	struct cmd *cmd;
	struct nl_cb *cb = NULL;
	struct nl_msg *msg;
	int devidx = 0;
	int err;
	const char *command, *section;

	if (argc <= 1 && idby != CIB_NONE)
		return 1;

	switch (idby) {
	case CIB_PHY:
		devidx = phy_lookup(*argv);
		argc--;
		argv++;
		break;
	case CIB_NETDEV:
		devidx = if_nametoindex(*argv);
		argc--;
		argv++;
		break;
	default:
		break;
	}

	section = command = *argv;
	argc--;
	argv++;

	for (cmd = &__start___cmd; cmd < &__stop___cmd; cmd++) {
		if (cmd->idby != idby)
			continue;
		if (cmd->section) {
			if (strcmp(cmd->section, section))
				continue;
			/* this is a bit icky ... */
			if (command == section) {
				if (argc <= 0)
					return 1;
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
		break;
	}

	if (cmd == &__stop___cmd)
		return 1;

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

	switch (idby) {
	case CIB_PHY:
		NLA_PUT_U32(msg, NL80211_ATTR_WIPHY, devidx);
		break;
	case CIB_NETDEV:
		NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, devidx);
		break;
	default:
		break;
	}

	err = cmd->handler(cb, msg, argc, argv);
	if (err)
		goto out;

	err = nl_send_auto_complete(state->nl_handle, msg);
	if (err < 0)
		goto out;

	nl_cb_err(cb, NL_CB_CUSTOM, error_handler, &err);
	nl_cb_set(cb, NL_CB_FINISH, NL_CB_CUSTOM, finish_handler, NULL);
	nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, ack_handler, &err);

	nl_recvmsgs(state->nl_handle, cb);
 out:
	nl_cb_put(cb);
 out_free_msg:
	nlmsg_free(msg);
	return err;
 nla_put_failure:
	fprintf(stderr, "building message failed\n");
	return 2;
}

int main(int argc, char **argv)
{
	struct nl80211_state nlstate;
	int err;
	const char *argv0;

	err = nl80211_init(&nlstate);
	if (err)
		return 1;

	/* strip off self */
	argc--;
	argv0 = *argv++;

	if (argc > 0 && strcmp(*argv, "--debug") == 0) {
		debug = 1;
		argc--;
		argv++;
	}

	if (argc == 0 || strcmp(*argv, "help") == 0) {
		usage(argv0);
		goto out;
	}

	if (strcmp(*argv, "dev") == 0) {
		argc--;
		argv++;
		err = handle_cmd(&nlstate, CIB_NETDEV, argc, argv);
	} else if (strcmp(*argv, "phy") == 0) {
		argc--;
		argv++;
		err = handle_cmd(&nlstate, CIB_PHY, argc, argv);
	} else
		err = handle_cmd(&nlstate, CIB_NONE, argc, argv);

	if (err == 1)
		usage(argv0);
	if (err < 0)
		fprintf(stderr, "command failed: %s (%d)\n", strerror(-err), err);

 out:
	nl80211_cleanup(&nlstate);

	return err;
}
