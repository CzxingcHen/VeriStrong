# smt-transactional-consistency-artifact
Artifact for the "smt-transactional-consistency" project

## Build (Ubuntu)

- Install required packages

```sh
sudo apt update && sudo apt install g++-12 git pkg-config cmake meson ninja-build libboost-log-dev libboost-test-dev libboost-graph-dev libz-dev libgmp-dev libfmt-dev
```

  - `subprojects/acyclic-minisat` requires [`fmt 10.2.0`](https://fmt.dev/latest/index.html) to record logs, see [fmt:usage](https://fmt.dev/latest/usage.html#building) to install.

- Clone this repo

```sh
git clone --recurse-submodules https://github.com/hengxin/smt-transactional-consistency-artifact
```

- Build with Meson

```sh
# debug build
# gcc 12 is required
CC=gcc-12 CXX=g++-12 meson setup builddir && meson compile -C builddir

# optimized debug build
CC=gcc-12 CXX=g++-12 meson setup builddir-debugoptimized --buildtype=debugoptimized && meson compile -C builddir-debugoptimized

# release build
CC=gcc-12 CXX=g++-12 meson setup builddir-release --buildtype=release && meson compile -C builddir-release
```

## Tests

Test files are located in `src/tests`. To run tests:

```sh
# build first
CC=gcc-12 CXX=g++-12 meson setup builddir && meson compile -C builddir

meson test -C builddir
```

## Histories

Some histories are included in `history`, to check a history:

```sh
./builddir/checker history/15_15_15_1000/hist-00000/history.bincode
# 'accept: true' means no violations are found
```

Dbcop is used to generate histories. For example:

```sh
# clone dbcop from https://github.com/amnore/dbcop
docker-compose up -d -f docker/postgres/docker-compose.yml

# build dbcop...

# generated histories are stored in /tmp/hist
# see 'dbcop genrate --help' and 'dbcop run --help'
dbcop/target/release/dbcop generate -d /tmp/gen -e 2 -n 10 -t 3 -v 2
dbcop/target/release/dbcop run -d /tmp/gen/ --db postgres-ser -o /tmp/hist 127.0.0.1:5432
```

### Note: a bug was detected in `src/solver/pruner.cpp`

```cpp
while (changed) {
  changed = false;
  for (auto constraint : constraints) {
    if (can be pruned) {
      changed = true;
    }
    ...
    if (changed) { // changed was misused here, for a previously pruned contraint can trigger the pruning of all following constraints
      prune constraint
    }
  }
}
```

-------

### Experiment Data

see [Experiment.md](./Experiment.md)