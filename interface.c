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

/* return 0 if not found, 1 if ok, -1 on error */
static int get_if_type(int *argc, char ***argv, enum nl80211_iftype *type)
{
	char *tpstr;

	if (*argc < 2)
		return 0;

	if (strcmp((*argv)[0], "type"))
		return 0;

	tpstr = (*argv)[1];
	*argc -= 2;
	*argv += 2;

	if (strcmp(tpstr, "adhoc") == 0 ||
	    strcmp(tpstr, "ibss") == 0) {
		*type = NL80211_IFTYPE_ADHOC;
		return 1;
	} else if (strcmp(tpstr, "monitor") == 0) {
		*type = NL80211_IFTYPE_MONITOR;
		return 1;
	} else if (strcmp(tpstr, "__ap") == 0) {
		*type = NL80211_IFTYPE_AP;
		return 1;
	} else if (strcmp(tpstr, "__ap_vlan") == 0) {
		*type = NL80211_IFTYPE_AP_VLAN;
		return 1;
	} else if (strcmp(tpstr, "wds") == 0) {
		*type = NL80211_IFTYPE_WDS;
		return 1;
	} else if (strcmp(tpstr, "station") == 0) {
		*type = NL80211_IFTYPE_STATION;
		return 1;
	} else if (strcmp(tpstr, "mp") == 0 ||
			strcmp(tpstr, "mesh") == 0) {
		*type = NL80211_IFTYPE_MESH_POINT;
		return 1;
	}


	fprintf(stderr, "invalid interface type %s\n", tpstr);
	return -1;
}

static int handle_interface_add(struct nl80211_state *state,
				char *phy, char *dev, int argc, char **argv)
{
	char *name;
	char *mesh_id = NULL;
	enum nl80211_iftype type;
	int tpset, err = -ENOBUFS;
	struct nl_msg *msg;

	if (argc < 1) {
		fprintf(stderr, "not enough arguments\n");
		return -1;
	}

	name = argv[0];
	argc--;
	argv++;

	tpset = get_if_type(&argc, &argv, &type);
	if (tpset == 0)
		fprintf(stderr, "you must specify an interface type\n");
	if (tpset <= 0)
		return -1;

	if (argc) {
		if (strcmp(argv[0], "mesh_id") != 0) {
			fprintf(stderr, "option %s not supported\n", argv[0]);
			return -1;
		}
		argc--;
		argv++;

		if (!argc) {
			fprintf(stderr, "not enough arguments\n");
			return -1;
		}
		mesh_id = argv[0];
		argc--;
		argv++;
	}

	if (argc) {
		fprintf(stderr, "too many arguments\n");
		return -1;
	}

	msg = nlmsg_alloc();
	if (!msg) {
		fprintf(stderr, "failed to allocate netlink msg\n");
		return -1;
	}

	genlmsg_put(msg, 0, 0, genl_family_get_id(state->nl80211), 0,
		    0, NL80211_CMD_NEW_INTERFACE, 0);
	if (dev)
		NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, if_nametoindex(dev));
	if (phy)
		return -1; /* XXX TODO */
	NLA_PUT_STRING(msg, NL80211_ATTR_IFNAME, name);
	if (tpset)
		NLA_PUT_U32(msg, NL80211_ATTR_IFTYPE, type);
	if (mesh_id)
		NLA_PUT(msg, NL80211_ATTR_MESH_ID, strlen(mesh_id), mesh_id);

	if ((err = nl_send_auto_complete(state->nl_handle, msg)) < 0 ||
	    (err = nl_wait_for_ack(state->nl_handle)) < 0) {
 nla_put_failure:
		fprintf(stderr, "failed to create interface: %d\n", err);
		nlmsg_free(msg);
		return -1;
	}

	nlmsg_free(msg);

	return 0;
}

