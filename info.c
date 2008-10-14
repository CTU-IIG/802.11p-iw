#include <errno.h>
#include <net/if.h>

#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <netlink/msg.h>
#include <netlink/attr.h>

#include "nl80211.h"
#include "iw.h"

static void print_flag(const char *name, int *open)
{
	if (!*open)
		printf(" (");
	else
		printf(", ");
	printf(name);
	*open = 1;
}

static int print_phy_handler(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb_msg[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));

	struct nlattr *tb_band[NL80211_BAND_ATTR_MAX + 1];

	struct nlattr *tb_freq[NL80211_FREQUENCY_ATTR_MAX + 1];
	static struct nla_policy freq_policy[NL80211_FREQUENCY_ATTR_MAX + 1] = {
		[NL80211_FREQUENCY_ATTR_FREQ] = { .type = NLA_U32 },
		[NL80211_FREQUENCY_ATTR_DISABLED] = { .type = NLA_FLAG },
		[NL80211_FREQUENCY_ATTR_PASSIVE_SCAN] = { .type = NLA_FLAG },
		[NL80211_FREQUENCY_ATTR_NO_IBSS] = { .type = NLA_FLAG },
		[NL80211_FREQUENCY_ATTR_RADAR] = { .type = NLA_FLAG },
	};

	struct nlattr *tb_rate[NL80211_BITRATE_ATTR_MAX + 1];
	static struct nla_policy rate_policy[NL80211_BITRATE_ATTR_MAX + 1] = {
		[NL80211_BITRATE_ATTR_RATE] = { .type = NLA_U32 },
		[NL80211_BITRATE_ATTR_2GHZ_SHORTPREAMBLE] = { .type = NLA_FLAG },
	};

	struct nlattr *nl_band;
	struct nlattr *nl_freq;
	struct nlattr *nl_rate;
	struct nlattr *nl_mode;
	int bandidx = 1;
	int rem_band, rem_freq, rem_rate, rem_mode;
	int open;

	nla_parse(tb_msg, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (!tb_msg[NL80211_ATTR_WIPHY_BANDS])
		return NL_SKIP;

	if (tb_msg[NL80211_ATTR_WIPHY_NAME])
		printf("Wiphy %s\n", nla_get_string(tb_msg[NL80211_ATTR_WIPHY_NAME]));

	nla_for_each_nested(nl_band, tb_msg[NL80211_ATTR_WIPHY_BANDS], rem_band) {
		printf("\tBand %d:\n", bandidx);
		bandidx++;

		nla_parse(tb_band, NL80211_BAND_ATTR_MAX, nla_data(nl_band),
			  nla_len(nl_band), NULL);

#ifdef NL80211_BAND_ATTR_HT_CAPA
		if (tb_band[NL80211_BAND_ATTR_HT_CAPA]) {
			unsigned short cap = nla_get_u16(tb_band[NL80211_BAND_ATTR_HT_CAPA]);
#define PCOM(fmt, args...) do { printf("\t\t\t * " fmt "\n", ##args); } while (0)
#define PBCOM(bit, args...) if (cap & (bit)) PCOM(args)
			printf("\t\tHT capabilities: 0x%.4x\n", cap);
			PBCOM(0x0001, "LPDC coding");
			if (cap & 0x0002)
				PCOM("20/40 MHz operation");
			else
				PCOM("20 MHz operation");
			switch ((cap & 0x000c) >> 2) {
			case 0:
				PCOM("static SM PS");
				break;
			case 1:
				PCOM("dynamic SM PS");
				break;
			case 2:
				PCOM("reserved SM PS");
				break;
			case 3:
				PCOM("SM PS disabled");
				break;
			}
			PBCOM(0x0010, "HT-greenfield");
			PBCOM(0x0020, "20 MHz short GI");
			PBCOM(0x0040, "40 MHz short GI");
			PBCOM(0x0080, "TX STBC");
			if (cap & 0x300)
				PCOM("RX STBC %d streams", (cap & 0x0300) >> 8);
			PBCOM(0x0400, "HT-delayed block-ack");
			PCOM("max A-MSDU len %d", 0xeff + ((cap & 0x0800) << 1));
			PBCOM(0x1000, "DSSS/CCK 40 MHz");
			PBCOM(0x2000, "PSMP support");
			PBCOM(0x4000, "40 MHz intolerant");
			PBCOM(0x8000, "L-SIG TXOP protection support");
		}
		if (tb_band[NL80211_BAND_ATTR_HT_AMPDU_FACTOR]) {
			unsigned char factor = nla_get_u8(tb_band[NL80211_BAND_ATTR_HT_AMPDU_FACTOR]);
			printf("\t\tHT A-MPDU factor: 0x%.4x (%d bytes)\n", factor, (1<<(13+factor))-1);
		}
		if (tb_band[NL80211_BAND_ATTR_HT_AMPDU_DENSITY]) {
			unsigned char dens = nla_get_u8(tb_band[NL80211_BAND_ATTR_HT_AMPDU_DENSITY]);
			printf("\t\tHT A-MPDU density: 0x%.4x (", dens);
			switch (dens) {
			case 0:
				printf("no restriction)\n");
				break;
			case 1:
				printf("1/4 usec)\n");
				break;
			case 2:
				printf("1/2 usec)\n");
				break;
			default:
				printf("%d usec)\n", 1<<(dens - 3));
			}
		}
		if (tb_band[NL80211_BAND_ATTR_HT_MCS_SET] &&
		    nla_len(tb_band[NL80211_BAND_ATTR_HT_MCS_SET]) == 16) {
			unsigned char *mcs = nla_data(tb_band[NL80211_BAND_ATTR_HT_MCS_SET]);
			printf("\t\tHT MCS set: %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x\n",
				mcs[0], mcs[1], mcs[2], mcs[3], mcs[4], mcs[5], mcs[6], mcs[7],
				mcs[8], mcs[9], mcs[10], mcs[11], mcs[12], mcs[13], mcs[14], mcs[15]);
		}
#endif

		printf("\t\tFrequencies:\n");

		nla_for_each_nested(nl_freq, tb_band[NL80211_BAND_ATTR_FREQS], rem_freq) {
			nla_parse(tb_freq, NL80211_FREQUENCY_ATTR_MAX, nla_data(nl_freq),
				  nla_len(nl_freq), freq_policy);
			if (!tb_freq[NL80211_FREQUENCY_ATTR_FREQ])
				continue;
			printf("\t\t\t* %d MHz", nla_get_u32(tb_freq[NL80211_FREQUENCY_ATTR_FREQ]));
			open = 0;
			if (tb_freq[NL80211_FREQUENCY_ATTR_DISABLED])
				print_flag("disabled", &open);
			if (tb_freq[NL80211_FREQUENCY_ATTR_PASSIVE_SCAN])
				print_flag("passive scanning", &open);
			if (tb_freq[NL80211_FREQUENCY_ATTR_NO_IBSS])
				print_flag("no IBSS", &open);
			if (tb_freq[NL80211_FREQUENCY_ATTR_RADAR])
				print_flag("radar detection", &open);
			if (open)
				printf(")");
			printf("\n");
		}

		printf("\t\tBitrates:\n");

		nla_for_each_nested(nl_rate, tb_band[NL80211_BAND_ATTR_RATES], rem_rate) {
			nla_parse(tb_rate, NL80211_BITRATE_ATTR_MAX, nla_data(nl_rate),
				  nla_len(nl_rate), rate_policy);
			if (!tb_rate[NL80211_BITRATE_ATTR_RATE])
				continue;
			printf("\t\t\t* %2.1f Mbps", 0.1 * nla_get_u32(tb_rate[NL80211_BITRATE_ATTR_RATE]));
			open = 0;
			if (tb_rate[NL80211_BITRATE_ATTR_2GHZ_SHORTPREAMBLE])
				print_flag("short preamble supported", &open);
			if (open)
				printf(")");
			printf("\n");
		}
	}

	if (!tb_msg[NL80211_ATTR_SUPPORTED_IFTYPES])
		return NL_SKIP;

	printf("\tSupported interface modes:\n");
	nla_for_each_nested(nl_mode, tb_msg[NL80211_ATTR_SUPPORTED_IFTYPES], rem_mode)
		printf("\t\t * %s\n", iftype_name(nl_mode->nla_type));

	return NL_SKIP;
}

static int handle_info(struct nl_cb *cb,
		       struct nl_msg *msg,
		       int argc, char **argv)
{
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, print_phy_handler, NULL);

	return 0;
}
TOPLEVEL(info, NULL, NL80211_CMD_GET_WIPHY, 0, CIB_PHY, handle_info);
TOPLEVEL(list, NULL, NL80211_CMD_GET_WIPHY, NLM_F_DUMP, CIB_NONE, handle_info);
