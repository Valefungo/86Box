#
# 86Box    A hypervisor and IBM PC system emulator that specializes in
#          running old operating systems and software designed for IBM
#          PC systems and compatibles from 1981 through fairly recent
#          system designs based on the PCI bus.
#
#          This file is part of the 86Box distribution.
#
#          CMake build script.
#
# Authors: David Hrdlička, <hrdlickadavid@outlook.com>
#          Jasmine Iwanek, <jriwanek@gmail.com>
#
#          Copyright 2020-2021 David Hrdlička.
#          Copyright 2024      Jasmine Iwanek.
#

add_library(chipset OBJECT
    82c100.c
    acc2036.c
    acc2168.c
    cs8220.c
    cs8230.c
    ali1429.c
    ali1435.c
    ali1489.c
    ali6117.c
    ali1409.c
    compaq.c
    compaq_386.c
    contaq_82c59x.c
    cs4031.c
    grid1520.c
    gc100.c
    headland.c
    ims8848.c
    intel_82335.c
    intel_420ex.c
    intel_4x0.c
    intel_sio.c
    isa486c.c
    ../ioapic.c
    laserxt.c
    neat.c
    olivetti_eva.c
    opti283.c
    opti291.c
    opti391.c
    opti495.c
    opti498.c
    opti499.c
    opti602.c
    opti822.c
    opti895.c
    philips.c
    sanyo.c
    scamp.c
    scat.c
    sis_85c310.c
    sis_85c4xx.c
    sis_85c496.c
    sl82c461.c
    stpc.c
    via_vt82c49x.c
    via_vt82c505.c
    umc_8886.c
    umc_hb4.c
    vl82c480.c
    wd76c10.c
)
