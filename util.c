#include <ctype.h>
#include <netlink/attr.h>
#include <errno.h>
#include <stdbool.h>
#include "iw.h"
#include "nl80211.h"

void mac_addr_n2a(char *mac_addr, unsigned char *arg)
{
	int i, l;

	l = 0;
	for (i = 0; i < ETH_ALEN ; i++) {
		if (i == 0) {
			sprintf(mac_addr+l, "%02x", arg[i]);
			l += 2;
		} else {
			sprintf(mac_addr+l, ":%02x", arg[i]);
			l += 3;
		}
	}
}

int mac_addr_a2n(unsigned char *mac_addr, char *arg)
{
	int i;

	for (i = 0; i < ETH_ALEN ; i++) {
		int temp;
		char *cp = strchr(arg, ':');
		if (cp) {
			*cp = 0;
			cp++;
		}
		if (sscanf(arg, "%x", &temp) != 1)
			return -1;
		if (temp < 0 || temp > 255)
			return -1;

		mac_addr[i] = temp;
		if (!cp)
			break;
		arg = cp;
	}
	if (i < ETH_ALEN - 1)
		return -1;

	return 0;
}

static const char *ifmodes[NL80211_IFTYPE_MAX + 1] = {
	"unspecified",
	"IBSS",
	"managed",
	"AP",
	"AP/VLAN",
	"WDS",
	"monitor",
	"mesh point"
};

static char modebuf[100];

const char *iftype_name(enum nl80211_iftype iftype)
{
	if (iftype <= NL80211_IFTYPE_MAX)
		return ifmodes[iftype];
	sprintf(modebuf, "Unknown mode (%d)", iftype);
	return modebuf;
}

static const char *commands[NL80211_CMD_MAX + 1] = {
	"unspecified",
	"get_wiphy",
	"set_wiphy",
	"new_wiphy",
	"del_wiphy",
	"get_interface",
	"set_interface",
	"new_interface",
	"del_interface",
	"get_key",
	"set_key",
	"new_key",
	"del_key",
	"get_beacon",
	"set_beacon",
	"new_beacon",
	"del_beacon",
	"get_station",
	"set_station",
	"new_station",
	"del_station",
	"get_mpath",
	"set_mpath",
	"new_mpath",
	"del_mpath",
	"set_bss",
	"set_reg",
	"reg_set_reg",
	"get_mesh_params",
	"set_mesh_params",
	"set_mgmt_extra_ie",
	"get_reg",
	"get_scan",
	"trigger_scan",
	"new_scan_results",
	"scan_aborted",
	"reg_change",
	"authenticate",
	"associate",
	"deauthenticate",
	"disassociate",
	"michael_mic_failure",
	"reg_beacon_hint",
	"join_ibss",
	"leave_ibss",
	"testmode",
	"connect",
	"roam",
	"disconnect",
	"set_wiphy_netns"
};

static char cmdbuf[100];

const char *command_name(enum nl80211_commands cmd)
{
	if (cmd <= NL80211_CMD_MAX)
		return commands[cmd];
	sprintf(cmdbuf, "Unknown command (%d)", cmd);
	return cmdbuf;
}

int ieee80211_channel_to_frequency(int chan)
{
	if (chan < 14)
		return 2407 + chan * 5;

	if (chan == 14)
		return 2484;

	/* FIXME: dot11ChannelStartingFactor (802.11-2007 17.3.8.3.2) */
	return (chan + 1000) * 5;
}

int ieee80211_frequency_to_channel(int freq)
{
	if (freq == 2484)
		return 14;

	if (freq < 2484)
		return (freq - 2407) / 5;

	/* FIXME: dot11ChannelStartingFactor (802.11-2007 17.3.8.3.2) */
	return freq/5 - 1000;
}

void print_ssid_escaped(const uint8_t len, const uint8_t *data)
{
	int i;

	for (i = 0; i < len; i++) {
		if (isprint(data[i]))
			printf("%c", data[i]);
		else
			printf("\\x%.2x", data[i]);
	}
}

static int hex2num(char digit)
{
	if (!isxdigit(digit))
		return -1;
	if (isdigit(digit))
		return digit - '0';
	return tolower(digit) - 'a' + 10;
}

static int hex2byte(char *hex)
{
	int d1, d2;

	d1 = hex2num(hex[0]);
	if (d1 < 0)
		return -1;
	d2 = hex2num(hex[1]);
	if (d2 < 0)
		return -1;
	return (d1 << 4) | d2;
}

static char *hex2bin(char *hex, char *buf)
{
	char *result = buf;
	int d;

	while (hex[0]) {
		d = hex2byte(hex);
		if (d < 0)
			return NULL;
		buf[0] = d;
		buf++;
		hex += 2;
	}

	return result;
}

