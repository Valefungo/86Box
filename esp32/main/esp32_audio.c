/*
 * esp32_audio.c - audio backend contract (normally src/sound/openal.c).
 *
 * M3 step 4 (real I2S output through the Tab5's ES8388 codec + a
 * software mixer for the 8 independent PCM streams) is WRITTEN but
 * PAUSED, not deleted - see ESP32_AUDIO_REAL below. Flip that to 1 to
 * re-enable it once there's budget to close the gap it opens.
 *
 * Why paused (session 2026-07-26): enabling it pulls in ESP-IDF's
 * `esp_driver_i2s` component, which - purely because our I2S config
 * requests a precise MCLK output (`periph_rtc_apll_acquire/_freq_set`
 * in ESP-IDF's clk_ctrl_os.c) - drags in ~4.2KB of ESP-IDF's own APLL
 * clock-management internal state that wasn't linked before. That's
 * ESP-IDF's own code, not this repo's, so it can't be PSRAM-tagged the
 * way the rest of this port's BSS-overflow fixes work. By this point
 * in the port, every safely-taggable cold global anywhere in 86Box's
 * own code had already been moved to PSRAM in earlier passes (M3 steps
 * 1-3 and part of this one) - the remaining ~4.2KB gap has no more
 * low-risk material left to free it with. Investigated and explicitly
 * rejected as not worth it for now: cutting whole 86Box peripheral
 * features (modems - already fully absent from this build's file list
 * before this even came up; SCSI controllers, joysticks, cassette/tape,
 * PostScript printing) would need also detangling their machine_table.c/
 * device-table registrations, the same class of change this project's
 * own memory already documents as a dead end (bus-generic device
 * architecture means almost nothing is a clean, isolated removal).
 * Editing ESP-IDF's own shared SDK checkout was also rejected (affects
 * other unrelated projects on this machine). Revisit when there's a
 * concrete reason to spend the budget - e.g. confirming on real Tab5
 * hardware whether the ES8388 actually needs MCLK, which would remove
 * this dependency for free if not.
 *
 * The real implementation below (ES8388 bring-up adapted from
 * picocalc-123's platforms/tab5/platform_audio.cpp - the one real,
 * working ES8388 driver found for this exact board; tiny386's own
 * i2s.c only supports ES8311 and is a confirmed no-op on Tab5) is
 * otherwise complete and real-build-verified up to the link stage.
 */
#define ESP32_AUDIO_REAL 0

#include <86box/86box.h>
#include <86box/sound.h>

#if ESP32_AUDIO_REAL

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "esp_attr.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "esp32_audio";

static int initialized = 0;

/* Owned by esp32_video.c - see the file header comment for why the
 * codec shares this bus instead of creating its own. */
extern i2c_master_bus_handle_t esp32_sys_i2c_handle;

#define ES8388_I2C_ADDR 0x10

/* ES8388 I2S pins (M5Tab5): MCLK=30, BCLK(SCLK)=27, WS(LCLK)=29, DOUT=26. */
#define TAB5_I2S_MCLK 30
#define TAB5_I2S_BCLK 27
#define TAB5_I2S_WS   29
#define TAB5_I2S_DOUT 26

/* Fixed internal mix/output rate. Every stream (each at its own
 * src_freqs[i]) is resampled to this rate regardless of what
 * sound_sample_rate is currently configured to. */
#define OUTPUT_RATE      48000
#define MIX_CHUNK_FRAMES 256

/* ES8388 registers used for a minimal playback-only bring-up. */
#define ES8388_CONTROL1    0x00
#define ES8388_CONTROL2    0x01
#define ES8388_CHIPPOWER   0x02
#define ES8388_ADCPOWER    0x03
#define ES8388_DACPOWER    0x04
#define ES8388_MASTERMODE  0x08
#define ES8388_DACCONTROL1  0x17
#define ES8388_DACCONTROL2  0x18
#define ES8388_DACCONTROL3  0x19
#define ES8388_DACCONTROL4  0x1a
#define ES8388_DACCONTROL5  0x1b
#define ES8388_DACCONTROL16 0x26
#define ES8388_DACCONTROL17 0x27
#define ES8388_DACCONTROL20 0x2a
#define ES8388_DACCONTROL21 0x2b
#define ES8388_DACCONTROL23 0x2d
#define ES8388_DACCONTROL24 0x2e
#define ES8388_DACCONTROL25 0x2f
#define ES8388_DACCONTROL26 0x30
#define ES8388_DACCONTROL27 0x31

static i2c_master_dev_handle_t es8388_dev;
static i2s_chan_handle_t       i2s_tx_chan;

static esp_err_t
es8388_write(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return i2c_master_transmit(es8388_dev, buf, sizeof(buf), 50);
}

/* Bring up the ES8388 for stereo I2S playback through LOUT1/2+ROUT1/2 at
 * 0dB, with the ESP32-P4 as I2S master (codec runs in slave mode). */
