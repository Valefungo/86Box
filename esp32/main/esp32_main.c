/*
 * esp32_main.c - app_main() entry point for the ESP32-P4 port.
 *
 * Mounts the SD card at /sdcard (real Tab5 pin mapping - see
 * mount_sdcard() below), brings up the 86Box core, and spawns exactly
 * one emulation thread that loops calling pc_run() - mirroring
 * src/unix/sdl_main.c's do_start()/main_thread() structure, without
 * BasiliskII's bespoke cooperative-task-plus-atomics model (86Box
 * already expects real pthreads, which is what thread.cpp gives us
 * here - see the M3 plan).
 */
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_pthread.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"

#include "esp_timer.h"

#include <86box/86box.h>
#include <86box/thread.h>
#include <86box/plat.h>
#include <86box/timer.h>
#include <86box/nvr.h>

static const char *TAG = "86box";

static SemaphoreHandle_t blit_mutex;
static thread_t         *emu_thread;

/* M5Stack Tab5 SDMMC pin mapping (slot 0, 4-bit), confirmed identical
 * across three independent working Tab5 ports (tiny386's storage.c,
 * picocalc-123's disk.c, M5Tab5-UserDemo's m5stack_tab5.c) - none of
 * these are ESP32-P4's generic SDMMC-slot-0 defaults by coincidence,
 * they're this board's actual wiring, so they must be set explicitly
 * rather than left to SDMMC_SLOT_CONFIG_DEFAULT(). The SD IO rail also
 * needs its own LDO power channel (LDO_VO4) turned on first - without
 * it every one of those three references reports mount failures, since
 * the card simply isn't powered. */
#define TAB5_SD_CLK       43
#define TAB5_SD_CMD       44
#define TAB5_SD_D0        39
#define TAB5_SD_D1        40
#define TAB5_SD_D2        41
#define TAB5_SD_D3        42
#define TAB5_SD_LDO_CHAN  4

static void
mount_sdcard(void)
{
    sd_pwr_ctrl_ldo_config_t ldo_cfg = {
        .ldo_chan_id = TAB5_SD_LDO_CHAN,
    };
    sd_pwr_ctrl_handle_t pwr_ctrl_handle = NULL;
    esp_err_t ret = sd_pwr_ctrl_new_on_chip_ldo(&ldo_cfg, &pwr_ctrl_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SD LDO power control init failed (0x%x) - SD card will not be powered", ret);
        return;
    }

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files              = 8,
        .allocation_unit_size   = 16 * 1024,
    };

    sdmmc_host_t host      = SDMMC_HOST_DEFAULT();
    host.slot              = SDMMC_HOST_SLOT_0;
    host.max_freq_khz       = SDMMC_FREQ_HIGHSPEED;
    host.pwr_ctrl_handle    = pwr_ctrl_handle;

    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 4;
    slot.clk   = TAB5_SD_CLK;
    slot.cmd   = TAB5_SD_CMD;
    slot.d0    = TAB5_SD_D0;
    slot.d1    = TAB5_SD_D1;
    slot.d2    = TAB5_SD_D2;
    slot.d3    = TAB5_SD_D3;
    slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    sdmmc_card_t *card = NULL;
    ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot, &mount_config, &card);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SD card mount failed (0x%x) - ROM/asset/disk paths under /sdcard will not work yet", ret);
    } else {
        ESP_LOGI(TAG, "SD card mounted at /sdcard");
    }
}

void
startblit(void)
{
    xSemaphoreTake(blit_mutex, portMAX_DELAY);
}

void
endblit(void)
{
    xSemaphoreGive(blit_mutex);
}

extern void esp32_video_init(void);
extern void esp32_video_test_pattern(void);
extern void esp32_input_init(void);
extern void esp32_input_poll(void);

