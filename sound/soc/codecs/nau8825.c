/*
 * NAU8825 audio codec driver
 *
 * Copyright 2015 Nuvoton Technology Corp.
 * Copyright 2015 Google Chromium project.
 *  Author: Anatol Pomozov <anatol@chromium.org>
 *
 * Licensed under the GPL-2.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/input.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/jack.h>

#include "nau8825.h"

struct nau8825 {
	struct device *dev;
	struct regmap *regmap;
	struct snd_soc_dapm_context *dapm;
	int irq;
	struct snd_soc_jack *jack;
	int button_pressed;
};

static const struct reg_default nau8825_reg_defaults[] = {
	{ NAU8825_REG_ENA_CTRL, 0x00ff },
	{ NAU8825_REG_CLK_DIVIDER, 0x0050 },
	{ NAU8825_REG_FLL1, 0x0 },
	{ NAU8825_REG_FLL2, 0x3126 },
	{ NAU8825_REG_FLL3, 0x0008 },
	{ NAU8825_REG_FLL4, 0x0010 },
	{ NAU8825_REG_FLL5, 0x0 },
	{ NAU8825_REG_FLL6, 0x6000 },
	{ NAU8825_REG_FLL_VCO_RSV, 0xf13c },
	{ NAU8825_REG_HSD_CTRL, 0x000c },
	{ NAU8825_REG_JACK_DET_CTRL, 0x0 },
	{ NAU8825_REG_INTERRUPT_MASK, 0x0 },
	{ NAU8825_REG_INTERRUPT_DIS_CTRL, 0xffff },
	{ NAU8825_REG_SAR_CTRL, 0x0015 },
	{ NAU8825_REG_KEYDET_CTRL, 0x0110 },
	{ NAU8825_REG_VDET_THRESHOLD_1, 0x0 },
	{ NAU8825_REG_VDET_THRESHOLD_2, 0x0 },
	{ NAU8825_REG_VDET_THRESHOLD_3, 0x0 },
	{ NAU8825_REG_VDET_THRESHOLD_4, 0x0 },
	{ NAU8825_REG_GPIO34_CTRL, 0x0 },
	{ NAU8825_REG_GPIO12_CTRL, 0x0 },
	{ NAU8825_REG_TDM_CTRL, 0x0 },
	{ NAU8825_REG_I2S_PCM_CTRL1, 0x000b },
	{ NAU8825_REG_I2S_PCM_CTRL2, 0x8010 },
	{ NAU8825_REG_LEFT_TIME_SLOT, 0x0 },
	{ NAU8825_REG_RIGHT_TIME_SLOT, 0x0 },
	{ NAU8825_REG_BIQ_CTRL, 0x0 },
	{ NAU8825_REG_BIQ_COF1, 0x0 },
	{ NAU8825_REG_BIQ_COF2, 0x0 },
	{ NAU8825_REG_BIQ_COF3, 0x0 },
	{ NAU8825_REG_BIQ_COF4, 0x0 },
	{ NAU8825_REG_BIQ_COF5, 0x0 },
	{ NAU8825_REG_BIQ_COF6, 0x0 },
	{ NAU8825_REG_BIQ_COF7, 0x0 },
	{ NAU8825_REG_BIQ_COF8, 0x0 },
	{ NAU8825_REG_BIQ_COF9, 0x0 },
	{ NAU8825_REG_BIQ_COF10, 0x0 },
	{ NAU8825_REG_ADC_RATE, 0x0010 },
	{ NAU8825_REG_DAC_CTRL1, 0x0001 },
	{ NAU8825_REG_DAC_CTRL2, 0x0 },
	{ NAU8825_REG_DAC_DGAIN_CTRL, 0x0 },
	{ NAU8825_REG_ADC_DGAIN_CTRL, 0x00cf },
	{ NAU8825_REG_MUTE_CTRL, 0x0 },
	{ NAU8825_REG_HSVOL_CTRL, 0x0 },
	{ NAU8825_REG_DACL_CTRL, 0x02cf },
	{ NAU8825_REG_DACR_CTRL, 0x00cf },
	{ NAU8825_REG_ADC_DRC_KNEE_IP12, 0x1486 },
	{ NAU8825_REG_ADC_DRC_KNEE_IP34, 0x0f12 },
	{ NAU8825_REG_ADC_DRC_SLOPES, 0x25ff },
	{ NAU8825_REG_ADC_DRC_ATKDCY, 0x3457 },
	{ NAU8825_REG_DAC_DRC_KNEE_IP12, 0x1486 },
	{ NAU8825_REG_DAC_DRC_KNEE_IP34, 0x0f12 },
	{ NAU8825_REG_DAC_DRC_SLOPES, 0x25f9 },
	{ NAU8825_REG_DAC_DRC_ATKDCY, 0x3457 },
	{ NAU8825_REG_IMM_MODE_CTRL, 0x0 },
	{ NAU8825_REG_CLASSG_CTRL, 0x0 },
	{ NAU8825_REG_OPT_EFUSE_CTRL, 0x0 },
	{ NAU8825_REG_MISC_CTRL, 0x0 },
	{ NAU8825_REG_BIAS_ADJ, 0x0 },
	{ NAU8825_REG_TRIM_SETTINGS, 0x0 },
	{ NAU8825_REG_ANALOG_CONTROL_1, 0x0 },
	{ NAU8825_REG_ANALOG_CONTROL_2, 0x0 },
	{ NAU8825_REG_ANALOG_ADC_1, 0x0011 },
	{ NAU8825_REG_ANALOG_ADC_2, 0x0020 },
	{ NAU8825_REG_RDAC, 0x0008 },
	{ NAU8825_REG_MIC_BIAS, 0x0006 },
	{ NAU8825_REG_BOOST, 0x0 },
	{ NAU8825_REG_FEPGA, 0x0 },
	{ NAU8825_REG_POWER_UP_CONTROL, 0x0 },
	{ NAU8825_REG_CHARGE_PUMP, 0x0 },
};

static bool nau8825_readable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case NAU8825_REG_ENA_CTRL:
	case NAU8825_REG_CLK_DIVIDER ... NAU8825_REG_FLL_VCO_RSV:
	case NAU8825_REG_HSD_CTRL ... NAU8825_REG_JACK_DET_CTRL:
	case NAU8825_REG_INTERRUPT_MASK ... NAU8825_REG_KEYDET_CTRL:
	case NAU8825_REG_VDET_THRESHOLD_1 ... NAU8825_REG_DACR_CTRL:
	case NAU8825_REG_ADC_DRC_KNEE_IP12 ... NAU8825_REG_ADC_DRC_ATKDCY:
	case NAU8825_REG_DAC_DRC_KNEE_IP12 ... NAU8825_REG_DAC_DRC_ATKDCY:
	case NAU8825_REG_IMM_MODE_CTRL ... NAU8825_REG_IMM_RMS_R:
	case NAU8825_REG_CLASSG_CTRL ... NAU8825_REG_OPT_EFUSE_CTRL:
	case NAU8825_REG_MISC_CTRL:
	case NAU8825_REG_I2C_DEVICE_ID ... NAU8825_REG_SARDOUT_RAM_STATUS:
	case NAU8825_REG_BIAS_ADJ:
	case NAU8825_REG_TRIM_SETTINGS ... NAU8825_REG_ANALOG_CONTROL_2:
	case NAU8825_REG_ANALOG_ADC_1 ... NAU8825_REG_MIC_BIAS:
	case NAU8825_REG_BOOST ... NAU8825_REG_FEPGA:
	case NAU8825_REG_POWER_UP_CONTROL ... NAU8825_REG_GENERAL_STATUS:
		return true;
	default:
		return false;
	}

}

static bool nau8825_writeable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case NAU8825_REG_RESET ... NAU8825_REG_ENA_CTRL:
	case NAU8825_REG_CLK_DIVIDER ... NAU8825_REG_FLL_VCO_RSV:
	case NAU8825_REG_HSD_CTRL ... NAU8825_REG_JACK_DET_CTRL:
	case NAU8825_REG_INTERRUPT_MASK:
	case NAU8825_REG_INT_CLR_KEY_STATUS ... NAU8825_REG_KEYDET_CTRL:
	case NAU8825_REG_VDET_THRESHOLD_1 ... NAU8825_REG_DACR_CTRL:
	case NAU8825_REG_ADC_DRC_KNEE_IP12 ... NAU8825_REG_ADC_DRC_ATKDCY:
	case NAU8825_REG_DAC_DRC_KNEE_IP12 ... NAU8825_REG_DAC_DRC_ATKDCY:
	case NAU8825_REG_IMM_MODE_CTRL:
	case NAU8825_REG_CLASSG_CTRL ... NAU8825_REG_OPT_EFUSE_CTRL:
	case NAU8825_REG_MISC_CTRL:
	case NAU8825_REG_BIAS_ADJ:
	case NAU8825_REG_TRIM_SETTINGS ... NAU8825_REG_ANALOG_CONTROL_2:
	case NAU8825_REG_ANALOG_ADC_1 ... NAU8825_REG_MIC_BIAS:
	case NAU8825_REG_BOOST ... NAU8825_REG_FEPGA:
	case NAU8825_REG_POWER_UP_CONTROL ... NAU8825_REG_CHARGE_PUMP:
		return true;
	default:
		return false;
	}
}

static bool nau8825_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case NAU8825_REG_RESET:
	case NAU8825_REG_IRQ_STATUS:
	case NAU8825_REG_INT_CLR_KEY_STATUS:
	case NAU8825_REG_IMM_RMS_L:
	case NAU8825_REG_IMM_RMS_R:
	case NAU8825_REG_I2C_DEVICE_ID:
	case NAU8825_REG_SARDOUT_RAM_STATUS:
	case NAU8825_REG_CHARGE_PUMP_INPUT_READ:
	case NAU8825_REG_GENERAL_STATUS:
		return true;
	default:
		return false;
	}
}

static const DECLARE_TLV_DB_MINMAX_MUTE(adc_vol_tlv, -10300, 2400);
static const DECLARE_TLV_DB_MINMAX_MUTE(sidetone_vol_tlv, -4200, 0);
static const DECLARE_TLV_DB_MINMAX(dac_vol_tlv, -5400, 0);
static const DECLARE_TLV_DB_MINMAX(fepga_gain_tlv, -100, 3600);

static const struct snd_kcontrol_new nau8825_snd_controls[] = {
	SOC_SINGLE_TLV("MIC Volume", NAU8825_REG_ADC_DGAIN_CTRL,
		0, 0xff, 0, adc_vol_tlv),
	SOC_DOUBLE_TLV("HP Sidetone Volume", NAU8825_REG_ADC_DGAIN_CTRL,
		12, 8, 0x0f, 0, sidetone_vol_tlv),
	SOC_DOUBLE_TLV("HP Volume", NAU8825_REG_HSVOL_CTRL,
		6, 0, 0x3f, 1, dac_vol_tlv),
	SOC_SINGLE_TLV("Frontend PGA Gain", NAU8825_REG_POWER_UP_CONTROL,
		8, 37, 0, fepga_gain_tlv),
};

static int nau8825_charge_pump_event(struct snd_soc_dapm_widget *w,
        struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct nau8825 *nau8825 = snd_soc_codec_get_drvdata(codec);
	struct regmap *regmap = nau8825->regmap;

	if (SND_SOC_DAPM_EVENT_ON(event)) {
		regmap_update_bits(regmap, NAU8825_REG_CHARGE_PUMP,
			NAU8825_JAMNODCLOW, NAU8825_JAMNODCLOW);
	} else {
		regmap_update_bits(regmap, NAU8825_REG_CHARGE_PUMP,
			NAU8825_JAMNODCLOW, 0);
	}

	return 0;
}

static int nau8825_output_driver_event(struct snd_soc_dapm_widget *w,
        struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct nau8825 *nau8825 = snd_soc_codec_get_drvdata(codec);
	struct regmap *regmap = nau8825->regmap;

	if (SND_SOC_DAPM_EVENT_ON(event)) {
		/* Power Up L and R Output Drivers */
		regmap_update_bits(regmap, NAU8825_REG_POWER_UP_CONTROL, 0x3c, 0x3c);
		/* Power up Main Output Drivers (The main driver must be turned on after
		   the predriver to avoid pops) */
		regmap_update_bits(regmap, NAU8825_REG_POWER_UP_CONTROL, 0x3, 0x3);
	} else {
		/* Power Down L and R Output Drivers */
		regmap_update_bits(regmap, NAU8825_REG_POWER_UP_CONTROL, 0x3f, 0x0);
	}

	return 0;
}

