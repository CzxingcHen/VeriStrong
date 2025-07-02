# Artifact and Technical Report for `Fast Verification of Strong Database Isolation`

<!-- see also in [IsoVista](https://github.com/hengxin/IsoVista). -->

## Directory Organization

```plain
/VeriStrong
|-- tech-rpt.pdf            # the accompanying technical report
|-- history                 # histories and benchmarks used in our experiments
|-- veristrong              # our prototype of VeriStrong
|-- README.md
|-- reproduce               # scripts of reproducing experiments
|-- precompiled             # precompiled instances of VeriStrong under Ubuntu 22.04, used for reproducing experiments
`-- tools       
    ├── CobraVerifier       # The Cobra Checker
    ├── PolySI              # The PolySI Checker (modified)
    ├── Viper               # The Viper Checker
    └── dbcop-verifier      # The dbcop Checker
```

## Reuse VeriStrong

### Prerequisite

It is highly recommended to compile VeriStrong under **Ubuntu 22.04**.

```sh
sudo apt update && sudo apt install g++-12 git pkg-config cmake meson ninja-build libboost-log-dev libboost-test-dev libboost-graph-dev libz-dev libgmp-dev libfmt-dev
```

### Build with Meson
```sh
# debug build
# gcc 12 is required
CC=gcc-12 CXX=g++-12 meson setup builddir && meson compile -C builddir

# optimized debug build
CC=gcc-12 CXX=g++-12 meson setup builddir-debugoptimized --buildtype=debugoptimized && meson compile -C builddir-debugoptimized

# release build
CC=gcc-12 CXX=g++-12 meson setup builddir-release --buildtype=release && meson compile -C builddir-release
```

### Usage

```sh
Usage: checker [--help] [--version] [--log-level VAR] [--pruning VAR] [--solver VAR] [--history-type VAR] [--isolation-level VAR] [--measuring-repeat-values] history

Positional arguments:
  history                       History file 

Optional arguments:
  -h, --help                    shows help message and exits 
  -v, --version                 prints version information and exits 
  --log-level                   Logging level [default: "INFO"]
  --pruning                     Do pruning [default: "none"]
  --solver                      Select backend solver [default: "acyclic-minisat"]
  --history-type                History type [default: "dbcop"]
  --isolation-level             Target isolation level [default: "ser"]
  --measuring-repeat-values     Measure the degree of repeated values 
```

## Reproduce Experiments

Since the variants of the backend of VeriStrong (built on top of Minisat) is controlled by macros, 
we provide several pre-compiled versions to reproduce experiments, 
see [`precompiled.zip`](https://github.com/CzxingcHen/VeriStrong/releases/download/precompiled/precompiled.zip).

### fig 5

- executable: `precompiled/builddir-release-fig5`;
- history: `history/fig5/`;
- script: no;

```sh
# for {RH, BL, WH}-1k
checker ./history/fig5/{RH-1k}/hist-00000/history.bincode

# for {tpcc, twitter, rubis}-1k
checker ./history/fig5/{twitter-1k} --history-type cobra
```

see the output of VeriStrong (in `stderr`); for example, 
```sh
[Minimal]
width = 5, count = 1
width = 4, count = 4
width = 1, count = 16
width = 3, count = 13
width = 2, count = 242
```

### fig 10

- executable: `precompiled/builddir-release-veristrong`; 
- history: `history/fig10`;
- script: `reproduce/fig10.py`;

This experiment may take hours due to the inefficiency of baselines.

### fig 11

#### (a) SER

- executable: 
  - VeriStrong: `precompiled/builddir-release-veristrong`; 
  - Cobra(w/o GPU): `tools/CobraVerifier`;
  - dbcop: `tools/dbcop-verifier`;
- history: `history/fig11`;
- script: `reproduce/fig11-ser.py`;

To reproduce Cobra (w/ V100 GPU), see [Cobra's repo](https://github.com/DBCobra/CobraHome);

#### (b) SI

- executable: 
  - VeriStrong: `precompiled/builddir-release-veristrong`; 
  - PolySI: `tools/PolySI`. Note that the source code of PolySI is modified;
  - Viper: `tools/Viper`;
  - dbcop: `tools/dbcop-verifier`;
- history: `history/fig11`;
- script: `reproduce/fig11-si.py`;

### fig 12 

- executable: `precompiled/builddir-release-veristrong`; 
- history: `history/fig12`;
- script: `reproduce/fig12.py`;

Note: fig 12 is the scalability experiment, which requires **about 64GB memory** and may take about 24 hours to run.

### fig 13

- executable: `precompiled/builddir-release-veristrong`; 
- history: `history/fig13-14-15-table1`;
- script: no;

```sh
# for {RH, BL, WH, HD}-10k
checker ./history/fig13-14-15-table1/{RH-10k}/hist-00000/history.bincode --pruning fast

# for {tpcc, twitter, rubis}-10k
checker ./history/fig13-14-15-table1/{twitter-10k} --history-type cobra --pruning fast
```

see the output of VeriStrong (in `stdout`); for example, 
```sh
[timestamp] [thread] [info]    construct time: 58ms               # constructing time
[timestamp] [thread] [info]    prune time: 235ms                  # pruning time
[timestamp] [thread] [info]    solver initializing time: 29ms     # encoding time
[timestamp] [thread] [info]    solve time: 360ms                  # solving time
```

### fig 14

- executable: 
  - VeriStrong: `precompiled/builddir-release-veristrong`; 
  - Variant (i) (w/o H): `precompiled/builddir-release-no-H`;
  - Variant (ii) (w/o H, C) and (iii) (w/o H, C, P): `precompiled/builddir-release-no-HC`;
- history: `history/fig13-14-15-table1`;
- script: `reproduce/fig14.py`;

This experiment may take hours.

### fig 15

- executable: 
  - encode 1: `precompiled/builddir-release-no-HC`;
  - encode 2: `precompiled/builddir-release-no-H`;
  - encode 3: `precompiled/builddir-release-no-H-enc-3`;
  - encode 4: `precompiled/builddir-release-no-H-enc-4`;
- history: `history/fig13-14-15-table1`;
- script: `reproduce/fig15.py`;

This experiment may take hours.

### table 1

- executable: 
  - with H: `precompiled/builddir-release-table1-H`;
  - without H: `precompiled/builddir-release-table1-no-H`;
- history: `history/fig13-14-15-table1`;
- script: no;

```sh
# for {RH, BL, WH, HD}-10k
checker ./history/fig13-14-15-table1/{RH-10k}/hist-00000/history.bincode --pruning fast

# for {tpcc, twitter, rubis}-10k
checker ./history/fig13-14-15-table1/{twitter-10k} --history-type cobra --pruning fast
```

see the output of VeriStrong (in `stderr`); for example, 
```sh
[Monitor]
find_cycle_times: 50
```

### table 2

We shows the collected histories in `history/table2`, with SER violations are under `ser` and SI violations are under `si`.
Note that histories under dbcop (for both SER and SI) include both valid and invalid executions, while those under cobra and polysi are confirmed violations only.

The experiment of table 2 are conducted manually for histories under `cobra` and `polysi`.
For `dbcop` histories, violations can be checked by modifying the script of fig 11 to run checkers.
Users may write additional scripts to parse and interpret the checker outputs.

## Supporting Weak Isolation Levels

To support weak isolation levels, we have integrated VeriStrong with the weak isolation checker [Plume](https://github.com/dracoooooo/Plume) in the [IsoVista](https://github.com/hengxin/IsoVista) system.
