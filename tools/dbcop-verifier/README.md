# DBCop

## Usage

1.  Clone it.
```
    git clone git@gitlab.math.univ-paris-diderot.fr:ranadeep/dbcop.git
```

2.  Compile and install using `cargo` and run.
    Make sure `~/.cargo/bin` is in your system path.
```
    cd dbcop
    dbcop install --path .
    dbcop --help
```
---

There are a few `docker-compose` files in `docker` directory to create docker cluster.

The workflow goes like this,

1. Generate a bunch of histories to execute on a database.
2. Execute those histories on a database using provided `traits`. (see in `examples`).
3. Verify the executed histories for `--cc`(causal consistency), `--si`(snapshot isolation), `--ser`(serialization).  

# Build on Ubuntu 22

```sh
# 0. install dependencies
sudo apt install libssl-dev libclang-dev
# cargo and rust >= 1.70.0, see https://doc.rust-lang.org/cargo/getting-started/installation.html

# 1. update Cargo.toml
# done in this repo.

# 2. build
cargo build --release


```