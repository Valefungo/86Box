/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          Sound emulation core.
 *
 * Authors: Sarah Walker, <https://pcem-emulator.co.uk/>
 *          Miran Grca, <mgrca8@gmail.com>
 *          Jasmine Iwanek, <jriwanek@gmail.com>
 *
 *          Copyright 2008-2018 Sarah Walker.
 *          Copyright 2016-2025 Miran Grca.
 *          Copyright 2024-2026 Jasmine Iwanek.
 */
#ifndef EMU_SOUND_H
#define EMU_SOUND_H

#define SOUND_CARD_MAX 4 /* currently we support up to 4 sound cards and a standalone MPU401 */

extern int  sound_gain;
extern char sound_output_device[512]; /* selected audio output device name, empty = system default */

enum {
    I_NORMAL = 0,
    I_MUSIC,
    I_WT,
    I_CD,
    I_FDD,
    I_HDD,
    I_YM2151,
    I_MIDI,
    I_MAX
};

#define FREQ_44100  44100
#define FREQ_48000  48000
#define FREQ_49716  49716
#define FREQ_55930  55930
#define FREQ_88200  88200
#define FREQ_96000  96000

#define SOUND_FREQ   FREQ_48000
#define SOUNDBUFLEN  (SOUND_FREQ / 50)

#define MUSIC_FREQ   FREQ_49716
#define MUSICBUFLEN  (MUSIC_FREQ / 36)

#define YM2151_FREQ  FREQ_55930
#define YM2151BUFLEN (YM2151_FREQ / 70)

#define CD_FREQ      FREQ_44100
#define CD_BUFLEN    (CD_FREQ / 10)

#define WT_FREQ      FREQ_44100
#define WTBUFLEN     (WT_FREQ / 45)

enum {
    SOUND_NONE = 0,
    SOUND_INTERNAL
};

extern int ppispeakon;
extern int speakon;

extern int midi_freq;
extern int midi_buf_size;

extern int sound_pos_global;
extern int music_pos_global;
extern int ym2151_pos_global;
extern int wavetable_pos_global;

extern int sound_card_current[SOUND_CARD_MAX];

extern void sound_add_handler(void (*get_buffer)(int32_t *buffer,
                                                 uint16_t len, void *priv),
                              void *priv);

extern void music_add_handler(void (*get_buffer)(int32_t *buffer,
                                                 uint16_t len, void *priv),
                              void *priv);

extern void ym2151_add_handler(void (*get_buffer)(int32_t *buffer,
                                                  uint16_t len, void *priv),
                               void *priv);

extern void wavetable_add_handler(void (*get_buffer)(int32_t *buffer,
                                                     uint16_t len, void *priv),
                                  void *priv);

extern void sound_set_cd_audio_filter(void (*filter)(int     channel,
                                                     double *buffer, void *priv),
                                      void *priv);
extern void sound_set_pc_speaker_filter(void (*filter)(int     channel,
                                                       double *buffer, void *priv),
                                        void *priv);
extern void sound_set_midi_filter(void (*filter)(int     channel,
                                                 double *buffer, void *priv),
                                  void *priv);

extern void (*filter_pc_speaker)(int channel, double *buffer, void *priv);
extern void *filter_pc_speaker_p;

extern void (*filter_midi)(int channel, double *buffer, void *priv);
extern void *filter_midi_p;

extern int sound_card_available(int card);
#ifdef EMU_DEVICE_H
extern const device_t *sound_card_getdevice(int card);
#endif
extern int         sound_card_has_config(int card);
extern const char *sound_card_get_internal_name(int card);
extern int         sound_card_get_from_internal_name(const char *s);
extern void        sound_card_init(void);
extern void        sound_set_cd_volume(unsigned int vol_l, unsigned int vol_r);

extern void sound_speed_changed(void);

extern void sound_init(void);
extern void sound_reset(void);

extern void sound_card_reset(void);

extern void sound_recalc_timers(void);
extern void sound_close(void);

extern void sound_cd_thread_end(void);
extern void sound_cd_thread_reset(void);

extern void sound_fdd_thread_init(void);
extern void sound_fdd_thread_end(void);

extern void sound_hdd_thread_init(void);
extern void sound_hdd_thread_end(void);

extern const char *sound_get_output_devices(void); /* returns double-null-terminated list, or NULL */
extern int         sound_get_device_sample_rate(const char *device_name);   /* probe native rate, 0 = unknown */
extern int         sound_get_device_supported_rates(const char *device_name, /* probe supported rates into rates_out; returns count */
                                                    int *rates_out, int max_rates);
extern void        closeal(void);
extern void        inital(void);

#ifdef bool
extern bool        fast_forward;
#endif

extern unsigned long long src_freqs[I_MAX];

extern void        givealbuffer_common(const void *buf, const uint8_t src, const int size);

