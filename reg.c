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

static int reg_handler(struct nl_msg *msg, void *arg)
{
        return NL_SKIP;
}

int isalpha_upper(char letter)
{
	if (letter >= 65 && letter <= 90)
		return 1;
	return 0;
}

static int is_alpha2(char *alpha2)
{
	if (isalpha_upper(alpha2[0]) && isalpha_upper(alpha2[1]))
		return 1;
	return 0;
}

static int is_world_regdom(char *alpha2)
{
	/* ASCII 0 */
	if (alpha2[0] == 48 && alpha2[1] == 48)
		return 1;
	return 0;
}

static int handle_reg_set(struct nl80211_state *state,
			int argc, char **argv)
{
	struct nl_msg *msg;
	struct nl_cb *cb = NULL;
	int ret = -1;
	int err;
	int finished = 0;
	char alpha2[3];

	if (argc < 1) {
		fprintf(stderr, "not enough arguments\n");
		return -1;
	}

	if (!is_alpha2(argv[0]) && !is_world_regdom(argv[0])) {
		fprintf(stderr, "not a valid ISO/IEC 3166-1 alpha2\n");
		fprintf(stderr, "Special non-alph2 usable entries:\n");
		fprintf(stderr, "\t00\tWorld Regulatory domain\n");
		return -1;
	}

	alpha2[0] = argv[0][0];
	alpha2[1] = argv[0][1];
	alpha2[2] = '\0';

	argc--;
	argv++;

	if (argc) {
		fprintf(stderr, "too many arguments\n");
		return -1;
	}

	msg = nlmsg_alloc();
	if (!msg)
		goto out;

	genlmsg_put(msg, 0, 0, genl_family_get_id(state->nl80211), 0,
		    0, NL80211_CMD_REQ_SET_REG, 0);

	NLA_PUT_STRING(msg, NL80211_ATTR_REG_ALPHA2, alpha2);

	cb = nl_cb_alloc(NL_CB_CUSTOM);
	if (!cb)
		goto out;

	err = nl_send_auto_complete(state->nl_handle, msg);

	if (err < 0) {
		fprintf(stderr, "failed to send reg set command\n");
		goto out;
	}

	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, reg_handler, NULL);
	nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, wait_handler, &finished);
	nl_cb_err(cb, NL_CB_CUSTOM, error_handler, NULL);

	err = nl_recvmsgs(state->nl_handle, cb);

	if (!finished) {
		err = nl_wait_for_ack(state->nl_handle);
	}

	if (err < 0)
		goto out;

	ret = 0;

 out:
	nl_cb_put(cb);
 nla_put_failure:
	nlmsg_free(msg);
	return ret;
}

int handle_reg(struct nl80211_state *state,
		   int argc, char **argv)
{
	char *cmd = argv[0];

	if (argc < 1) {
		fprintf(stderr, "you must specify an station command\n");
		return -1;
	}

	argc--;
	argv++;

	/* XXX: write support for getting the currently set regdomain
	if (strcmp(cmd, "get") == 0)
		return handle_reg_get(state, argc, argv);
	*/

	if (strcmp(cmd, "set") == 0)
		return handle_reg_set(state, argc, argv);

	printf("invalid regulatory command %s\n", cmd);
	return -1;
}