/* DAC Mux 0x33[9] and 0x34[9] */
static const char *nau8825_dac_src[] = {
	"DACL", "DACR",
};

static SOC_ENUM_SINGLE_DECL(
	nau8825_dacl_enum, NAU8825_REG_DACL_CTRL,
	NAU8825_DACL_CH_SEL_SFT, nau8825_dac_src);

static SOC_ENUM_SINGLE_DECL(
	nau8825_dacr_enum, NAU8825_REG_DACR_CTRL,
	NAU8825_DACR_CH_SEL_SFT, nau8825_dac_src);

static const struct snd_kcontrol_new nau8825_dacl_mux =
	SOC_DAPM_ENUM("DACL Source", nau8825_dacl_enum);

static const struct snd_kcontrol_new nau8825_dacr_mux =
	SOC_DAPM_ENUM("DACR Source", nau8825_dacr_enum);

static const struct snd_soc_dapm_widget nau8825_dapm_widgets[] = {
	SND_SOC_DAPM_INPUT("MIC"),
	SND_SOC_DAPM_MICBIAS("MICBIAS", NAU8825_REG_MIC_BIAS, 8, 0),

	SND_SOC_DAPM_PGA("Frontend PGA", NAU8825_REG_POWER_UP_CONTROL, 14, 0, NULL,
		0),

	SND_SOC_DAPM_ADC("ADC", NULL, NAU8825_REG_ENA_CTRL, 8, 0),
	SND_SOC_DAPM_SUPPLY("ADC Clock", NAU8825_REG_ENA_CTRL, 7, 0, NULL, 0),

	/* ADC for button press detection */
	SND_SOC_DAPM_ADC("SAR", NULL, NAU8825_REG_SAR_CTRL, NAU8825_SAR_ADC_EN_SFT,
		0),

	SND_SOC_DAPM_DAC("ADACL", NULL, NAU8825_REG_RDAC, 12, 0),
	SND_SOC_DAPM_DAC("ADACR", NULL, NAU8825_REG_RDAC, 13, 0),
	SND_SOC_DAPM_SUPPLY("ADACL Clock", NAU8825_REG_RDAC, 8, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("ADACR Clock", NAU8825_REG_RDAC, 9, 0, NULL, 0),

	SND_SOC_DAPM_DAC("DDACR", NULL, NAU8825_REG_ENA_CTRL,
		NAU8825_ENABLE_DACR_SFT, 0),
	SND_SOC_DAPM_DAC("DDACL", NULL, NAU8825_REG_ENA_CTRL,
		NAU8825_ENABLE_DACL_SFT, 0),
	SND_SOC_DAPM_SUPPLY("DDAC Clock", NAU8825_REG_ENA_CTRL, 6, 0, NULL, 0),

	SND_SOC_DAPM_MUX("DACL Mux", SND_SOC_NOPM, 0, 0,
			&nau8825_dacl_mux),
	SND_SOC_DAPM_MUX("DACR Mux", SND_SOC_NOPM, 0, 0,
			&nau8825_dacr_mux),

	SND_SOC_DAPM_PGA("HP amp L", NAU8825_REG_CLASSG_CTRL, 1, 0, NULL, 0),
	SND_SOC_DAPM_PGA("HP amp R", NAU8825_REG_CLASSG_CTRL, 2, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("HP amp power", NAU8825_REG_CLASSG_CTRL, 0, 0, NULL, 0),

	SND_SOC_DAPM_SUPPLY("Change Pump", NAU8825_REG_CHARGE_PUMP, 5, 0,
		nau8825_charge_pump_event, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	// Datasheet says 'power down disable'. What exactly block is powers down? Ask Nuvoton
	SND_SOC_DAPM_SUPPLY("DACL Power", NAU8825_REG_CHARGE_PUMP, 8, 1, NULL, 0),
	SND_SOC_DAPM_SUPPLY("DACR Power", NAU8825_REG_CHARGE_PUMP, 9, 1, NULL, 0),

	// Ask upstream: this widget manages 6 power widgets for different drivers.
	// Or it is better to have 6 separate widgets?
	SND_SOC_DAPM_OUT_DRV_E("Output Driver", SND_SOC_NOPM, 0, 0, NULL, 0,
		nau8825_output_driver_event, SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_OUTPUT("HPOL"),
	SND_SOC_DAPM_OUTPUT("HPOR"),

	// add mixer Reg0x33[14:15]
};

static const struct snd_soc_dapm_route nau8825_dapm_routes[] = {
	{"Frontend PGA", NULL, "MIC"},
	{"ADC", NULL, "Frontend PGA"},
	{"ADC", NULL, "ADC Clock"},
	{"Capture", NULL, "ADC"},

	{"DDACL", NULL, "Playback"},
	{"DDACR", NULL, "Playback"},
	{"DDACL", NULL, "DDAC Clock"},
	{"DDACR", NULL, "DDAC Clock"},
	{"DDACL", NULL, "DACL Power"},
	{"DDACR", NULL, "DACR Power"},
	{"DACL Mux", "DACL", "DDACL"},
	{"DACL Mux", "DACR", "DDACR"},
	{"DACR Mux", "DACL", "DDACL"},
	{"DACR Mux", "DACR", "DDACR"},
	{"HP amp L", NULL, "Change Pump"},
	{"HP amp R", NULL, "Change Pump"},
	{"HP amp L", NULL, "DACL Mux"},
	{"HP amp R", NULL, "DACR Mux"},
	{"HP amp L", NULL, "HP amp power"},
	{"HP amp R", NULL, "HP amp power"},
	{"ADACL", NULL, "HP amp L"},
	{"ADACR", NULL, "HP amp R"},
	{"ADACL", NULL, "ADACL Clock"},
	{"ADACR", NULL, "ADACR Clock"},
	{"Output Driver", NULL, "ADACL"},
	{"Output Driver", NULL, "ADACR"},
	{"HPOL", NULL, "Output Driver"},
	{"HPOR", NULL, "Output Driver"},
};

static int nau8825_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct nau8825 *nau8825 = snd_soc_codec_get_drvdata(codec);
	unsigned int val_len = 0;

	switch (params_width(params)) {
	case 16:
		val_len |= NAU8825_I2S_DL_16;
		break;
	case 20:
		val_len |= NAU8825_I2S_DL_20;
		break;
	case 24:
		val_len |= NAU8825_I2S_DL_24;
		break;
	case 32:
		val_len |= NAU8825_I2S_DL_32;
		break;
	default:
		return -EINVAL;
	}
	regmap_update_bits(nau8825->regmap, NAU8825_REG_I2S_PCM_CTRL1,
		NAU8825_I2S_DL_MASK, val_len);

	return 0;
}

static int nau8825_mute(struct snd_soc_dai *codec_dai, int mute)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct nau8825 *nau8825 = snd_soc_codec_get_drvdata(codec);
	int reg_val;

	reg_val = mute ? NAU8825_HP_MUTE : 0;
	regmap_update_bits(nau8825->regmap, NAU8825_REG_HSVOL_CTRL,
		NAU8825_HP_MUTE, reg_val);

	return 0;
}

static int nau8825_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct nau8825 *nau8825 = snd_soc_codec_get_drvdata(codec);
	unsigned int ctrl1_val = 0, ctrl2_val = 0;

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		ctrl2_val |= NAU8825_I2S_MS_MASTER;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_IB_NF:
		ctrl1_val |= NAU8825_I2S_BP_INV;
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		ctrl1_val |= NAU8825_I2S_DF_I2S;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		ctrl1_val |= NAU8825_I2S_DF_LEFT;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		ctrl1_val |= NAU8825_I2S_DF_RIGTH;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		ctrl1_val |= NAU8825_I2S_DF_PCM_AB;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		ctrl1_val |= NAU8825_I2S_DF_PCM_AB;
		ctrl1_val |= NAU8825_I2S_PCMB_EN;
		break;
	default:
		return -EINVAL;
	}

	regmap_update_bits(nau8825->regmap, NAU8825_REG_I2S_PCM_CTRL1,
		NAU8825_I2S_DL_MASK | NAU8825_I2S_DF_MASK |
		NAU8825_I2S_BP_MASK | NAU8825_I2S_PCMB_MASK,
		ctrl1_val);
	regmap_update_bits(nau8825->regmap, NAU8825_REG_I2S_PCM_CTRL2,
		NAU8825_I2S_MS_MASK, ctrl2_val);

	return 0;
}

