#
# 86Box-minimal ESP32-P4 port - main component CMakeLists.txt
#
# Explicit source list (not the shared src/CMakeLists.txt - kept
# deliberately separate so the desktop build is never at risk of
# breaking from ESP32-specific changes; see the M3 plan for the
# reasoning). Extracted from each subsystem's own CMakeLists.txt,
# respecting DYNAREC=OFF/NEW_DYNAREC=OFF (codegen*/386_dynarec_ops.c
# excluded) and skipping desktop-only optional backends (FluidSynth,
# RtMidi, MUNT, OpenAL, VFIO, Wacom, etc.) that don't apply here.
#

# version.h.in expects the standard CMAKE_PROJECT_* variables, which are
# only set by CMake's own project() command - ESP-IDF's project.cmake
# wraps project() and tracks its own PROJECT_VERSION_* variables instead,
# so CMAKE_PROJECT_* stay unset here (silently expanding to nothing rather
# than erroring, which showed up downstream as e.g. BCD8(EMU_VERSION_MAJ)
# expanding to BCD8() in src/mem/spd.c). Set them by hand to match the
# desktop build's project(86Box VERSION 7.0 ...) call.
set(CMAKE_PROJECT_NAME "86Box")
set(CMAKE_PROJECT_VERSION "7.0")
set(CMAKE_PROJECT_VERSION_MAJOR "7")
set(CMAKE_PROJECT_VERSION_MINOR "0")
set(CMAKE_PROJECT_VERSION_PATCH "0")
if(NOT EMU_COPYRIGHT_YEAR)
    set(EMU_COPYRIGHT_YEAR 2026)
endif()

set(EMU_BUILD_NUM 0)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../../src/include/86box/version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated_include/86box/version.h"
    @ONLY
)