static void
es8388_init_playback(void)
{
    es8388_write(ES8388_DACCONTROL3, 0x04); /* mute DAC while configuring */
    es8388_write(ES8388_CONTROL2,    0x50);
    es8388_write(ES8388_CHIPPOWER,   0x00); /* power up whole chip */

    es8388_write(ES8388_MASTERMODE,  0x00); /* codec = I2S slave */

    es8388_write(ES8388_DACPOWER,    0xC0); /* DAC + outputs off while configuring */
    es8388_write(ES8388_CONTROL1,    0x12); /* play & record mode */
    es8388_write(ES8388_DACCONTROL1, 0x18); /* 16-bit I2S */
    es8388_write(ES8388_DACCONTROL2, 0x02); /* 256x MCLK ratio, single speed */
    es8388_write(ES8388_DACCONTROL16, 0x00);
    es8388_write(ES8388_DACCONTROL17, 0x90); /* L DAC -> L mixer, 0dB */
    es8388_write(ES8388_DACCONTROL20, 0x90); /* R DAC -> R mixer, 0dB */
    es8388_write(ES8388_DACCONTROL21, 0x80); /* ADC/DAC share LRCK */
    es8388_write(ES8388_DACCONTROL23, 0x00);

    es8388_write(ES8388_DACCONTROL4, 0x00); /* L DAC volume 0dB */
    es8388_write(ES8388_DACCONTROL5, 0x00); /* R DAC volume 0dB */

    /* LOUT/ROUT volume: gain_dB = -45 + 1.5*reg, so 0x1E (30) is 0dB. */
    es8388_write(ES8388_DACCONTROL24, 0x1e);
    es8388_write(ES8388_DACCONTROL25, 0x1e);
    es8388_write(ES8388_DACCONTROL26, 0x1e);
    es8388_write(ES8388_DACCONTROL27, 0x1e);

    es8388_write(ES8388_ADCPOWER, 0xFF); /* ADC unused, power it down */

    es8388_write(ES8388_DACPOWER, 0x3C); /* power up DAC + LOUT1/ROUT1/LOUT2/ROUT2 */
    es8388_write(ES8388_DACCONTROL3, 0x00); /* unmute */
}

/* ---------- per-stream resampling ring buffer -------------------------- */

#define SRC_RING_FRAMES 4096 /* ~85ms at 48kHz, more at lower rates */

typedef struct {
    float           ring[SRC_RING_FRAMES][2];
    volatile size_t write_idx;
    volatile int    avail; /* frames currently buffered, written by
                             * producer, decremented by consumer */
    double          phase; /* consumer-only: fractional position within
                             * the next unconsumed frame */
    double          step;  /* src_freqs[i] / OUTPUT_RATE, refreshed on push */
} audio_src_t;

/* 8 streams * 4096 frames * 2 channels * 4 bytes = 256KB - far too big
 * for internal SRAM, this belongs in PSRAM regardless of the usual
 * hot-path-vs-cold tagging discipline used elsewhere in this port. */
static EXT_RAM_BSS_ATTR audio_src_t audio_srcs[I_MAX];

void
givealbuffer_common(const void *buf, const uint8_t src, const int size)
{
    /* No fast_forward check here: that global is only ever defined by
     * the desktop backends (openal.c/sndio.c/etc.) and the SDL platform
     * layer, none of which this port compiles - sound.h's own
     * declaration of it is itself dead here (guarded by `#ifdef bool`,
     * which this build's -std=gnu23 doesn't satisfy since bool is a
     * keyword, not a macro, so the declaration never appears either). */
    if (!initialized || src >= I_MAX)
        return;

    audio_src_t *s       = &audio_srcs[src];
    int          nframes = size / 2; /* size counts interleaved L+R scalars */
    int          avail   = s->avail;
    int          room    = SRC_RING_FRAMES - avail;
    if (nframes > room)
        nframes = room; /* drop overflow - keep already-queued audio intact */

    size_t widx = s->write_idx;
    if (sound_is_float) {
        const float *fbuf = (const float *) buf;
        for (int i = 0; i < nframes; i++) {
            s->ring[widx][0] = fbuf[i * 2 + 0];
            s->ring[widx][1] = fbuf[i * 2 + 1];
            widx             = (widx + 1) % SRC_RING_FRAMES;
        }
    } else {
        const int16_t *ibuf = (const int16_t *) buf;
        for (int i = 0; i < nframes; i++) {
            s->ring[widx][0] = (float) ibuf[i * 2 + 0] / 32768.0f;
            s->ring[widx][1] = (float) ibuf[i * 2 + 1] / 32768.0f;
            widx             = (widx + 1) % SRC_RING_FRAMES;
        }
    }
    s->write_idx = widx;
    s->step      = (double) src_freqs[src] / OUTPUT_RATE;
    s->avail     = avail + nframes;
}

/* ---------- mix + I2S output task --------------------------------------- */