static const struct snd_soc_dai_ops nau8825_dai_ops = {
	.hw_params	= nau8825_hw_params,
	.digital_mute	= nau8825_mute,
	.set_fmt	= nau8825_set_dai_fmt,
};

#define NAU8825_RATES	SNDRV_PCM_RATE_8000_192000
#define NAU8825_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE \
			 | SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver nau8825_dai = {
	.name = "nau8825-hifi",
	.playback = {
		.stream_name	 = "Playback",
		.channels_min	 = 1,
		.channels_max	 = 2,
		.rates		 = NAU8825_RATES,
		.formats	 = NAU8825_FORMATS,
	},
	.capture = {
		.stream_name	 = "Capture",
		.channels_min	 = 1,
		.channels_max	 = 1,
		.rates		 = NAU8825_RATES,
		.formats	 = NAU8825_FORMATS,
	},
	.ops = &nau8825_dai_ops,
};

/**
 * nau8825_enable_jack_detect - Specify a jack for event reporting
 *
 * @component:  component to register the jack with
 * @jack: jack to use to report headset and button events on
 *
 * After this function has been called the headset insert/remove and button
 * events will be routed to the given jack.  Jack can be null to stop
 * reporting.
 */
int nau8825_enable_jack_detect(struct snd_soc_codec *codec,
				struct snd_soc_jack *jack)
{
	struct nau8825 *nau8825 = snd_soc_codec_get_drvdata(codec);