set(BOX86_CORE_SRCS
    "../../src/86box.c"
    "../../src/config.c"
    "../../src/timer.c"
    "../../src/io.c"
    "../../src/acpi.c"
    "../../src/apm.c"
    "../../src/dma.c"
    "../../src/ddma.c"
    "../../src/nmi.c"
    "../../src/pci_dummy.c"
    "../../src/pic.c"
    "../../src/pit.c"
    "../../src/pit_exact.c"
    "../../src/pit_fast.c"
    "../../src/port_6x.c"
    "../../src/port_92.c"
    "../../src/ppi.c"
    "../../src/pci.c"
    "../../src/mca.c"
    "../../src/device.c"
    "../../src/nvr.c"
    "../../src/nvr_at.c"
    "../../src/nvr_ps2.c"
    "../../src/machine_status.c"
    "../../src/thread.cpp"
    "../../src/cdrom/cdrom.c"
    "../../src/cdrom/cdrom_aaru.c"
    "../../src/cdrom/cdrom_image_viso.c"
    "../../src/cdrom/cdrom_mke.c"
    "../../src/cdrom/cdrom_image_ccd.c"
    # cdrom_image.c (zlib+sndfile) and cdrom_chd.c (zlib+libchdr) need
    # libraries that don't exist on ESP-IDF - stubbed by
    # esp32_cdrom_stub.c below instead of vendoring all three.
    "../../src/unix/dummy_cdrom_ioctl.c"
    "../../src/unix/dummy_floppy_ioctl.c"
    "../../src/char/char.c"
    "../../src/char/char_file.c"
    "../../src/char/char_loopback.c"
    # char_pipe.c (mkfifo - no named-pipe concept on ESP-IDF/FAT) and
    # char_serial.c/char_stdio.c (bridge a serial port to a real host TTY
    # via termios/PTY - same "no host OS underneath" story) are stubbed by
    # esp32_char_stub.c below.
    "../../src/chipset/82c100.c"
    "../../src/chipset/acc2036.c"
    "../../src/chipset/acc2168.c"
    "../../src/chipset/cs8220.c"
    "../../src/chipset/cs8230.c"
    "../../src/chipset/ali1429.c"
    "../../src/chipset/ali1435.c"
    "../../src/chipset/ali1489.c"
    "../../src/chipset/ali6117.c"
    "../../src/chipset/ali1409.c"
    "../../src/chipset/compaq.c"
    "../../src/chipset/compaq_386.c"
    "../../src/chipset/contaq_82c59x.c"
    "../../src/chipset/cs4031.c"
    "../../src/chipset/grid1520.c"
    "../../src/chipset/gc100.c"
    "../../src/chipset/headland.c"
    "../../src/chipset/ims8848.c"
    "../../src/chipset/intel_82335.c"
    "../../src/chipset/intel_420ex.c"
    "../../src/chipset/intel_4x0.c"
    "../../src/chipset/intel_sio.c"
    "../../src/chipset/isa486c.c"
    "../../src/ioapic.c"
    "../../src/chipset/laserxt.c"
    "../../src/chipset/neat.c"
    "../../src/chipset/olivetti_eva.c"
    "../../src/chipset/opti283.c"
    "../../src/chipset/opti291.c"
    "../../src/chipset/opti391.c"
    "../../src/chipset/opti495.c"
    "../../src/chipset/opti498.c"
    "../../src/chipset/opti499.c"
    "../../src/chipset/opti602.c"
    "../../src/chipset/opti822.c"
    "../../src/chipset/opti895.c"
    "../../src/chipset/philips.c"
    "../../src/chipset/sanyo.c"
    "../../src/chipset/scamp.c"
    "../../src/chipset/scat.c"
    "../../src/chipset/sis_85c310.c"
    "../../src/chipset/sis_85c4xx.c"
    "../../src/chipset/sis_85c496.c"
    "../../src/chipset/sl82c461.c"
    "../../src/chipset/stpc.c"
    "../../src/chipset/via_vt82c49x.c"
    "../../src/chipset/via_vt82c505.c"
    "../../src/chipset/umc_8886.c"
    "../../src/chipset/umc_hb4.c"
    "../../src/chipset/vl82c480.c"
    "../../src/chipset/wd76c10.c"
    "../../src/device/access_bus.c"
    "../../src/device/ast_nvr.c"
    "../../src/device/ast_readout.c"
    "../../src/device/bugger.c"
    "../../src/device/cartridge.c"
    "../../src/device/cassette.c"
    "../../src/device/clock_ics9xxx.c"
    "../../src/device/dell_jumper.c"
    "../../src/device/hasp.c"
    "../../src/device/hwm.c"
    "../../src/device/hwm_gl518sm.c"
    "../../src/device/hwm_lm75.c"
    "../../src/device/hwm_lm78.c"
    "../../src/device/hwm_vt82c686.c"
    "../../src/device/i2c.c"
    "../../src/device/i2c_gpio.c"
    "../../src/device/ibm_5161.c"
    "../../src/device/isamem.c"
    "../../src/device/isarom.c"
    "../../src/device/isartc.c"
    "../../src/device/isapnp.c"
    "../../src/device/kbc_at.c"
    "../../src/device/kbc_at_dev.c"
    "../../src/device/kbc_xt.c"
    "../../src/device/keyboard.c"
    "../../src/device/keyboard_at.c"
    "../../src/device/keyboard_xt.c"
    "../../src/device/lpt.c"
    "../../src/device/mouse.c"
    "../../src/device/mouse_bus.c"
    "../../src/device/mouse_microtouch_touchscreen.c"
    "../../src/device/mouse_ps2.c"
    "../../src/device/mouse_serial.c"
    "../../src/device/mouse_upc.c"
    "../../src/device/nec_mate_unk.c"
    "../../src/device/novell_cardkey.c"
    "../../src/device/pci_bridge.c"
    "../../src/device/phoenix_486_jumper.c"
    "../../src/device/postcard.c"
    "../../src/device/radisys_config.c"
    "../../src/device/serial.c"
    "../../src/device/smbus_ali7101.c"
    "../../src/device/smbus_piix4.c"
    "../../src/device/smbus_sis5595.c"
    "../../src/device/tulip_jumper.c"
    "../../src/device/unittester.c"
    "../../src/device/zenith_scratchpad.c"
    "../../src/disk/hdd.c"
    "../../src/disk/hdd_image.c"
    "../../src/disk/hdd_table.c"
    "../../src/disk/hdc.c"
    "../../src/disk/hdc_st506_xt.c"
    "../../src/disk/hdc_st506_at.c"
    "../../src/disk/hdc_xta.c"
    "../../src/disk/hdc_xta_ps1.c"
    "../../src/disk/hdc_esdi_at.c"
    "../../src/disk/hdc_xtide.c"
    "../../src/disk/hdc_ide.c"
    "../../src/disk/hdc_ide_ali5213.c"
    "../../src/disk/hdc_ide_opti611.c"
    "../../src/disk/hdc_ide_cmd640.c"
    "../../src/disk/hdc_ide_cmd646.c"
    "../../src/disk/hdc_ide_pc87410.c"
    "../../src/disk/hdc_ide_rz1000.c"
    "../../src/disk/hdc_ide_sff8038i.c"
    "../../src/disk/hdc_ide_um8673f.c"
    "../../src/disk/hdc_ide_w83769f.c"
    "../../src/disk/hdd_audio.c"
    "../../src/disk/rdisk.c"
    "../../src/disk/mo.c"
    "../../src/disk/minivhd/cwalk.c"
    "../../src/disk/minivhd/xml2_encoding.c"
    "../../src/disk/minivhd/convert.c"
    "../../src/disk/minivhd/create.c"
    "../../src/disk/minivhd/minivhd_io.c"
    "../../src/disk/minivhd/manage.c"
    "../../src/disk/minivhd/struct_rw.c"
    "../../src/disk/minivhd/minivhd_util.c"
    "../../src/floppy/fdd.c"
    "../../src/floppy/fdc.c"
    "../../src/floppy/fdc_compaticard.c"
    "../../src/floppy/fdc_magitronic.c"
    "../../src/floppy/fdc_monster.c"
    "../../src/floppy/fdc_pii15xb.c"
    "../../src/floppy/fdi2raw.c"
    "../../src/floppy/fdd_common.c"
    "../../src/floppy/fdd_86f.c"
    "../../src/floppy/fdd_fdi.c"
    "../../src/floppy/fdd_imd.c"
    "../../src/floppy/fdd_img.c"
    "../../src/floppy/fdd_pcjs.c"
    "../../src/floppy/fdd_mfm.c"
    "../../src/floppy/fdd_td0.c"
    "../../src/floppy/lzw/lzwdecode.c"
    "../../src/floppy/lzw/lzwencode.c"
    "../../src/floppy/fdd_tape.c"
    "../../src/floppy/fdd_audio.c"
    "../../src/game/gameport.c"
    "../../src/game/joystick_standard.c"
    "../../src/game/joystick_ch_flightstick_pro.c"
    "../../src/game/joystick_sw_pad.c"
    "../../src/game/joystick_tm_fcs.c"
    "../../src/machine/machine.c"
    "../../src/machine/machine_table.c"
    "../../src/machine/m_xt.c"
    "../../src/machine/m_xt_t1000.c"
    "../../src/machine/m_xt_jukost.c"
    "../../src/machine/m_xt_xi8088.c"
    "../../src/machine/m_pcjr.c"
    "../../src/machine/m_amstrad.c"
    "../../src/machine/m_europc.c"
    "../../src/machine/m_elt.c"
    "../../src/machine/m_xt_olivetti.c"
    "../../src/machine/m_xt_ibm5550.c"
    "../../src/machine/m_tandy.c"
    "../../src/machine/m_v86p.c"
    "../../src/machine/m_at_t3100e.c"
    "../../src/machine/m_ps1.c"
    "../../src/machine/m_ps2_isa.c"
    "../../src/machine/m_at_common.c"
    "../../src/machine/m_at_286.c"
    "../../src/machine/m_at_386sx.c"
    "../../src/machine/m_at_m6117.c"
    "../../src/machine/m_at_386dx.c"
    "../../src/machine/m_at_486slc.c"
    "../../src/machine/m_at_386dx_486.c"
    "../../src/machine/m_at_socket1.c"
    "../../src/machine/m_at_socket2.c"
    "../../src/machine/m_at_socket3.c"
    "../../src/machine/m_at_socket3_pci.c"
    "../../src/machine/m_at_486_misc.c"
    "../../src/mem/amd_flash.c"
    "../../src/mem/catalyst_flash.c"
    "../../src/mem/i2c_eeprom.c"
    "../../src/mem/intel_flash.c"
    "../../src/mem/mem.c"
    "../../src/mem/mmu_2386.c"
    "../../src/mem/nmc93cxx.c"
    "../../src/mem/rom.c"
    "../../src/mem/row.c"
    "../../src/mem/smram.c"
    "../../src/mem/spd.c"
    "../../src/mem/sst_flash.c"
    "../../src/printer/prt_cpmap.c"
    "../../src/printer/prt_text.c"
    "../../src/printer/prt_ps.c"
    # png.c (libpng) and prt_escp.c (freetype2, and the only caller of
    # png.c's functions) need libraries that don't exist on ESP-IDF -
    # stubbed by esp32_printer_stub.c below.
    "../../src/sio/sio_82091aa.c"
    "../../src/sio/sio_90c50.c"
    "../../src/sio/sio_acc3221.c"
    "../../src/sio/sio_ali5113.c"
    "../../src/sio/sio_cbm_io.c"
    "../../src/sio/sio_gm82c803ab.c"
    "../../src/sio/sio_f82c606.c"
    "../../src/sio/sio_f82c710.c"
    "../../src/sio/sio_fdc37c6xx.c"
    "../../src/sio/sio_fdc37c669.c"
    "../../src/sio/sio_p82c604.c"
    "../../src/sio/sio_pc87310.c"
    "../../src/sio/sio_pc873xx.c"
    "../../src/sio/sio_w837x7.c"
    "../../src/sio/sio_w83877.c"
    "../../src/sio/sio_w83977.c"
    "../../src/sio/sio_um866x.c"
    "../../src/sio/sio_um8669f.c"
    "../../src/sio/sio_vl82c113.c"
    "../../src/scsi/scsi.c"
    "../../src/scsi/scsi_device.c"
    "../../src/scsi/scsi_cdrom.c"
    "../../src/scsi/scsi_disk.c"
    "../../src/scsi/scsi_tape.c"
    "../../src/scsi/scsi_x54x.c"
    "../../src/scsi/scsi_aha154x.c"
    "../../src/scsi/scsi_buslogic.c"
    "../../src/scsi/scsi_ncr5380.c"
    "../../src/scsi/scsi_ncr53c400.c"
    "../../src/scsi/scsi_t128.c"
    "../../src/scsi/scsi_ncr53c8xx.c"
    "../../src/scsi/scsi_pcscsi.c"
    "../../src/scsi/scsi_ql1xxx.c"
    "../../src/sound/sound.c"
    "../../src/sound/snd_opl.c"
    "../../src/sound/snd_opl2_nuked.c"
    "../../src/sound/snd_opl3_nuked.c"
    "../../src/sound/snd_cqm_nuked.c"
    "../../src/sound/snd_opl_ymfm.cpp"
    "../../src/sound/snd_resid.cpp"
    "../../src/sound/midi.c"
    "../../src/sound/snd_speaker.c"
    "../../src/sound/snd_lpt_adlipt.c"
    "../../src/sound/snd_lpt_cms.c"
    "../../src/sound/snd_lpt_dac.c"
    "../../src/sound/snd_lpt_dss.c"
    "../../src/sound/snd_lpt_tndlpt.c"
    "../../src/sound/snd_pssj.c"
    "../../src/sound/snd_ps1.c"
    "../../src/sound/snd_adlib.c"
    "../../src/sound/snd_adlibgold.c"
    "../../src/sound/snd_ad1848.c"
    "../../src/sound/snd_azt2316a.c"
    "../../src/sound/snd_azt2320.c"
    "../../src/sound/snd_cms.c"
    "../../src/sound/snd_sb.c"
    "../../src/sound/snd_sb_dsp.c"
    "../../src/sound/snd_emu8k.c"
    "../../src/sound/snd_mpu401.c"
    "../../src/sound/snd_sn76489.c"
    "../../src/sound/snd_ym7128.c"
    "../../src/sound/snd_opl_esfm.c"
    "../../src/sound/snd_ps1_midi.c"
    "../../src/sound/snd_sensation.c"
    "../../src/sound/sound_util.c"
    "../../src/utils/cJSON.c"
    "../../src/utils/crc.c"
    "../../src/utils/crc32.c"
    "../../src/utils/fifo.c"
    "../../src/utils/fifo8.c"
    "../../src/utils/ini.c"
    "../../src/utils/log.c"
    "../../src/utils/random.c"
    "../../src/utils/mdsx_dll.c"
    "../../src/video/agpgart.c"
    "../../src/video/video.c"
    "../../src/video/vid_table.c"
    "../../src/video/ramdac/vid_ramdac_ati68860.c"
    "../../src/video/ramdac/vid_ramdac_ati68875.c"
    "../../src/video/ramdac/vid_ramdac_att20c49x.c"
    "../../src/video/ramdac/vid_ramdac_att2xc498.c"
    "../../src/video/ramdac/vid_ramdac_bt481.c"
    "../../src/video/ramdac/vid_ramdac_ibm_rgb528.c"
    "../../src/video/ramdac/vid_ramdac_sc1148x.c"
    "../../src/video/ramdac/vid_ramdac_sc1502x.c"
    "../../src/video/ramdac/vid_ramdac_sdac.c"
    "../../src/video/ramdac/vid_ramdac_stg1702.c"
    "../../src/video/ramdac/vid_ramdac_tkd8001.c"
    "../../src/video/ramdac/vid_ramdac_tvp3026.c"
    "../../src/video/clockgen/vid_clockgen_av9194.c"
    "../../src/video/clockgen/vid_clockgen_icd2047.c"
    "../../src/video/clockgen/vid_clockgen_icd2061.c"
    "../../src/video/clockgen/vid_clockgen_ics1494.c"
    "../../src/video/clockgen/vid_clockgen_ics2494.c"
    "../../src/video/clockgen/vid_clockgen_ics2595.c"
    "../../src/video/clockgen/vid_clockgen_ics90c64a.c"
    "../../src/video/vid_ddc.c"
    "../../src/video/vid_ddc_edid_custom.c"
    "../../src/video/vid_cga.c"
    "../../src/video/vid_cga_comp.c"
    "../../src/video/vid_cga_colorplus.c"
    "../../src/video/vid_cga_compaq_plasma.c"
    "../../src/video/vid_cga_olivetti.c"
    "../../src/video/vid_cga_toshiba_t1000.c"
    "../../src/video/vid_cga_toshiba_t3100e.c"
    "../../src/video/vid_cga_v6355.c"
    "../../src/video/vid_pcjr.c"
    "../../src/video/vid_tandy.c"
    "../../src/video/vid_mda.c"
    "../../src/video/vid_hercules.c"
    "../../src/video/vid_hercules_plus.c"
    "../../src/video/vid_ega.c"
    "../../src/video/vid_ega_render.c"
    "../../src/video/vid_jega.c"
    "../../src/video/vid_vga.c"
    "../../src/video/vid_svga.c"
    "../../src/video/vid_svga_render.c"
    "../../src/video/vid_8514a.c"
    "../../src/video/vid_xga.c"
    "../../src/video/vid_ps55da2.c"
    "../../src/video/vid_ati_eeprom.c"
    "../../src/video/vid_ati28800.c"
    "../../src/video/vid_ati_mach8.c"
    "../../src/video/vid_ati_mach64.c"
    "../../src/video/vid_ati_mach64_accel.c"
    "../../src/video/vid_cl54xx.c"
    "../../src/video/vid_et3000.c"
    "../../src/video/vid_et4000.c"
    "../../src/video/vid_et4000w32.c"
    "../../src/video/vid_ht216.c"
    "../../src/video/vid_oak_oti.c"
    "../../src/video/vid_paradise.c"
    "../../src/video/vid_f82c425.c"
    "../../src/video/vid_ti_cf62011.c"
    "../../src/video/vid_tvga.c"
    "../../src/video/vid_tgui9440.c"
    "../../src/video/vid_mga.c"
    "../../src/video/nv/nv_rivatimer.c"
)

