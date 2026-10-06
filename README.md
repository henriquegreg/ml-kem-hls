# PQC ML-KEM - HLS Accelerators on Alveo U50

## Overview

This repository contains the design, implementation and benchmarking of High-Level Synthesis (HLS) accelerators for the post-quantum cryptography (PQC) key encapsulation mechanism ML-KEM (CRYSTALS-Kyber), targeting the **Xilinx Alveo U50** data center accelerator card. Each of the three KEM operations (key generation, encapsulation and decapsulation) is synthesized as an independent kernel, exported as a Vitis object (`.xo`) and linked into a ready-to-load FPGA binary (`.xclbin`). The host communicates with the kernels through XRT (Xilinx Runtime) and the data buffers are placed in the card's HBM.

The repository supports and provides designs for three security levels: Kyber-512, Kyber-768, and Kyber-1024. For every operation and every security level, **three design versions (`v1`, `v2`, `v3`)** are provided, each one applying a progressively more aggressive set of HLS optimizations to the same underlying algorithm. This yields 3 operations x 3 security levels x 3 versions = **27 kernels**, all of which are shipped pre-built.

The algorithmic source is the C reference implementation of CRYSTALS-Kyber (`pq-crystals/kyber`, `ref/` directory), modified only with HLS directives (`#pragma HLS ...`), static loop bounds and the top-level wrappers required for synthesis. Randomness is never generated inside the kernels: the host supplies the "coins" (derandomized API), which makes every execution deterministic and directly comparable against the software reference through Known Answer Tests (KAT).

## Repository Structure

The repository is divided into four main directories:

1. **`keygen/`**: Contains the HLS sources, build scripts and pre-built binaries of the key generation kernel (three versions).


2. **`encaps/`**: Contains the HLS sources, build scripts and pre-built binaries of the encapsulation kernel (three versions).


3. **`decaps/`**: Contains the HLS sources, build scripts and pre-built binaries of the decapsulation kernel (three versions).


4. **`benchmark/`**: Contains the Jupyter Notebook, helper module and KAT seeds used to benchmark and validate all 27 kernels against the software reference running on the host CPU.



```
.
├── keygen/
│   ├── kyber-keygen-v1/
│   ├── kyber-keygen-v2/
│   └── kyber-keygen-v3/
├── encaps/
│   ├── kyber-encaps-v1/
│   ├── kyber-encaps-v2/
│   └── kyber-encaps-v3/
├── decaps/
│   ├── kyber-decaps-v1/
│   ├── kyber-decaps-v2/
│   └── kyber-decaps-v3/
└── benchmark/
    ├── Benchmark.ipynb
    ├── benchmark_utils.py
    ├── keygen_seeds.txt
    └── enc_seeds.txt
```

---

### 1. `keygen/`, `encaps/` and `decaps/` (HLS Kernels)



The three operation folders share exactly the same layout. Each one contains one subfolder per design version (`kyber-<op>-v1`, `-v2`, `-v3`), and every version folder is **self-contained**: it has all the C sources, the build scripts and the generated binaries for the three security levels.

* **Top-level HLS function and header**: The entry point synthesized by Vitis HLS. It declares the AXI interfaces, copies the inputs from global memory (HBM) into local buffers, calls the corresponding Kyber routine, and writes the results back.

| Folder | Source / Header | Top function | Arguments (in kernel call order) |
|---|---|---|---|
| `keygen/` | `keygen.c`, `keygen.h` | `keygen` | `coins` (in, 64 B), `pk` (out), `sk` (out) |
| `encaps/` | `enc.c`, `enc.h` | `enc` | `pk` (in), `coins` (in, 32 B), `ct` (out), `ss` (out) |
| `decaps/` | `dec.c`, `dec.h` | `dec` | `ss_out` (out, 32 B), `ct_in` (in), `sk_in` (in) |

Every argument is an `m_axi` master port (base address passed at runtime via the AXI4-Lite control interface), and each one is placed in its own bundle (`gmem_coins`, `gmem_pk`, `gmem_sk`, `gmem_ct`, `gmem_ss`) so that HLS can issue independent AXI transactions in parallel. The return value / `ap_start` / `ap_done` handshake is mapped to the `control` AXI4-Lite bundle.

* **Kyber components (`kem.c`, `indcpa.c`, `polyvec.c`, `poly.c`, `ntt.c`, `cbd.c`, `reduce.c`, `verify.c`)**: Original Kyber reference C files, annotated with HLS pragmas. They implement, respectively, the CCA-secure KEM layer, the CPA-secure public-key encryption layer (matrix generation, packing/unpacking), vector-of-polynomial arithmetic, single-polynomial arithmetic and (de)compression, the Number Theoretic Transform, the centered binomial distribution sampler, Montgomery/Barrett modular reduction, and constant-time comparison/selection.