	snd_jack_set_key(jack->jack, SND_JACK_BTN_0, KEY_MEDIA);
	snd_jack_set_key(jack->jack, SND_JACK_BTN_1, KEY_VOICECOMMAND);
	snd_jack_set_key(jack->jack, SND_JACK_BTN_2, KEY_VOLUMEUP);
	snd_jack_set_key(jack->jack, SND_JACK_BTN_3, KEY_VOLUMEDOWN);

	nau8825->jack = jack;

	return 0;
}
EXPORT_SYMBOL_GPL(nau8825_enable_jack_detect);


static bool nau8825_is_jack_inserted(struct regmap *regmap)
{
	int status;

	regmap_read(regmap, NAU8825_REG_I2C_DEVICE_ID, &status);
	return !(status & NAU8825_GPIO2JD1);
}

static void nau8825_restart_jack_detection(struct regmap *regmap)
{
	/* this will restart the entire jack detection process including MIC/GND
	 * switching and create interrupts. We have to go from 0 to 1 and back
	 * to 0 to restart.
	 */
	regmap_update_bits(regmap, NAU8825_REG_JACK_DET_CTRL,
		NAU8825_JACK_DET_RESTART, NAU8825_JACK_DET_RESTART);
	regmap_update_bits(regmap, NAU8825_REG_JACK_DET_CTRL,
		NAU8825_JACK_DET_RESTART, 0);
}