static void
emulation_thread(void *param)
{
    (void) param;

    /* pc_init()/pc_init_roms()/pc_init_modules()/pc_reset_hard_init()
     * run here rather than in app_main() on purpose: they were written
     * for desktop OSes with megabyte-scale default thread stacks (86box.c's
     * own pc_init() alone has a 2KB local buffer before it even calls into
     * config/path-handling code several frames deeper). ESP-IDF's "main"
     * task stack is a few KB by default and internal SRAM is too tight in
     * this port to just grow it there (see the M3 BSS-budget saga) - so
     * this thread is deliberately given a large PSRAM-backed stack via
     * esp_pthread_set_cfg() in app_main() instead, right before it's
     * created. First real hardware boot (2026-07-26) hit exactly this as
     * a "Stack protection fault" inside fopen() from pc_init() running on
     * the tiny main-task stack - this restructuring is the fix. */
    static char *fake_argv[] = { "86box", NULL };

    /* Diagnostic (2026-07-28): a real PSRAM-heap-exhaustion crash
     * (calloc() failure in mem_reset()'s pages[] allocation) showed only
     * ~246KB free PSRAM heap by the time mem_reset() runs for a machine
     * with just 8MB of configured guest RAM - far less than a rough
     * hand-count of known static/dynamic consumers suggested should be
     * left. Trace free PSRAM/internal heap at each boot-sequence
     * checkpoint to find where it actually goes, instead of guessing. */
#define HEAP_LOG(msg)                                                                          \
    ESP_LOGI(TAG, msg " - free PSRAM=%u free internal=%u largest_psram=%u largest_internal=%u", \
             (unsigned) heap_caps_get_free_size(MALLOC_CAP_SPIRAM),                              \
             (unsigned) heap_caps_get_free_size(MALLOC_CAP_INTERNAL),                            \
             (unsigned) heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM),                     \
             (unsigned) heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL))

    HEAP_LOG("before pc_init");
    if (!pc_init(1, fake_argv)) {
        ESP_LOGE(TAG, "pc_init failed");
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "pc_init done, scanning ROMs");
    HEAP_LOG("after pc_init");
    if (!pc_init_roms()) {
        ESP_LOGE(TAG, "No usable ROM images found under /sdcard/86box/roms/");
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "pc_init_roms done");
    HEAP_LOG("after pc_init_roms");
    pc_init_modules();
    ESP_LOGI(TAG, "pc_init_modules done");
    HEAP_LOG("after pc_init_modules");
    pc_reset_hard_init();
    ESP_LOGI(TAG, "pc_reset_hard_init done, entering main loop");
    HEAP_LOG("after pc_reset_hard_init");
