# CS 223P – Transaction Concurrency Control Project

This project implements a multi-threaded transaction processing layer built on top of RocksDB. The system supports two concurrency control algorithms: Optimistic Concurrency Control (OCC) and Conservative Two-Phase Locking (Conservative 2PL).

The goal of the project is to compare how these two approaches perform under different levels of concurrency and contention. The program runs experiments using multiple worker threads and simulated workloads, and then reports statistics such as throughput, retries, and response times.

------------------------------------------------------------

Dependencies

The following libraries are required to build and run the project:

- RocksDB
- Compression libraries used by RocksDB (snappy, zlib, bz2, lz4, zstd)
- GCC or Clang with C++20 support
- pthreads

On Ubuntu or Debian systems, you can install the dependencies with:

sudo apt-get install librocksdb-dev libsnappy-dev zlib1g-dev libbz2-dev liblz4-dev libzstd-dev

On Mac:

brew install rocksdb snappy lz4 zstd zlib bzip2

------------------------------------------------------------

Building the Project

To compile the program, run:

make

This will produce the executable called:

transaction_system

To remove compiled files and rebuild the project, run:

make clean

------------------------------------------------------------

Running the Program

The program takes two arguments:

./transaction_system <input_file> <workload_file>

<input_file> is the path to the initial data file (INSERT/END format).
<workload_file> is the path to the workload definition file. The program parses this file to determine the transaction types and operations to run. The workload filename (without extension) is used automatically to name the output directory and label results.

Workload 1: Transfer workload

Example:
./transaction_system Inputs/input1.txt Workloads/workload1.txt

Workload 2: TPC-C style workload with NewOrder and Payment transactions

Example:
./transaction_system Inputs/input2.txt Workloads/workload2.txt

Each run automatically executes a set of experiments comparing OCC and 2PL. Results are saved as CSV files under the Results/ directory.

------------------------------------------------------------

Plotting Results

After running the program, generate plots from the CSV results using:

python3 plot_results.py <results_folder>

To plot all workloads at once:

python3 plot_results.py Results

To plot a specific workload:

python3 plot_results.py Results/workload1

Plots are saved to a plots/ subdirectory inside each workload folder. Requires Python 3 with pandas and matplotlib installed:

pip install -r requirements.txt

------------------------------------------------------------

Configuring Threads and Contention

Thread count and contention settings are configured in src/main.cpp. Experiments are started using the function:

run_benchmark(label, runner, num_threads, txns_per_thread, hot_prob, hot_frac);

Parameter descriptions:

num_threads       Number of worker threads running concurrently
txns_per_thread   Number of transactions each thread executes
hot_prob          Probability (0.0–1.0) that a key is selected from the hot set
hot_frac          Fraction of the keyspace considered "hot" (for example 0.1 = 10%)

The default experiment setup varies the number of threads (1, 2, 4, 8) while keeping contention fixed at 0.5, and then varies contention levels (0%, 25%, 50%, 75%, 100%) while keeping the number of threads fixed at 4.

If you want to run a custom configuration, modify or add a run_benchmark call in main() and recompile.

------------------------------------------------------------

Program Output

For each experiment configuration, the program prints statistics for both concurrency control algorithms.

Example output:

=== workload1 | threads=4 hot_prob=0.5 ===
  [OCC]
    Committed  : 800
    Retries    : 42 (5.0%)
    Throughput : 12345.6 transactions/sec
    Avg Resp   : 0.32 ms
    Max Resp   : 1.84 ms
    P50 Resp   : 0.28 ms
    P95 Resp   : 0.91 ms
  [2PL]
    ...

The output includes:

- Number of committed transactions
- Retry count and retry rate percentage
- Throughput in transactions per second
- Response time statistics (average, max, p50, p95)

For Workload 2, the program also prints per-transaction-type statistics (average response time and count).