static void
audio_task(void *arg)
{
    (void) arg;
    static EXT_RAM_BSS_ATTR int16_t out_buf[MIX_CHUNK_FRAMES * 2];

    for (;;) {
        for (int f = 0; f < MIX_CHUNK_FRAMES; f++) {
            float accl = 0.0f, accr = 0.0f;

            for (int i = 0; i < I_MAX; i++) {
                audio_src_t *s     = &audio_srcs[i];
                int          avail = s->avail;
                if (avail < 2)
                    continue; /* not enough buffered data - contribute silence */

                size_t widx  = s->write_idx;
                size_t ridx0 = (widx + SRC_RING_FRAMES - (size_t) avail) % SRC_RING_FRAMES;
                size_t ridx1 = (ridx0 + 1) % SRC_RING_FRAMES;
                float  frac  = (float) s->phase;

                accl += s->ring[ridx0][0] + (s->ring[ridx1][0] - s->ring[ridx0][0]) * frac;
                accr += s->ring[ridx0][1] + (s->ring[ridx1][1] - s->ring[ridx0][1]) * frac;

                s->phase += s->step;
                while (s->phase >= 1.0 && avail >= 2) {
                    s->phase -= 1.0;
                    avail--;
                }
                s->avail = avail;
            }

            const double gain = sound_muted ? 0.0 : pow(10.0, (double) sound_gain / 20.0);
            accl *= (float) gain;
            accr *= (float) gain;
            if (accl > 1.0f)
                accl = 1.0f;
            else if (accl < -1.0f)
                accl = -1.0f;
            if (accr > 1.0f)
                accr = 1.0f;
            else if (accr < -1.0f)
                accr = -1.0f;

            out_buf[f * 2 + 0] = (int16_t) (accl * 32767.0f);
            out_buf[f * 2 + 1] = (int16_t) (accr * 32767.0f);
        }

        size_t written;
        i2s_channel_write(i2s_tx_chan, out_buf, sizeof(out_buf), &written, portMAX_DELAY);
    }
}

static esp_err_t
audio_hw_init(void)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = ES8388_I2C_ADDR,
        .scl_speed_hz    = 100000,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(esp32_sys_i2c_handle, &dev_cfg, &es8388_dev),
                        TAG, "ES8388 add device failed");

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear        = true;
    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan_cfg, &i2s_tx_chan, NULL),
                        TAG, "I2S channel alloc failed");

    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(OUTPUT_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = (gpio_num_t) TAB5_I2S_MCLK,
            .bclk = (gpio_num_t) TAB5_I2S_BCLK,
            .ws   = (gpio_num_t) TAB5_I2S_WS,
            .dout = (gpio_num_t) TAB5_I2S_DOUT,
            .din  = I2S_GPIO_UNUSED,
        },
    };
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(i2s_tx_chan, &std_cfg),
                        TAG, "I2S std init failed");
    ESP_RETURN_ON_ERROR(i2s_channel_enable(i2s_tx_chan), TAG, "I2S enable failed");

    es8388_init_playback();

    ESP_LOGI(TAG, "audio ready (ES8388 @0x%02X, I2S0, %d Hz)", ES8388_I2C_ADDR, OUTPUT_RATE);
    return ESP_OK;
}

void
inital(void)
{
    if (initialized)
        return;

    memset(audio_srcs, 0, sizeof(audio_srcs));

    if (audio_hw_init() != ESP_OK) {
        ESP_LOGE(TAG, "audio hardware init failed - continuing silent");
        return;
    }

    xTaskCreate(audio_task, "esp32_audio", 4096, NULL, 5, NULL);

    initialized = 1;
}

void
closeal(void)
{
    /* The mixer task keeps running (draining to silence as the ring
     * buffers empty) rather than tearing down the I2S channel - nothing
     * calls inital() a second time in this port, so there is no
     * re-init path to keep clean for. */
    initialized = 0;
}

const char *
sound_get_output_devices(void)
{
    return NULL;
}

int
sound_get_device_sample_rate(const char *device_name)
{
    (void) device_name;
    return OUTPUT_RATE;
}

int
sound_get_device_supported_rates(const char *device_name, int *rates_out, int max_rates)
{
    (void) device_name;
    if (max_rates > 0)
        rates_out[0] = OUTPUT_RATE;
    return max_rates > 0 ? 1 : 0;
}

#else /* !ESP32_AUDIO_REAL - silent stub, see the file header comment */

void
inital(void)
{
}

void
closeal(void)
{
}

void
givealbuffer_common(const void *buf, const uint8_t src, const int size)
{
    (void) buf;
    (void) src;
    (void) size;
}

const char *
sound_get_output_devices(void)
{
    return NULL;
}

int
sound_get_device_sample_rate(const char *device_name)
{
    (void) device_name;
    return 0;
}

int
sound_get_device_supported_rates(const char *device_name, int *rates_out, int max_rates)
{
    (void) device_name;
    (void) rates_out;
    (void) max_rates;
    return 0;
}

#endif /* ESP32_AUDIO_REAL */