#undef HEAP_LOG

    /* Set the PAUSE mode depending on the renderer - same call, same
     * place in the sequence, as src/unix/sdl_main.c's do_start() path.
     * dopause starts at 1 (paused) in 86box.c - without this, pc_run()
     * below would never execute at all. */
    plat_pause(0);

    /*
     * Real-time-paced main loop, ported from src/unix/sdl_main.c's
     * main_thread() - the first version of this loop just called
     * pc_run() back-to-back with no pacing at all, which both ran the
     * guest CPU far faster than real time and, more urgently, never
     * gave FreeRTOS's idle task a chance to run on this core, which
     * tripped the task watchdog on real Tab5 hardware (2026-07-26).
     * fast_forward/gdbstub support from the desktop version are
     * dropped: fast_forward is only ever defined by the desktop sound
     * backends this port doesn't compile, and gdbstub is desktop-only
     * debug tooling, neither applies here.
     */
    int64_t old_time  = esp_timer_get_time() / 1000;
    int     drawits   = 0;
    int     frames    = 0;

    /* Liveness/diagnostic counters - a periodic heartbeat here is the
     * only way to tell "pc_run() is being called but nothing gets drawn"
     * apart from "the loop never even calls pc_run()" (e.g. dopause stuck
     * at 1), since neither case crashes or trips the watchdog. */
    int64_t last_log_ms  = old_time;
    uint32_t pc_run_count = 0;

    /* esp32_input_poll() does real I2C transactions (touch + physical
     * keyboard), each easily costing single-digit milliseconds - calling
     * it on every single loop iteration (as opposed to sdl_main.c's
     * cheap, non-blocking SDL event poll on desktop) throttles the
     * *entire* loop, including pc_run(), down to I2C polling speed. Rate-
     * limit it to a plenty-fast-for-humans ~60Hz instead, decoupling
     * input latency from CPU emulation throughput. */
    int64_t last_input_poll_ms = old_time;

    while (!is_quit) {
        int64_t new_time = esp_timer_get_time() / 1000;
        drawits += (int) (new_time - old_time);
        old_time = new_time;

        if ((new_time - last_log_ms) >= 3000) {
            ESP_LOGI(TAG, "main loop alive: t=%lldms dopause=%d drawits=%d pc_run_count=%u",
                     (long long) new_time, dopause, drawits, (unsigned) pc_run_count);
            last_log_ms = new_time;
        }

        if ((drawits > 0) && !dopause) {
            drawits -= force_10ms ? 10 : 1;
            if (drawits > 50)
                drawits = 0;

            pc_run();
            pc_run_count++;

            /* Every 200 frames we save the machine status. */
            if (++frames >= (force_10ms ? 200 : 2000) && nvr_dosave) {
                nvr_save();
                nvr_dosave = 0;
                frames     = 0;
            }

            /* pc_run() alone now costs tens of ms of real wall-clock time
             * per call (see the M4 performance investigation) - drawits
             * gets replenished by that same amount on the *next*
             * iteration and its >50 clamp resets it to 0, so the "nothing
             * to do yet" branch below almost never runs anymore. Without
             * an explicit yield here too, this task never voluntarily
             * gives up CPU0, and at its default priority (higher than
             * IDLE0's) that's enough to genuinely starve IDLE0 long
             * enough to trip the FreeRTOS task watchdog (observed on
             * real hardware, 2026-07-28). One tick is negligible next to
             * a ~90-100ms pc_run() call but guarantees IDLE0 gets
             * scheduled every iteration regardless of the drawits state. */
            vTaskDelay(1);
        } else {
            /* Nothing to do yet - yield so idle/watchdog housekeeping
             * on this core actually gets to run. */
            vTaskDelay(pdMS_TO_TICKS(1));
        }

        if ((new_time - last_input_poll_ms) >= 16) {
            esp32_input_poll();
            last_input_poll_ms = new_time;
        }
    }

    pc_close(NULL);
    vTaskDelete(NULL);
}

void
app_main(void)
{
    ESP_LOGI(TAG, "86Box-minimal starting");

    mount_sdcard();

    blit_mutex = xSemaphoreCreateMutex();

    esp32_video_init();

    /* Diagnostic (2026-07-27): push a fixed test pattern straight to the
     * panel here, before pc_init()/the CPU/the emulator ever run - proves
     * or disproves whether the display path itself (panel bring-up, DSI
     * push, rot_buf) works at all, independent of whether 86Box's own
     * CPU emulation ever gets far enough to request a redraw. Remove
     * once the display path is confirmed working on real hardware. */
    esp32_video_test_pattern();

    esp32_input_init();

    /* Large PSRAM-backed stack for the emulation thread - see the
     * comment at the top of emulation_thread() for why. Only affects
     * the next std::thread/pthread created on this calling context
     * (thread_create() right below), not any other thread in the app. */
    esp_pthread_cfg_t pthread_cfg = esp_pthread_get_default_config();
    pthread_cfg.thread_name       = "emu";
    pthread_cfg.stack_size        = 65536;
    pthread_cfg.stack_alloc_caps  = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
    /* Pinning emulation_thread to core 1 + raising its priority (tried
     * 2026-07-27, reasoning: rule out preemption by same-core system
     * tasks as the cause of cpu_exec()'s ~10-11ms/call cost) made no
     * measurable difference on real hardware, and reverting it is the
     * cleanest way to isolate whether it's contributing to the *worse*
     * ~87-89ms/call figure seen after the gfxcard config fix landed (the
     * blit-path diagnostics never fire either way, so that slowdown
     * isn't explained by real blit waits) - back to ESP-IDF's own
     * defaults (CONFIG_PTHREAD_TASK_PRIO_DEFAULT, tskNO_AFFINITY) for
     * both this thread and anything it creates in turn (blit_thread),
     * same as before any of today's threading experiments. */
    esp_pthread_set_cfg(&pthread_cfg);

    emu_thread = thread_create(emulation_thread, NULL);
    thread_wait(emu_thread);
}
