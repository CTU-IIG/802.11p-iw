#ifndef __IW_H
#define __IW_H

#include <linux/nl80211.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>

#define ETH_ALEN 6

struct nl80211_state {
	struct nl_handle *nl_handle;
	struct nl_cache *nl_cache;
	struct genl_family *nl80211;
};

enum command_identify_by {
	CIB_NONE,
	CIB_PHY,
	CIB_NETDEV,
};

struct cmd {
	const char *section;
	const char *name;
	const char *args;
	const enum nl80211_commands cmd;
	int nl_msg_flags;
	const enum command_identify_by idby;
	/*
	 * The handler should return a negative error code,
	 * zero on success, 1 if the arguments were wrong
	 * and the usage message should and 2 otherwise.
	 */
	int (*handler)(struct nl80211_state *state,
		       struct nl_msg *msg,
		       int argc, char **argv);
};

#define __COMMAND(sect, name, args, nlcmd, flags, idby, handler)	\
	static const struct cmd __cmd_ ## handler ## nlcmd ## idby	\
	__attribute__((used)) __attribute__((section("__cmd")))		\
	= { sect, name, args, nlcmd, flags, idby, handler }
#define COMMAND(section, name, args, cmd, flags, idby, handler)	\
	__COMMAND(#section, #name, args, cmd, flags, idby, handler)
#define TOPLEVEL(name, args, cmd, flags, idby, handler)		\
	__COMMAND(NULL, #name, args, cmd, flags, idby, handler)
extern struct cmd __start___cmd;
extern struct cmd __stop___cmd;

int mac_addr_a2n(unsigned char *mac_addr, char *arg);
int mac_addr_n2a(char *mac_addr, unsigned char *arg);

int error_handler(struct sockaddr_nl *nla, struct nlmsgerr *err, void *arg);

const char *iftype_name(enum nl80211_iftype iftype);

#endif /* __IW_H */
