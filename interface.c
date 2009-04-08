#include <net/if.h>
#include <errno.h>
#include <string.h>

#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <netlink/msg.h>
#include <netlink/attr.h>

#include "nl80211.h"
#include "iw.h"

static char *mntr_flags[NL80211_MNTR_FLAG_MAX + 1] = {
	"none",
	"fcsfail",
	"plcpfail",
	"control",
	"otherbss",
	"cook",
};

static int parse_mntr_flags(int *_argc, char ***_argv,
			    struct nl_msg *msg)
{
	struct nl_msg *flags;
	int err = -ENOBUFS;
	enum nl80211_mntr_flags flag;
	int argc = *_argc;
	char **argv = *_argv;

	flags = nlmsg_alloc();
	if (!flags)
		return -ENOMEM;

	while (argc) {
		int ok = 0;
		for (flag = __NL80211_MNTR_FLAG_INVALID;
		     flag <= NL80211_MNTR_FLAG_MAX; flag++) {
			if (strcmp(*argv, mntr_flags[flag]) == 0) {
				ok = 1;
				/*
				 * This shouldn't be adding "flag" if that is
				 * zero, but due to a problem in the kernel's
				 * nl80211 code (using NLA_NESTED policy) it
				 * will reject an empty nested attribute but
				 * not one that contains an invalid attribute
				 */
				NLA_PUT_FLAG(flags, flag);
				break;
			}
		}
		if (!ok) {
			err = -EINVAL;
			goto out;
		}
		argc--;
		argv++;
	}

	nla_put_nested(msg, NL80211_ATTR_MNTR_FLAGS, flags);
	err = 0;
 nla_put_failure:
 out:
	nlmsg_free(flags);

	*_argc = argc;
	*_argv = argv;

	return err;
}

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
	} else if (strcmp(tpstr, "master") == 0) {
		*type = NL80211_IFTYPE_UNSPECIFIED;
		fprintf(stderr, "See http://wireless.kernel.org/RTFM-AP.\n");
		return 2;
	} else if (strcmp(tpstr, "ap") == 0) {
		*type = NL80211_IFTYPE_UNSPECIFIED;
		fprintf(stderr, "See http://wireless.kernel.org/RTFM-AP.\n");
		return 2;
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
				struct nl_cb *cb,
				struct nl_msg *msg,
				int argc, char **argv)
{
	char *name;
	char *mesh_id = NULL;
	enum nl80211_iftype type;
	int tpset;

	if (argc < 1)
		return 1;

	name = argv[0];
	argc--;
	argv++;

	tpset = get_if_type(&argc, &argv, &type);
	if (tpset != 1)
		return tpset;

	if (argc) {
		if (strcmp(argv[0], "mesh_id") == 0) {
			argc--;
			argv++;

			if (!argc)
				return 1;
			mesh_id = argv[0];
			argc--;
			argv++;
		} else if (strcmp(argv[0], "flags") == 0) {
			argc--;
			argv++;
			if (parse_mntr_flags(&argc, &argv, msg)) {
				fprintf(stderr, "flags error\n");
				return 2;
			}
		} else {
			return 1;
		}
	}

	if (argc)
		return 1;

	NLA_PUT_STRING(msg, NL80211_ATTR_IFNAME, name);
	if (tpset)
		NLA_PUT_U32(msg, NL80211_ATTR_IFTYPE, type);
	if (mesh_id)
		NLA_PUT(msg, NL80211_ATTR_MESH_ID, strlen(mesh_id), mesh_id);

	return 0;
 nla_put_failure:
	return -ENOBUFS;
}
COMMAND(interface, add, "<name> type <type> [mesh_id <meshid>] [flags ...]",
	NL80211_CMD_NEW_INTERFACE, 0, CIB_PHY, handle_interface_add);
COMMAND(interface, add, "<name> type <type> [mesh_id <meshid>] [flags ...]",
	NL80211_CMD_NEW_INTERFACE, 0, CIB_NETDEV, handle_interface_add);

static int handle_interface_del(struct nl80211_state *state,
				struct nl_cb *cb,
				struct nl_msg *msg,
				int argc, char **argv)
{
	return 0;
}
TOPLEVEL(del, NULL, NL80211_CMD_DEL_INTERFACE, 0, CIB_NETDEV, handle_interface_del);
HIDDEN(interface, del, NULL, NL80211_CMD_DEL_INTERFACE, 0, CIB_NETDEV, handle_interface_del);

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

static int handle_interface_info(struct nl80211_state *state,
				 struct nl_cb *cb,
				 struct nl_msg *msg,
				 int argc, char **argv)
{
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, print_iface_handler, NULL);
	return 0;
}
TOPLEVEL(info, NULL, NL80211_CMD_GET_INTERFACE, 0, CIB_NETDEV, handle_interface_info);

static int handle_interface_set(struct nl80211_state *state,
				struct nl_cb *cb,
				struct nl_msg *msg,
				int argc, char **argv)
{
	if (!argc)
		return 1;

	NLA_PUT_U32(msg, NL80211_ATTR_IFTYPE, NL80211_IFTYPE_MONITOR);

	switch (parse_mntr_flags(&argc, &argv, msg)) {
	case 0:
		return 0;
	case -ENOMEM:
		fprintf(stderr, "failed to allocate flags\n");
		return 2;
	case -EINVAL:
		fprintf(stderr, "unknown flag %s\n", *argv);
		return 2;
	default:
		return 2;
	}
 nla_put_failure:
	return -ENOBUFS;
}
COMMAND(set, monitor, "<flag> [...]",
	NL80211_CMD_SET_INTERFACE, 0, CIB_NETDEV, handle_interface_set);

static int handle_interface_meshid(struct nl80211_state *state,
				   struct nl_cb *cb,
				   struct nl_msg *msg,
				   int argc, char **argv)
{
	char *mesh_id = NULL;

	if (argc != 1)
		return 1;

	mesh_id = argv[0];

	NLA_PUT(msg, NL80211_ATTR_MESH_ID, strlen(mesh_id), mesh_id);

	return 0;
 nla_put_failure:
	return -ENOBUFS;
}
COMMAND(set, meshid, "<meshid>",
	NL80211_CMD_SET_INTERFACE, 0, CIB_NETDEV, handle_interface_meshid);
