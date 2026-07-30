cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(86Box VERSION 7.0)

# Temporary (2026-07-30): Kconfig's COMPILER_OPTIMIZATION choice tops out at
# -O2 ("Perf") - no built-in -O3 option. Appending it here overrides the -O2
# Kconfig already added, since GCC takes the last -O flag on the command
# line. Part of an explicit user-requested -Og vs -O2 vs -O3 comparison
# (an earlier, unrelated session reportedly saw -O2 run ~20x *slower* than
# -Og on real hardware - surprising and not yet explained - so this is a
# re-test, not an assumption that higher -O is better here). Remove this
# line (or comment it out) to go back to plain Kconfig-selected -O2.
# -O3 tried and reverted (2026-07-30): links fine on code size alone, but
# blows the internal-SRAM budget by ~160 bytes (bss overflow), and the
# linker's --enable-non-contiguous-regions handling of that overflow
# segfaults instead of erroring cleanly (a known fragility, not something
# fixable here). -O2 (via Kconfig's COMPILER_OPTIMIZATION_PERF) is the
# practical ceiling on this target for now.
# idf_build_set_property(COMPILE_OPTIONS "-O3" APPEND)