* **`fips202.c`, `fips202.h`, `symmetric-shake.c`, `symmetric.h`**: Keccak / SHA-3 / SHAKE implementation and the symmetric-primitive glue used by Kyber (hashes H and G, XOF and PRF). The Keccak loops are restructured with static bounds so that HLS can pipeline them.


* **`params.h`**: Kyber parameter set. The security level is selected at compile time through the `KYBER_K` macro (`2` = Kyber-512, `3` = Kyber-768, `4` = Kyber-1024). It also defines HLS-specific constants that provide static loop bounds (`GEN_MATRIX_MAX_EXTRA_BLOCKS`, `KECCAK_MAX_ABSORB_BLOCKS`, `KECCAK_MAX_SQUEEZE_BLOCKS`), which are required by Vitis HLS to bound the rejection-sampling and Keccak loops.


* **`randombytes.h`**: Declaration kept for compatibility with the reference code. The kernels never call it, since the coins are provided by the host.


* **`run_hls.tcl`**: Vitis HLS script that creates the project, adds the sources, sets the target part (`xcu50-fsvh2104-2-e`) and clock (3.3 ns, i.e. 300 MHz), runs C synthesis and exports the kernel as `kyber<512|768|1024>_<op>.xo`. The security level is chosen through an environment variable (see [How to Recompile Hardware](#how-to-recompile-hardware)).


* **`Makefile`**: Automation of the full flow. Targets `kyber<N>_<op>.xo` call Vitis HLS and targets `kyber<N>_<op>.xclbin` call `v++` to link the kernel against the `xilinx_u50_gen3x16_xdma_5_202210_1` platform.


* **`connectivity.cfg`** (**`v3` only**): `v++` linker configuration that maps each AXI bundle to a different HBM pseudo-channel, so that the input and output buffers do not compete for the same memory port.

| Kernel | `connectivity.cfg` mapping |
|---|---|
| `keygen` | `coins` -> HBM[0], `pk` -> HBM[1], `sk` -> HBM[2] |
| `enc` | `pk` -> HBM[0], `coins` -> HBM[1], `ct` -> HBM[2], `ss` -> HBM[3] |
| `dec` | `ct` -> HBM[0], `sk` -> HBM[1], `ss` -> HBM[2] |

For `v1` and `v2` no configuration file is used and the platform defaults apply. The `v3` decapsulation configuration additionally sets the Vivado implementation strategy `Congestion_SpreadLogic_high`. The file contains a comment indicating that this line must be **commented out when building Kyber-768**.


* **Pre-built binaries (`.xo` and `.xclbin`)**: One pair per security level (`kyber512_<op>`, `kyber768_<op>`, `kyber1024_<op>`). The `.xo` is the synthesized HLS kernel and the `.xclbin` is the FPGA binary loaded by XRT. These are the files consumed by the benchmark notebook.



#### Design versions

The three versions differ **only in HLS directives and data-movement code**; the cryptographic algorithm is identical, and all of them pass the same KAT check.

| Version | Main characteristics |
|---|---|
| **`v1`** (baseline) | Direct port of the reference code. Inputs/outputs are copied between HBM and local buffers with byte-wise `PIPELINE II=1` loops and 16-beat AXI bursts. The NTT / inverse NTT keep the original nested `len / start / j` loop structure with cyclic array partitioning (factor 4). Vector loops over `K` only carry `LOOP_TRIPCOUNT` hints. |
| **`v2`** (loop restructuring) | The NTT and inverse NTT are rewritten as a flat "7 layers x 128 butterflies" pipelined loop (`PIPELINE II=1`, `DEPENDENCE inter false`, partition factor 2). The loops over the `K` polynomials in `polyvec.c` are fully unrolled, and the matrix-generation loop in `indcpa.c` is unrolled so that the SHAKE-128 sampling of different matrix elements can run in parallel. One Keccak pipeline is relaxed to `II=2`. `kem.c` is adjusted for HLS in the encapsulation and decapsulation kernels. Higher LUT usage than `v1`. |
| **`v3`** (memory + partitioning) | Builds on `v2`. Host transfers use `memcpy`, which HLS maps to AXI burst transfers, with longer bursts (64 beats for `keygen`, 256 beats for `enc`/`dec`, and `max_widen_bitwidth=512` on `enc`). Adds `connectivity.cfg` to spread buffers across HBM pseudo-channels. NTT partition factor raised to 4 with `DEPENDENCE intra false`, additional `ARRAY_PARTITION` / `DEPENDENCE` pragmas in `cbd.c`, `poly.c` and `polyvec.c`, and a tighter pipeline interval (`II=4` -> `II=1`) in `poly.c`. |

#### Kernel LUT usage

LUT counts taken from the synthesis/implementation reports (these are the values used as the *area* term of the area-time product in the benchmark notebook).

| Kernel | Kyber-512 v1 / v2 / v3 | Kyber-768 v1 / v2 / v3 | Kyber-1024 v1 / v2 / v3 |
|---|---|---|---|
| `keygen` | 35,634 / 42,104 / 54,111 | 33,776 / 45,553 / 52,271 | 33,722 / 50,105 / 52,583 |
| `enc` | 51,349 / 67,746 / 73,276 | 58,691 / 71,896 / 73,006 | 58,823 / 78,199 / 72,725 |
| `dec` | 63,478 / 79,438 / 67,004 | 71,234 / 83,817 / 77,207 | 71,351 / 90,161 / 76,741 |

---

### 2. `benchmark/` (Benchmark and Validation)



This folder contains everything required to measure and validate the 27 pre-built kernels against the software reference.

* **`Benchmark.ipynb`**: Jupyter Notebook that runs the whole experiment end to end. For every security level (512/768/1024), operation (`keygen`, `enc`, `dec`) and hardware version (`v1`, `v2`, `v3`) it measures time, clock cycles, power, throughput, energy and area-time product, and compares them against the C reference implementation running on the host CPU. The notebook is organized in the following sections:

  1. **Global configuration**: Kyber parameters (key/ciphertext sizes), list of versions, `get_hw_cfg()` (maps variant/operation/version to the `.xclbin` path), `RUNS` (timed executions per configuration, default `1000`), `DEVICE_INDEX` (FPGA card index) and the `KERNEL_LUTS` table.
  2. **Compilation of the software reference**: builds one shared library per security level (`kyber512_ref.so`, `kyber768_ref.so`, `kyber1024_ref.so`) from the Kyber reference sources with `gcc -O3` and loads them through `ctypes`.
  3. **Input generation**: random coins, public key, ciphertext and expected shared secret for every operation.
  4. **Software benchmark**: times each CPU call and converts it to cycles using the CPU frequency.
  5. **Hardware benchmark**: loads each `.xclbin` through `pyxrt`, runs a discarded warm-up execution, and then times `RUNS` executions on two time bases: **total** (host-to-device transfer + launch + execution + device-to-host transfer) and **exec** (kernel execution only).
  6. **Consolidated results table**: one row per (variant, operation, version) with mean +/- 95% confidence interval, min-max and relative error, plus speedups of each hardware version over software (cycles, time, throughput and energy).
  7. **Correctness check (KAT)**: compares hardware and software outputs byte by byte (details below).
  8. **Export**: writes the results to Excel and CSV.


* **`benchmark_utils.py`**: Helper module imported by the notebook.
  * `get_clock_mhz()`: reads the kernel clock frequency from an `.xclbin` with `xclbinutil`.
  * `get_cpu_mhz()`: reads the maximum CPU frequency from `sysfs` / `/proc/cpuinfo`.
  * `SensorSampler`: background thread that samples power and temperature during a run. For the FPGA it reads the card's `hwmon` entries (`power1_input`, `temp1_input`, Xilinx vendor ID `0x10ee`); for the CPU it uses Intel RAPL energy counters and the thermal zone.
  * `load_kat_seeds()`: loads the KAT seed files.


* **`keygen_seeds.txt`**: 100 KAT seeds of 64 bytes (coins for key generation).


* **`enc_seeds.txt`**: 100 KAT seeds of 32 bytes (coins for encapsulation).

#### Correctness check

For every version and security level, and for up to 100 KAT vectors, the notebook runs the operation on the FPGA and on the software reference with identical inputs and requires an exact match:

* **Key generation**: hardware `pk` and `sk` must be identical to the reference.
* **Encapsulation**: hardware `ct` and `ss` must be identical to the reference.
* **Decapsulation**: hardware `ss` must be identical to the shared secret produced by the reference encapsulation.

#### Generated outputs

All files are written to the folder from which the notebook is executed (`benchmark/`):

* **`benchmark_kyber_results.xlsx`**: consolidated table with grouped column headers.
* **`benchmark_kyber_results.csv`**: same table with flattened headers (`group | metric`).
* **`benchmark_kyber_raw_results.csv`**: raw, full-precision results (one row per platform / version / variant / operation), convenient for plotting.

---

## Requirements and Dependencies

**To run the benchmark (pre-built binaries):**

* Host machine with an **Alveo U50** card installed, with the `xilinx_u50_gen3x16_xdma_5_202210_1` platform (deployment shell) flashed.
* Xilinx Runtime (XRT) matching the deployment platform, including the `pyxrt` Python bindings and the `xclbinutil` tool (`source /opt/xilinx/xrt/setup.sh`).
* Python 3 with Jupyter, `numpy`, `pandas` and `openpyxl` (Excel export). `psutil` is optional (CPU temperature fallback).
* `gcc` to build the software reference libraries.
* The **CRYSTALS-Kyber reference implementation** ([`pq-crystals/kyber`](https://github.com/pq-crystals/kyber)), cloned so that its `ref/` folder is reachable at `../../kyber/ref` from the `benchmark/` folder (see below).
* (Optional) Read access to `/sys/class/powercap/intel-rapl/` for CPU power readings. Without it, the software power and energy columns are reported as `NaN`.

**To rebuild the hardware (optional):**

* Xilinx Vitis (including Vitis HLS and `v++`) with Alveo U50 platform files installed.
* `make`.

## How to Run

1. **Clone the repositories:**
The notebook looks for the Kyber reference code at `../../kyber/ref` relative to `benchmark/`, so clone both repositories as siblings:

```bash
git clone https://github.com/pq-crystals/kyber.git
git clone https://github.com/henriquegreg/ml-kem-hls.git
```

The resulting layout must be:

```
<workspace>/
├── kyber/ref/        <- software reference
└── ml-kem-hls/
    └── benchmark/    <- run the notebook from here
```

Alternatively, edit `REF_DIR` in the global configuration cell of the notebook.


2. **Set up the environment:**

```bash
source /opt/xilinx/xrt/setup.sh
xbutil examine          # confirm that the U50 card is visible
```


3. **Start Jupyter from the `benchmark/` directory** (all paths in the notebook are relative to it):

```bash
cd ml-kem-hls/benchmark
jupyter notebook Benchmark.ipynb
```


4. **Configure the experiment (optional):** In the *Global Configurations* cell you can change `RUNS` (default `1000`), `DEVICE_INDEX` (default `0`) and `VERSIONS` (to benchmark only a subset of `v1`, `v2`, `v3`). Missing `.xclbin` files are skipped with a message.


5. **Run all cells.** The notebook will compile the software libraries, benchmark the CPU, load each `.xclbin` on the FPGA and benchmark it, print the consolidated table, run the KAT correctness check and export the results. The benchmark cell is the longest-running one, since it executes `RUNS` runs for every variant, operation and version.

## How to Recompile Hardware

If you modify the HLS sources of any kernel, rebuild it from inside the corresponding version folder (for example `encaps/kyber-encaps-v3/`):

1. Make sure the Vitis environment is loaded (`source <Vitis_install_dir>/settings64.sh`) and that the Alveo U50 platform is installed.


2. Synthesize the kernel(s) with Vitis HLS and export the `.xo`:

```bash
make xo                       # all three security levels
make kyber768_enc.xo          # a single security level
```

The environment variable that selects the security level is `KYBER_K_ENV` (`2` = 512, `3` = 768, `4` = 1024), and the Makefile sets it for you. To call the script manually:

```bash
KYBER_K_ENV=4 vitis_hls -f run_hls.tcl
```

Synthesis reports are generated in `kyber<N>_<op>/solution1/syn/report/`.

> **Note for `keygen/`:** the `run_hls.tcl` scripts in `keygen/` read the variable `KYBER_K` (not `KYBER_K_ENV`). To select the security level manually, run `KYBER_K=<2|3|4> vitis_hls -f run_hls.tcl`.


3. Link the kernel and generate the FPGA binary with `v++`:

```bash
make                          # builds all three .xclbin files of this folder
make kyber768_enc.xclbin      # a single security level
```

This step runs `v++ -l -t hw` against the `xilinx_u50_gen3x16_xdma_5_202210_1` platform and can take several hours per binary. In the `v3` folders `connectivity.cfg` is picked up automatically. When building **Kyber-768 decapsulation `v3`**, comment out the `[vivado]` strategy line in `connectivity.cfg` first.


4. The resulting `kyber<N>_<op>.xclbin` files are already in the location expected by `benchmark/Benchmark.ipynb` (`../<op>/kyber-<op>-v<X>/kyber<N>_<op>.xclbin`). Re-run the notebook to benchmark and validate the new binaries.


5. To clean the build products of a folder:

```bash
make clean
```

If you change the HLS resource usage of a kernel, update the corresponding entry in the `KERNEL_LUTS` dictionary of the notebook so that the area-time product remains consistent.