static void nau8825_eject_jack(struct nau8825 *nau8825)
{
	struct snd_soc_dapm_context *dapm = nau8825->dapm;
	struct regmap *regmap = nau8825->regmap;

	snd_soc_dapm_disable_pin(dapm, "SAR");
	snd_soc_dapm_disable_pin(dapm, "MICBIAS");
	/* Detach 2kOhm Resistors from MICBIAS to MICGND1/2 */
	regmap_update_bits(regmap, NAU8825_REG_MIC_BIAS,
		NAU8825_MICBIAS_JKSLV | NAU8825_MICBIAS_JKR2, 0);
	/* ground HPL/HPR, MICGRND1/2 */
	regmap_update_bits(regmap, NAU8825_REG_HSD_CTRL, 0xf, 0xf);

	snd_soc_dapm_sync(dapm);
}

static int nau8825_button_decode(int value)
{
	int buttons;

	if (value & BIT(0))
		buttons |= SND_JACK_BTN_0;
	if (value & BIT(1))
		buttons |= SND_JACK_BTN_1;
	if (value & BIT(2))
		buttons |= SND_JACK_BTN_2;
	if (value & BIT(3))
		buttons |= SND_JACK_BTN_3;

	return buttons;
}

static int nau8825_jack_insert(struct nau8825 *nau8825)
{
	struct regmap *regmap = nau8825->regmap;
	struct snd_soc_dapm_context *dapm = nau8825->dapm;
	int jack_status_reg, mic_detected;
	int type;

	regmap_read(regmap, NAU8825_REG_GENERAL_STATUS, &jack_status_reg);
	mic_detected = (jack_status_reg >> 10) & 3;

	switch(mic_detected) {
	case 0:
		/* no mic */
		type = SND_JACK_HEADPHONE;
		break;
	case 1:
		dev_info(nau8825->dev, "OMTP (micgnd1) mic connected\n");
		type = SND_JACK_HEADSET;

		/* Unground MICGND1 */
		regmap_update_bits(regmap, NAU8825_REG_HSD_CTRL, 3 << 2, 1 << 2);
		/* Unground HPL/R */
		regmap_update_bits(regmap, NAU8825_REG_HSD_CTRL, 0x3, 0);
		/* Attach 2kOhm Resistor from MICBIAS to MICGND1 */
		regmap_update_bits(regmap, NAU8825_REG_MIC_BIAS,
			NAU8825_MICBIAS_JKSLV | NAU8825_MICBIAS_JKR2,
			NAU8825_MICBIAS_JKR2);
		/* Attach SARADC to MICGND1 */
		regmap_update_bits(regmap, NAU8825_REG_SAR_CTRL,
			NAU8825_SAR_INPUT_MASK,
			NAU8825_SAR_INPUT_JKR2);

		snd_soc_dapm_force_enable_pin(dapm, "MICBIAS");
		snd_soc_dapm_force_enable_pin(dapm, "SAR");
		snd_soc_dapm_sync(dapm);
		break;
	case 2:
	case 3:
		dev_info(nau8825->dev, "CTIA (micgnd2) mic connected\n");
		type = SND_JACK_HEADSET;

		/* Unground MICGND2 */
		regmap_update_bits(regmap, NAU8825_REG_HSD_CTRL, 3 << 2, 2 << 2);
		/* Unground HPL/R */
		regmap_update_bits(regmap, NAU8825_REG_HSD_CTRL, 0x3, 0);
		/* Attach 2kOhm Resistor from MICBIAS to MICGND2 */
		regmap_update_bits(regmap, NAU8825_REG_MIC_BIAS,
			NAU8825_MICBIAS_JKSLV | NAU8825_MICBIAS_JKR2,
			NAU8825_MICBIAS_JKSLV);
		/* Attach SARADC to MICGND2 */
		regmap_update_bits(regmap, NAU8825_REG_SAR_CTRL,
			NAU8825_SAR_INPUT_MASK,
			NAU8825_SAR_INPUT_JKSLV);

		snd_soc_dapm_force_enable_pin(dapm, "MICBIAS");
		snd_soc_dapm_force_enable_pin(dapm, "SAR");
		snd_soc_dapm_sync(dapm);
		break;
	}

	return type;
}