int parse_keys(struct nl_msg *msg, char **argv, int argc)
{
	struct nlattr *keys;
	int i = 0;
	bool have_default = false;
	char keybuf[13];

	if (!argc)
		return 1;

	NLA_PUT_FLAG(msg, NL80211_ATTR_PRIVACY);

	keys = nla_nest_start(msg, NL80211_ATTR_KEYS);
	if (!keys)
		return -ENOBUFS;

	do {
		char *arg = *argv;
		int pos = 0, keylen;
		struct nlattr *key = nla_nest_start(msg, ++i);
		char *keydata;

		if (!key)
			return -ENOBUFS;

		if (arg[pos] == 'd') {
			NLA_PUT_FLAG(msg, NL80211_KEY_DEFAULT);
			pos++;
			if (arg[pos] == ':')
				pos++;
			have_default = true;
		}

		if (!isdigit(arg[pos]))
			goto explain;
		NLA_PUT_U8(msg, NL80211_KEY_IDX, arg[pos++] - '0');
		if (arg[pos++] != ':')
			goto explain;
		keydata = arg + pos;
		switch (strlen(keydata)) {
		case 10:
			keydata = hex2bin(keydata, keybuf);
		case 5:
			NLA_PUT_U32(msg, NL80211_KEY_CIPHER, 0x000FAC01);
			keylen = 5;
			break;
		case 26:
			keydata = hex2bin(keydata, keybuf);
		case 13:
			NLA_PUT_U32(msg, NL80211_KEY_CIPHER, 0x000FAC05);
			keylen = 13;
			break;
		default:
			goto explain;
		}

		if (!keydata)
			goto explain;

		NLA_PUT(msg, NL80211_KEY_DATA, keylen, keydata);

		argv++;
		argc--;

		/* one key should be TX key */
		if (!have_default && !argc)
			NLA_PUT_FLAG(msg, NL80211_KEY_DEFAULT);

		nla_nest_end(msg, key);
	} while (argc);

	nla_nest_end(msg, keys);

	return 0;
 nla_put_failure:
	return -ENOBUFS;
 explain:
	fprintf(stderr, "key must be [d:]index:data where\n"
			"  'd:'     means default (transmit) key\n"
			"  'index:' is a single digit (0-3)\n"
			"  'data'   must be 5 or 13 ascii chars\n"
			"           or 10 or 26 hex digits\n"
			"for example: d:2:6162636465 is the same as d:2:abcde\n");
	return 2;
}

static void print_mcs_index(const __u8 *mcs)
{
	unsigned int mcs_bit, prev_bit = -2, prev_cont = 0;

	for (mcs_bit = 0; mcs_bit <= 76; mcs_bit++) {
		unsigned int mcs_octet = mcs_bit/8;
		unsigned int MCS_RATE_BIT = 1 << mcs_bit % 8;
		bool mcs_rate_idx_set;

		mcs_rate_idx_set = !!(mcs[mcs_octet] & MCS_RATE_BIT);

		if (!mcs_rate_idx_set)
			continue;

		if (prev_bit != mcs_bit - 1) {
			if (prev_bit != -2)
				printf("%d, ", prev_bit);
			else
				printf(" ");
			printf("%d", mcs_bit);
			prev_cont = 0;
		} else if (!prev_cont) {
			printf("-");
			prev_cont = 1;
		}

		prev_bit = mcs_bit;
	}

	if (prev_cont)
		printf("%d", prev_bit);
	printf("\n");
}

/*
 * There are only 4 possible values, we just use a case instead of computing it,
 * but technically this can also be computed through the formula:
 *
 * Max AMPDU length = (2 ^ (13 + exponent)) - 1 bytes
 */
static __u32 compute_ampdu_length(__u8 exponent)
{
	switch (exponent) {
	case 0: return 8191;  /* (2 ^(13 + 0)) -1 */
	case 1: return 16383; /* (2 ^(13 + 1)) -1 */
	case 2: return 32767; /* (2 ^(13 + 2)) -1 */
	case 3: return 65535; /* (2 ^(13 + 3)) -1 */
	default: return 0;
	}
}

static const char *print_ampdu_space(__u8 space)
{
	switch (space) {
	case 0: return "No restriction";
	case 1: return "1/4 usec";
	case 2: return "1/2 usec";
	case 3: return "1 usec";
	case 4: return "2 usec";
	case 5: return "4 usec";
	case 6: return "8 usec";
	case 7: return "16 usec";
	default:
		return "BUG (spacing more than 3 bits!)";
	}
}

void print_ampdu_length(__u8 exponent)
{
	__u32 max_ampdu_length;

	max_ampdu_length = compute_ampdu_length(exponent);

	if (max_ampdu_length) {
		printf("\t\tMaximum RX AMPDU length %d bytes (exponent: 0x0%02x)\n",
		       max_ampdu_length, exponent);
        } else {
		printf("\t\tMaximum RX AMPDU length: unrecognized bytes "
		       "(exponent: %d)\n", exponent);
	}
}