#define givealbuffer(b)         givealbuffer_common(b, I_NORMAL, (sound_sample_rate / 50) << 1)
#define givealbuffer_music(b)   givealbuffer_common(b, I_MUSIC, MUSICBUFLEN << 1)
#define givealbuffer_ym2151(b)  givealbuffer_common(b, I_YM2151, YM2151BUFLEN << 1)
#define givealbuffer_wt(b)      givealbuffer_common(b, I_WT, WTBUFLEN << 1)
#define givealbuffer_cd(b)      givealbuffer_common(b, I_CD, CD_BUFLEN << 1)
#define givealbuffer_fdd(b, s)  givealbuffer_common(b, I_FDD, s)
#define givealbuffer_hdd(b, s)  givealbuffer_common(b, I_HDD, s)
#define givealbuffer_midi(b, s) givealbuffer_common(b, I_MIDI, s)

#define sb_vibra16c_onboard_relocate_base sb_vibra16s_onboard_relocate_base
#define sb_vibra16cl_onboard_relocate_base sb_vibra16s_onboard_relocate_base
#define sb_vibra16xv_onboard_relocate_base sb_vibra16s_onboard_relocate_base
extern void sb_vibra16s_onboard_relocate_base(uint16_t new_addr, void *priv);

#ifdef EMU_DEVICE_H
/* AdLib and AdLib Gold */
extern const device_t adlib_device;
extern const device_t adgold_device;

/* Analog Devices AD1816 */

/* Aztech Sound Galaxy 16 */

/* C-Media CMI8x38 */

/* Covox ISA */

/* Creative Labs Game Blaster */

/* Creative Labs Sound Blaster */
extern const device_t sb_1_device;
extern const device_t sb_15_device;
extern const device_t sb_mcv_device;
extern const device_t sb_2_device;
extern const device_t sb_pro_v1_device;
extern const device_t sb_pro_v2_device;
extern const device_t sb_pro_mcv_device;
extern const device_t sb_pro_compat_device;
extern const device_t sb_16_device;
extern const device_t sb_vibra16c_onboard_device;
extern const device_t sb_vibra16c_device;
extern const device_t sb_vibra16cl_onboard_device;
extern const device_t sb_vibra16cl_device;
extern const device_t sb_vibra16s_onboard_device;
extern const device_t sb_vibra16s_device;
extern const device_t sb_vibra16xv_onboard_device;
extern const device_t sb_vibra16xv_device;
extern const device_t sb_16_pnp_device;
extern const device_t sb_16_pnp_ide_device;
extern const device_t sb_16_compat_device;
extern const device_t sb_16_compat_nompu_device;
extern const device_t sb_16_reply_mca_device;
extern const device_t sb_goldfinch_device;
extern const device_t sb_32_pnp_device;
extern const device_t sb_awe32_device;
extern const device_t sb_awe32_pnp_device;
extern const device_t sb_awe32_ide_pnp_device;
extern const device_t sb_awe64_value_device;
extern const device_t sb_awe64_device;
extern const device_t sb_awe64_ide_device;
extern const device_t sb_awe64_gold_device;

/* Crystal CS423x */

/* ESS Technology */
extern const device_t ess_688_device;
extern const device_t ess_ess0100_pnp_device;
extern const device_t ess_ess0968_pnp_688_device;
extern const device_t ess_1688_device;
extern const device_t ess_1688_compaq_device;
extern const device_t ess_ess0102_pnp_device;
extern const device_t ess_ess0968_pnp_device;
extern const device_t ess_soundpiper_16_mca_device;
extern const device_t ess_soundpiper_32_mca_device;
extern const device_t ess_chipchat_16_mca_device;
extern const device_t ess_1788_device;
extern const device_t ess_1888_device;
extern const device_t ess_1888_compaq_device;
extern const device_t ess_1887_device;
extern const device_t ess_1868_device;
extern const device_t ess_1869_device;

/* Ensoniq AudioPCI */

/* Gravis UltraSound family */

/* IBM Music Feature Card */

/* IBM PS/1 Audio Card */
extern const device_t ps1snd_device;

/* Innovation SSI-2001 */

/* Mindscape Music Board */

/* MediaVision ThunderBoard */
extern const device_t thunderboard_device;

/* OPTi 82c93x */

/* PC Speaker */
extern const device_t speaker_device;

/* Pro Audio Spectrum, Plus, 16, and 16D */

/* Rainbow Arts PC-Soundman */

/* Tandy PSSJ */
extern const device_t pssj_device;
extern const device_t pssj_isa_device;
extern const device_t pssj_1e0_device;

/* Tandy PSG */
extern const device_t tndy_device;

/* Tandy Sensation */
extern const device_t sensationaud_device;

/* TexElec SAAYM */

/* Windows Sound System */

/* Yamaha YMF-7xx */

#ifdef USE_LIBSERIALPORT
/* External Audio device OPL2Board (Host Connected hardware)*/
#endif 

#endif

#endif /*EMU_SOUND_H*/