#define NAU8825_BUTTONS (SND_JACK_BTN_0 | SND_JACK_BTN_1 | \
                         SND_JACK_BTN_2 | SND_JACK_BTN_3)

static irqreturn_t nau8825_interrupt(int irq, void *data)
{
	struct nau8825 *nau8825 = (struct nau8825 *)data;
	struct regmap *regmap = nau8825->regmap;
	int active_irq, clear_irq = 0, event = 0, event_mask = 0;

	regmap_read(regmap, NAU8825_REG_IRQ_STATUS, &active_irq);

	if ((active_irq & NAU8825_JACK_EJECTION_IRQ_MASK) ==
		NAU8825_JACK_EJECTION_DETECTED) {

		nau8825_eject_jack(nau8825);
		event_mask |= SND_JACK_HEADSET;
		clear_irq = NAU8825_JACK_EJECTION_IRQ_MASK;
	} else if (active_irq & NAU8825_KEY_SHORT_PRESS_IRQ) {
		int key_status;

		regmap_read(regmap, NAU8825_REG_INT_CLR_KEY_STATUS, &key_status);

		/* upper 8 bits of the register are for short pressed keys,
		   lower 8 bits - for long pressed buttons */
		nau8825->button_pressed = nau8825_button_decode(key_status >> 8);

		event |= nau8825->button_pressed;
		event_mask |= NAU8825_BUTTONS;
		clear_irq = NAU8825_KEY_SHORT_PRESS_IRQ;
	} else if (active_irq & NAU8825_KEY_RELEASE_IRQ) {
		event_mask = NAU8825_BUTTONS;
		clear_irq = NAU8825_KEY_RELEASE_IRQ;
	} else if (active_irq & NAU8825_HEADSET_COMPLETION_IRQ) {
		if (nau8825_is_jack_inserted(regmap)) {
			event |= nau8825_jack_insert(nau8825);
		} else {
			dev_warn(nau8825->dev, "Headset completion IRQ fired but no headset connected\n");
			nau8825_eject_jack(nau8825);
		}

		event_mask |= SND_JACK_HEADSET;
		clear_irq = NAU8825_HEADSET_COMPLETION_IRQ;
	}

	if (!clear_irq)
		clear_irq = active_irq;
	/* clears the rightmost interruption */
	regmap_write(regmap, NAU8825_REG_INT_CLR_KEY_STATUS, clear_irq);

	if (event_mask)
		snd_soc_jack_report(nau8825->jack, event, event_mask);

	return IRQ_HANDLED;
}

