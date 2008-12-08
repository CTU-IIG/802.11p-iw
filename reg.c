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

static int isalpha_upper(char letter)
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

static int handle_reg_set(struct nl_cb *cb,
			  struct nl_msg *msg,
			  int argc, char **argv)
{
	char alpha2[3];

	if (argc < 1)
		return 1;

	if (!is_alpha2(argv[0]) && !is_world_regdom(argv[0])) {
		fprintf(stderr, "not a valid ISO/IEC 3166-1 alpha2\n");
		fprintf(stderr, "Special non-alpha2 usable entries:\n");
		fprintf(stderr, "\t00\tWorld Regulatory domain\n");
		return 2;
	}

	alpha2[0] = argv[0][0];
	alpha2[1] = argv[0][1];
	alpha2[2] = '\0';

	argc--;
	argv++;

	if (argc)
		return 1;

	NLA_PUT_STRING(msg, NL80211_ATTR_REG_ALPHA2, alpha2);

	return 0;
 nla_put_failure:
	return -ENOBUFS;
}
COMMAND(reg, set, "<ISO/IEC 3166-1 alpha2>",
	NL80211_CMD_REQ_SET_REG, 0, CIB_NONE, handle_reg_set);