void print_ampdu_spacing(__u8 spacing)
{
        printf("\t\tMinimum RX AMPDU time spacing: %s (0x%02x)\n",
               print_ampdu_space(spacing), spacing);
}

void print_ht_capability(__u16 cap)
{
#define PRINT_HT_CAP(_cond, _str) \
	do { \
		if (_cond) \
			printf("\t\t\t" _str "\n"); \
	} while (0)

	printf("\t\tCapabilities: 0x%02x\n", cap);

	PRINT_HT_CAP((cap & BIT(0)), "RX LDCP");
	PRINT_HT_CAP((cap & BIT(1)), "HT20/HT40");
	PRINT_HT_CAP(!(cap & BIT(1)), "HT20");

	PRINT_HT_CAP(((cap >> 2) & 0x3) == 0, "Static SM Power Save");
	PRINT_HT_CAP(((cap >> 2) & 0x3) == 1, "Dynamic SM Power Save");
	PRINT_HT_CAP(((cap >> 2) & 0x3) == 3, "SM Power Save disabled");

	PRINT_HT_CAP((cap & BIT(4)), "RX Greenfield");
	PRINT_HT_CAP((cap & BIT(5)), "RX HT20 SGI");
	PRINT_HT_CAP((cap & BIT(6)), "RX HT40 SGI");
	PRINT_HT_CAP((cap & BIT(7)), "TX STBC");

	PRINT_HT_CAP(((cap >> 8) & 0x3) == 0, "No RX STBC");
	PRINT_HT_CAP(((cap >> 8) & 0x3) == 1, "RX STBC 1-stream");
	PRINT_HT_CAP(((cap >> 8) & 0x3) == 2, "RX STBC 2-streams");
	PRINT_HT_CAP(((cap >> 8) & 0x3) == 3, "RX STBC 3-streams");

	PRINT_HT_CAP((cap & BIT(10)), "HT Delayed Block Ack");

	PRINT_HT_CAP((cap & BIT(11)), "Max AMSDU length: 3839 bytes");
        PRINT_HT_CAP(!(cap & BIT(11)), "Max AMSDU length: 7935 bytes");

	/*
	 * For beacons and probe response this would mean the BSS
	 * does or does not allow the usage of DSSS/CCK HT40.
	 * Otherwise it means the STA does or does not use
	 * DSSS/CCK HT40.
	 */
	PRINT_HT_CAP((cap & BIT(12)), "DSSS/CCK HT40");
	PRINT_HT_CAP(!(cap & BIT(12)), "No DSSS/CCK HT40");

	/* BIT(13) is reserved */

	PRINT_HT_CAP((cap & BIT(14)), "40 MHz Intolerant");

	PRINT_HT_CAP((cap & BIT(15)), "L-SIG TXOP protection");
#undef PRINT_HT_CAP
}

void print_ht_mcs(const __u8 *mcs)
{
	/* As defined in 7.3.2.57.4 Supported MCS Set field */
	unsigned int tx_max_num_spatial_streams, max_rx_supp_data_rate;
	bool tx_mcs_set_defined, tx_mcs_set_equal, tx_unequal_modulation;

	max_rx_supp_data_rate = ((mcs[10] >> 8) & ((mcs[11] & 0x3) << 8));
	tx_mcs_set_defined = !!(mcs[12] & (1 << 0));
	tx_mcs_set_equal = !(mcs[12] & (1 << 1));
	tx_max_num_spatial_streams = ((mcs[12] >> 2) & 3) + 1;
	tx_unequal_modulation = !!(mcs[12] & (1 << 4));

	if (max_rx_supp_data_rate)
		printf("\t\tHT Max RX data rate: %d Mbps\n", max_rx_supp_data_rate);
	/* XXX: else see 9.6.0e.5.3 how to get this I think */

	if (tx_mcs_set_defined) {
		if (tx_mcs_set_equal) {
			printf("\t\tHT TX/RX MCS rate indexes supported:");
			print_mcs_index(mcs);
		} else {
			printf("\t\tHT RX MCS rate indexes supported:");
			print_mcs_index(mcs);

			if (tx_unequal_modulation)
				printf("\t\tTX unequal modulation supported\n");
			else
				printf("\t\tTX unequal modulation not supported\n");

			printf("\t\tHT TX Max spatial streams: %d\n",
				tx_max_num_spatial_streams);

			printf("\t\tHT TX MCS rate indexes supported may differ\n");
		}
	} else {
		printf("\t\tHT RX MCS rate indexes supported:");
		print_mcs_index(mcs);
		printf("\t\tHT TX MCS rate indexes are undefined\n");
	}
}