# src/cpu/ - not covered by the subsystem-extraction script (only the 16
# add_library(name OBJECT ...) subdirs were scanned). Listed by hand from
# src/cpu/CMakeLists.txt's unconditional set(); the DYNAREC-only files
# (386_dynarec_ops.c, codegen_timing_*.c) are correctly omitted since this
# build is DYNAREC=OFF.
set(CPU_CORE_SRCS
    "../../src/cpu/cpu.c"
    "../../src/cpu/cpu_table.c"
    "../../src/cpu/fpu.c"
    "../../src/cpu/x86.c"
    "../../src/cpu/808x.c"
    "../../src/cpu/808x_common.c"
    "../../src/cpu/808x_marty_86box.c"
    "../../src/cpu/vx0.c"
    "../../src/cpu/vx0_biu.c"
    "../../src/cpu/386.c"
    "../../src/cpu/386_common.c"
    "../../src/cpu/386_dynarec.c"
    "../../src/cpu/x86_ops_mmx.c"
    "../../src/cpu/x86seg_common.c"
    "../../src/cpu/x86seg.c"
    "../../src/cpu/x86seg_2386.c"
    "../../src/cpu/x87.c"
    "../../src/cpu/x87_timings.c"
    "../../src/cpu/i8080.c"
)

# Vendored math/synthesis libraries - small, no conditionals, safe to glob.
file(GLOB SOFTFLOAT3E_SRCS "../../src/cpu/softfloat3e/*.c" "../../src/cpu/softfloat3e/*.cc")
file(GLOB AYUMI_SRCS "../../src/sound/ayumi/*.c")
file(GLOB ESFMU_SRCS "../../src/sound/esfmu/*.c")
file(GLOB YMFM_SRCS "../../src/sound/ymfm/*.cpp")
# NOT a plain glob: resid-fp/CMakeLists.txt deliberately omits version.cc
# from its own file list (it needs a VERSION macro that's never defined -
# dead/vestigial file). A glob here would silently pull it back in.
file(GLOB RESIDFP_SRCS "../../src/sound/resid-fp/*.cc" "../../src/sound/resid-fp/*.cpp")
list(REMOVE_ITEM RESIDFP_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/../../src/sound/resid-fp/version.cc")

# Our own ESP32 platform layer (replaces src/unix/). esp32_network.c stubs
# the whole src/network/ subsystem (needs libpcap/libslirp - not viable on
# ESP-IDF without a lwIP rewrite, out of scope for this milestone).
set(ESP32_PLATFORM_SRCS
    esp32_main.c
    esp32_plat.c
    esp32_plat_posix.c
    esp32_video.c
    esp32_audio.c
    esp32_input.c
    esp32_hid_xt_table.c
    esp32_tab5_kbd.cpp
    esp32_network.c
    esp32_cdrom_stub.c
    esp32_char_stub.c
    esp32_printer_stub.c
    esp32_ui_stub.c
)

idf_component_register(
    SRCS
        ${BOX86_CORE_SRCS}
        ${CPU_CORE_SRCS}
        ${SOFTFLOAT3E_SRCS}
        ${AYUMI_SRCS}
        ${ESFMU_SRCS}
        ${YMFM_SRCS}
        ${RESIDFP_SRCS}
        ${ESP32_PLATFORM_SRCS}
    INCLUDE_DIRS
        "."
        "../../src/include"
        "../../src/cpu"
        "${CMAKE_CURRENT_BINARY_DIR}/generated_include"
    REQUIRES
        driver
        esp_timer
        esp_psram
        fatfs
        sdmmc
        vfs
        pthread
        esp_lcd
        esp_lcd_st7121
        esp_lcd_touch
        esp_lcd_touch_st7123
        i2c_bus
        m5_tab5_keyboard_component
        esp_driver_i2c
        esp_driver_i2s
        esp_driver_ledc
        esp_driver_gpio
        esp_hw_support
        heap
)

target_compile_definitions(${COMPONENT_LIB} PUBLIC
    MEM_COMPACT_TABLES
    CPPTHREADS
    # Same macro the desktop SDL build defines (src/CMakeLists.txt) to skip
    # Qt-only code (GL3/Vulkan shader config parsing, VM manager CLI args) -
    # applies just as much here, we have neither.
    USE_SDL_UI
    # fdd_audio.c's per-profile config struct (BIOS_VENDOR_COUNT x
    # MAX_SEEK_SAMPLES nested audio_sample_t arrays, x FDD_AUDIO_PROFILE_MAX
    # profiles) is ~93MB of static storage for floppy mechanical-sound
    # effects - not needed for this boot-test milestone, and upstream
    # already has this exact escape hatch (see fdd_audio.h).
    DISABLE_FDD_AUDIO
)

# 86Box's own code has some warnings we don't want to fail the build over
# on a first port (unused params in stubbed-out desktop-only code paths,
# sign-compare in vendored synthesis libs, etc).
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-unused-variable
    -Wno-unused-function
    -Wno-unused-but-set-variable
    -Wno-unused-parameter
    -Wno-sign-compare
    -Wno-missing-field-initializers
    # uint32_t is `unsigned long` on this 32-bit RISC-V ABI (not `unsigned
    # int` like on 64-bit desktop targets), so every %u/sscanf against a
    # uint32_t* mismatches by GCC's strict format-checking - harmless here
    # since both are 32 bits wide on ILP32, but treated as a hard error by
    # -Werror=format upstream. Downgrading to a warning for this first
    # port; the real fix (PRIu32/%lu throughout) is cleanup work for later,
    # not a correctness issue on this target.
    -Wno-error=format
    # Same ABI quirk: int32_t/uint32_t are typedef'd to long/unsigned long
    # here (not int/unsigned int), so passing a local `int len` as an
    # `int32_t *` parameter is a real type mismatch to the compiler even
    # though both are 32-bit two's complement - harmless in practice,
    # same follow-up cleanup as the format fix above applies.
    -Wno-error=incompatible-pointer-types
    # cdrom_image_ccd.c/cdrom_image_viso.c call fseeko64/ftello64 assuming
    # glibc-style LFS availability - this toolchain's newlib doesn't
    # implement them. See esp32_libc_compat.h.
    -include "${CMAKE_CURRENT_SOURCE_DIR}/esp32_libc_compat.h"
)