static void nau8825_setup_buttons(struct regmap *regmap)
{
	/* Setup Button Detect (Debounce, number of buttons, and Hysteresis) */
	regmap_write(regmap, NAU8825_REG_KEYDET_CTRL, 0x7311);

	/* Setup 4 buttons impedane according to Android specification
	 * https://source.android.com/accessories/headset-spec.html
	 * Button 0 - 0-70 Ohm
	 * Button 1 - 110-180 Ohm
	 * Button 2 - 210-290 Ohm
	 * Button 3 - 360-680 Ohm
	 */
	regmap_write(regmap, NAU8825_REG_VDET_THRESHOLD_1, 0x0f1f);
	regmap_write(regmap, NAU8825_REG_VDET_THRESHOLD_2, 0x325f);
}

static void nau8825_init_regs(struct regmap *regmap)
{
	/* Power down DACs for power savings */
	regmap_write(regmap, NAU8825_REG_CHARGE_PUMP, 0x0300);
	/* DAC Zero Crossing Enable */
	regmap_write(regmap, NAU8825_REG_MUTE_CTRL, 0x1000);
	/* VMID Enable and Tieoff */
	regmap_write(regmap, NAU8825_REG_BIAS_ADJ, 0x0060);
	/* Analog Bias Enable, Disable Boost Driver,
	   Automatic Short circuit protection enable */
	regmap_write(regmap, NAU8825_REG_BOOST, 0x3140);
	/* Ground HP Outputs[1:0],
	   Enable Automatic Mic/Gnd switching reading on insert interrupt[6] */
	regmap_write(regmap, NAU8825_REG_HSD_CTRL, 0x004f);

	regmap_write(regmap, NAU8825_REG_JACK_DET_CTRL, 0x01e0);
	/* IRQ Output Enable, Mask Insert Interrupt */
	regmap_write(regmap, NAU8825_REG_INTERRUPT_MASK, 0x0801);
	/* Jack Detect pull up (High=eject, Low=insert) */
	regmap_write(regmap, NAU8825_REG_GPIO12_CTRL, 0x0800);
	/* Setup SAR ADC */
	regmap_write(regmap, NAU8825_REG_SAR_CTRL, 0x0280);
	nau8825_setup_buttons(regmap);

	/* Setup ADC x128 OSR */
	regmap_write(regmap, NAU8825_REG_ADC_RATE, 0x0002);
	/* Setup DAC x128 OSR */
	regmap_write(regmap, NAU8825_REG_DAC_CTRL1, 0x0082);
	/* DAC bias settings */
	regmap_write(regmap, NAU8825_REG_ANALOG_CONTROL_2, 0x1003);
	/* ADC Bias Settings */
	regmap_write(regmap, NAU8825_REG_ANALOG_ADC_2, 0x0260);
	/* Maximize Jack Insert Debounce */
	regmap_write(regmap, NAU8825_REG_JACK_DET_CTRL, 0x00e0);

	/* Setup clocks */
	regmap_write(regmap, NAU8825_REG_ENA_CTRL, 0x0416);

	/* chip needs one FSCLK cycle in order to generate interrupts,
	   as we cannot guarantee one will be provided by the system turning
	   master mode on then off enables us to generate that FSCLK cycle
	   with a minimum of contention on the clock bus.
	   Enables master mode and correctly divides BCLK and FSCLK from MCLK.
	*/
	regmap_write(regmap, NAU8825_REG_I2S_PCM_CTRL2, 0x2019);
	regmap_write(regmap, NAU8825_REG_I2S_PCM_CTRL2, 0x0010);

	/* Mask jack insert interruption */
	regmap_write(regmap, NAU8825_REG_INTERRUPT_DIS_CTRL, 0x0011);
}

