# CS 223P – Transaction Concurrency Control Project

Multi-threaded transaction processing layer built on RocksDB, implementing and comparing Optimistic Concurrency Control (OCC) and Conservative Two-Phase Locking (Conservative 2PL).

---

## Dependencies

- **RocksDB** (and its compression dependencies): `librocksdb`, `libsnappy`, `libz`, `libbz2`, `liblz4`, `libzstd`
- **GCC / Clang** with C++20 support
- **pthreads**

On Ubuntu/Debian:
```bash
sudo apt-get install librocksdb-dev libsnappy-dev zlib1g-dev libbz2-dev liblz4-dev libzstd-dev
```

---

## Build

```bash
make
```

Produces the `txn_system` binary. To clean:
```bash
make clean
```

---

## Running the Workloads

```bash
./txn_system <input_file> <workload_number>
```

- `<input_file>` — path to the initial data file (INSERT/END format)
- `<workload_number>` — `1` for the transfer workload, `2` for the TPC-C-like workload

**Workload 1 (Transfer):**
```bash
./txn_system workloads/input1.txt 1
```

**Workload 2 (NewOrder + Payment):**
```bash
./txn_system workloads/input2.txt 2
```

Each run automatically executes a full experiment suite comparing OCC and 2PL.

---

## Setting Number of Threads and Contention Level

Thread count and contention parameters are configured directly in `src/main.cpp` via the `run_experiment()` calls. Each call has the signature:

```cpp
run_experiment(label, runner, num_threads, txns_per_thread, hot_prob, hot_frac);
```

| Parameter | Description |
|---|---|
| `num_threads` | Number of concurrent worker threads |
| `txns_per_thread` | Transactions each thread executes |
| `hot_prob` | Probability (0.0–1.0) that a key is selected from the hot set |
| `hot_frac` | Fraction of the keyspace that constitutes the hot set (e.g., 0.1 = 10%) |

The default experiment suite varies threads (1, 2, 4, 8) at fixed contention (50% hot probability, 10% hot fraction), then varies contention (0%, 30%, 70%, 100%) at fixed thread count (4).

To run a single custom configuration, add or modify a `run_experiment` call in `main()` and recompile.

---

## Output Format

For each configuration, the program prints:

```
=== WL1 | threads=4 hot_prob=0.5 ===
  [OCC]
    Committed  : 800
    Retries    : 42 (5.0%)
    Throughput : 12345.6 txns/sec
    Avg Resp   : 0.32 ms
    Max Resp   : 1.84 ms
    P50 Resp   : 0.28 ms
    P95 Resp   : 0.91 ms
  [2PL]
    ...
    [NewOrder] count=412 avg=0.29 ms
    [Payment]  count=388 avg=0.35 ms
```

Per-transaction-type breakdown is shown when a workload has more than one transaction type (Workload 2).
