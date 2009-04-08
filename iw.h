#ifndef __IW_H
#define __IW_H

#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>

#include "nl80211.h"

#define ETH_ALEN 6

#ifndef CONFIG_LIBNL20
#  define nl_sock nl_handle
#endif

struct nl80211_state {
	struct nl_sock *nl_sock;
	struct nl_cache *nl_cache;
	struct genl_family *nl80211;
};

enum command_identify_by {
	CIB_NONE,
	CIB_PHY,
	CIB_NETDEV,
};

enum id_input {
	II_NONE,
	II_NETDEV,
	II_PHY_NAME,
	II_PHY_IDX,
};

struct cmd {
	const char *section;
	const char *name;
	const char *args;
	const enum nl80211_commands cmd;
	int nl_msg_flags;
	int hidden;
	const enum command_identify_by idby;
	/*
	 * The handler should return a negative error code,
	 * zero on success, 1 if the arguments were wrong
	 * and the usage message should and 2 otherwise.
	 */
	int (*handler)(struct nl80211_state *state,
		       struct nl_cb *cb,
		       struct nl_msg *msg,
		       int argc, char **argv);
};

#define ARRAY_SIZE(ar) (sizeof(ar)/sizeof(ar[0]))

#define __COMMAND(sect, name, args, nlcmd, flags, hidden, idby, handler)\
	static const struct cmd						\
	__cmd_ ## handler ## _ ## nlcmd ## _ ## idby ## _ ## hidden	\
	__attribute__((used)) __attribute__((section("__cmd")))		\
	= { sect, name, args, nlcmd, flags, hidden, idby, handler }
#define COMMAND(section, name, args, cmd, flags, idby, handler)	\
	__COMMAND(#section, #name, args, cmd, flags, 0, idby, handler)
#define HIDDEN(section, name, args, cmd, flags, idby, handler)	\
	__COMMAND(#section, #name, args, cmd, flags, 1, idby, handler)
#define TOPLEVEL(name, args, cmd, flags, idby, handler)		\
	__COMMAND(NULL, #name, args, cmd, flags, 0, idby, handler)
extern struct cmd __start___cmd;
extern struct cmd __stop___cmd;


int handle_cmd(struct nl80211_state *state, enum id_input idby,
	       int argc, char **argv);
__u32 listen_events(struct nl80211_state *state,
		    const int n_waits, const __u32 *waits);


int mac_addr_a2n(unsigned char *mac_addr, char *arg);
int mac_addr_n2a(char *mac_addr, unsigned char *arg);

const char *iftype_name(enum nl80211_iftype iftype);
int ieee80211_channel_to_frequency(int chan);
int ieee80211_frequency_to_channel(int freq);

int nl_get_multicast_id(struct nl_sock *sock, const char *family, const char *group);

char *reg_initiator_to_string(__u8 initiator);

#endif /* __IW_H */
