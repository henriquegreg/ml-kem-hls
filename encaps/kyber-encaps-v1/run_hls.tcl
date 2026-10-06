# run_hls.tcl – Vitis HLS synthesis script for Kyber keygen accelerator.
#
# Usage (from the kyber_hls_v2 directory):
#   vitis_hls -f run_hls.tcl
#
# Set KYBER_K to 2, 3, or 4 to select the security level.
# All other parameters are derived from params.h.

# ── User-configurable ──────────────────────────────────────────────────────
if { [info exists ::env(KYBER_K_ENV)] } {
    set KYBER_K $::env(KYBER_K_ENV)
    puts "Encontrada variavel de ambiente! Usando KYBER_K = $KYBER_K"
} else {
    puts "Variante nao definida no sistema. Usando default (3)."
    set KYBER_K 3
}

set TARGET_CLK_NS  3.3    ;# 300 MHz; U50 DSP/routing supports this comfortably
set PART           "xcu50-fsvh2104-2-e"
# ──────────────────────────────────────────────────────────────────────────

set VARIANT     [expr {$KYBER_K == 2 ? "512" : $KYBER_K == 3 ? "768" : "1024"}]
set PROJ_NAME   "kyber${VARIANT}_enc"
set SOL_NAME    "solution1"
set TOP_FUNC    "enc"

# ── Project / solution ────────────────────────────────────────────────────
open_project $PROJ_NAME
set_top $TOP_FUNC

# ── Design source files ───────────────────────────────────────────────────
set CFLAGS "-DKYBER_K=$KYBER_K -O3 -fomit-frame-pointer -Wall -Wextra -Wpedantic -Wmissing-prototypes -Wredundant-decls -Wshadow -Wpointer-arith"

# ── Source files ───────────────────────────────────────────────────────────
add_files enc.c               -cflags $CFLAGS
add_files kem.c               -cflags $CFLAGS
add_files indcpa.c            -cflags $CFLAGS
add_files polyvec.c           -cflags $CFLAGS
add_files poly.c              -cflags $CFLAGS
add_files ntt.c               -cflags $CFLAGS
add_files cbd.c               -cflags $CFLAGS
add_files reduce.c            -cflags $CFLAGS
add_files verify.c            -cflags $CFLAGS
add_files fips202.c           -cflags $CFLAGS
add_files symmetric-shake.c   -cflags $CFLAGS

# ── Testbench ─────────────────────────────────────────────────────────────
#add_files -tb enc_tb.c -cflags "${CFLAGS} -I."

# ── Solution (target device + clock) ──────────────────────────────────────
open_solution $SOL_NAME -flow_target vitis
set_part $PART
create_clock -period $TARGET_CLK_NS -name default

# ── Step 1: C simulation ──────────────────────────────────────────────────
#csim_design -clean -O

# ── Step 2: C synthesis ───────────────────────────────────────────────────
csynth_design

# ── Step 3: RTL co-simulation ─────────────────────────────────────────────
# Uncomment after confirming csim passes to save iteration time.
# cosim_design -O -rtl verilog -trace_level all

# ── Export IP as .xo ───────────────────────────────────────────────────────
export_design -format xo -output "${PROJ_NAME}.xo"

close_project
puts "\n>>> Done. Reports: ${PROJ_NAME}/${SOL_NAME}/syn/report/"