static int handle_interface_del(struct nl80211_state *state,
				char *phy, char *dev, int argc, char **argv)
{
	int err = -ENOBUFS;
	struct nl_msg *msg;

	if (argc) {
		fprintf(stderr, "too many arguments\n");
		return -1;
	}

        msg = nlmsg_alloc();
	if (!msg)
        	return -1;

	genlmsg_put(msg, 0, 0, genl_family_get_id(state->nl80211), 0,
		    0, NL80211_CMD_DEL_INTERFACE, 0);
	if (!dev) {
		fprintf(stderr, "need device\n");
		nlmsg_free(msg);
		return -1;
	}
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, if_nametoindex(dev));

	if ((err = nl_send_auto_complete(state->nl_handle, msg)) < 0 ||
	    (err = nl_wait_for_ack(state->nl_handle)) < 0) {
 nla_put_failure:
		fprintf(stderr, "failed to remove interface: %d\n", err);
		nlmsg_free(msg);
		return -1;
	}

	nlmsg_free(msg);

	return 0;
}

static int print_iface_handler(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *tb_msg[NL80211_ATTR_MAX + 1];

	nla_parse(tb_msg, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (tb_msg[NL80211_ATTR_IFNAME])
		printf("Interface %s\n", nla_get_string(tb_msg[NL80211_ATTR_IFNAME]));
	if (tb_msg[NL80211_ATTR_IFINDEX])
		printf("\tifindex %d\n", nla_get_u32(tb_msg[NL80211_ATTR_IFINDEX]));
	if (tb_msg[NL80211_ATTR_IFTYPE])
		printf("\ttype %s\n", iftype_name(nla_get_u32(tb_msg[NL80211_ATTR_IFTYPE])));

	return NL_SKIP;
}

static int ack_wait_handler(struct nl_msg *msg, void *arg)
{
	int *finished = arg;

	*finished = 1;
	return NL_STOP;
}

static int handle_interface_info(struct nl80211_state *state,
				 char *phy, char *dev,
				 int argc, char **argv, int flags)
{
	int err = -ENOBUFS;
	struct nl_msg *msg;
	struct nl_cb *cb = NULL;
	int finished = 0;

	if (argc) {
		fprintf(stderr, "too many arguments\n");
		return -1;
	}

        msg = nlmsg_alloc();
	if (!msg)
        	return -1;

	genlmsg_put(msg, 0, 0, genl_family_get_id(state->nl80211), 0,
		    flags, NL80211_CMD_GET_INTERFACE, 0);
	if (!dev) {
		fprintf(stderr, "need device\n");
		nlmsg_free(msg);
		return -1;
	}
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, if_nametoindex(dev));

	cb = nl_cb_alloc(NL_CB_CUSTOM);
	if (!cb)
		goto out;

	if (nl_send_auto_complete(state->nl_handle, msg) < 0)
		goto out;

	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, print_iface_handler, NULL);
	nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, ack_wait_handler, &finished);

	err = nl_recvmsgs(state->nl_handle, cb);

	if (!finished)
		err = nl_wait_for_ack(state->nl_handle);

	if (err)
		fprintf(stderr, "failed to get information: %d\n", err);

 out:
 nla_put_failure:
	nlmsg_free(msg);
	nl_cb_put(cb);
	return 0;
}

int handle_interface(struct nl80211_state *state,
		     char *phy, char *dev, int argc, char **argv)
{
	char *cmd = argv[0];

	if (argc < 1) {
		fprintf(stderr, "you must specify an interface command\n");
		return -1;
	}

	argc--;
	argv++;

	if (strcmp(cmd, "add") == 0)
		return handle_interface_add(state, phy, dev, argc, argv);
	else if (strcmp(cmd, "del") == 0)
		return handle_interface_del(state, phy, dev, argc, argv);
	else if (strcmp(cmd, "get") == 0)
		return handle_interface_info(state, phy, dev, argc, argv, 0);
	else if (strcmp(cmd, "dump") == 0)
		return handle_interface_info(state, phy, dev, argc, argv, NLM_F_DUMP);

	printf("invalid interface command %s\n", cmd);
	return -1;
}