static const struct regmap_config nau8825_regmap_config = {
	.val_bits = 16,
	.reg_bits = 16,

	.max_register = NAU8825_REG_MAX,
	.readable_reg = nau8825_readable_reg,
	.writeable_reg = nau8825_writeable_reg,
	.volatile_reg = nau8825_volatile_reg,

	.cache_type = REGCACHE_RBTREE,
	.reg_defaults = nau8825_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(nau8825_reg_defaults),
};

static int nau8825_codec_probe(struct snd_soc_codec *codec)
{
	struct nau8825 *nau8825 = snd_soc_codec_get_drvdata(codec);
	struct device *dev = codec->dev;

	nau8825->dapm = &codec->dapm;

	nau8825_init_regs(nau8825->regmap);
	/* The interrupt clock is gated by x1[10:8],
	 * one of them needs to be enabled all time for
	 * interrupts to happen. */
	snd_soc_dapm_force_enable_pin(&codec->dapm, "DDACR");
	snd_soc_dapm_sync(&codec->dapm);

	if (nau8825->irq) {
		int ret = devm_request_threaded_irq(dev, nau8825->irq, NULL,
			nau8825_interrupt, IRQF_TRIGGER_LOW | IRQF_ONESHOT,
			"nau8825", nau8825);

		if (ret) {
			dev_err(dev, "Cannot request irq %d (%d)\n", nau8825->irq, ret);
			return ret;
		}
	}
	nau8825_restart_jack_detection(nau8825->regmap);

	return 0;
}

static struct snd_soc_codec_driver nau8825_codec_driver = {
	.probe = nau8825_codec_probe,

	.controls = nau8825_snd_controls,
	.num_controls = ARRAY_SIZE(nau8825_snd_controls),
	.dapm_widgets = nau8825_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(nau8825_dapm_widgets),
	.dapm_routes = nau8825_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(nau8825_dapm_routes),
};

static void nau8825_reset_chip(struct regmap *regmap)
{
	regmap_write(regmap, NAU8825_REG_RESET, 0x00);
	regmap_write(regmap, NAU8825_REG_RESET, 0x00);
}

static int nau8825_i2c_probe(struct i2c_client *i2c,
	const struct i2c_device_id *id)
{
	struct device *dev = &i2c->dev;
	struct nau8825 *nau8825;
	int ret, value;

	nau8825 = devm_kzalloc(dev, sizeof(*nau8825), GFP_KERNEL);
	if (!nau8825)
		return -ENOMEM;

	i2c_set_clientdata(i2c, nau8825);

	nau8825->regmap = devm_regmap_init_i2c(i2c, &nau8825_regmap_config);
	if (IS_ERR(nau8825->regmap))
		return PTR_ERR(nau8825->regmap);
	nau8825->irq = i2c->irq;
	nau8825->dev = dev;

	nau8825_reset_chip(nau8825->regmap);
	ret = regmap_read(nau8825->regmap, NAU8825_REG_I2C_DEVICE_ID, &value);
	if (ret < 0) {
		dev_err(dev, "Failed to read device id from the NAU8825: %d\n",
			ret);
		return ret;
	}
	if ((value & NAU8825_SOFTWARE_ID_MASK) !=
			NAU8825_SOFTWARE_ID_NAU8825) {
		dev_err(dev, "Not a NAU8825 chip\n");
		return -ENODEV;
	}

	return snd_soc_register_codec(&i2c->dev, &nau8825_codec_driver,
			&nau8825_dai, 1);
}

static int nau8825_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_codec(&client->dev);
	return 0;
}

static const struct i2c_device_id nau8825_i2c_ids[] = {
	{ "nau8825", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, nau8825_i2c_ids);

static struct i2c_driver nau8825_driver = {
	.driver = {
		.name = "nau8825",
		.owner = THIS_MODULE,
	},
	.probe = nau8825_i2c_probe,
	.remove = nau8825_i2c_remove,
	.id_table = nau8825_i2c_ids,
};
module_i2c_driver(nau8825_driver);

MODULE_DESCRIPTION("ASoC nau8825 driver");
MODULE_AUTHOR("Anatol Pomozov <anatol@chromium.org>");
MODULE_LICENSE("GPL");