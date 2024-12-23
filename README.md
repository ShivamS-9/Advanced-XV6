# Advanced XV6: Distributed Sorting System & Copy-On-Write Fork 

## Overview

This project implements a **Distributed Sorting System** using the **Merge Sort** algorithm, which is parallelized with threads to handle large datasets efficiently. The project also includes an analysis of **Copy-On-Write (COW)** fork performance in the **xv6** operating system, simulating its behavior in managing page faults during read and write operations.

Additionally, the **LAZY Corp** simulation was carried out to improve file management under high concurrency and implement a **distributed sorting mechanism** that switches between two algorithms: **Distributed Count Sort** and **Distributed Merge Sort**.

## Project Components

### 1. **Distributed Sorting System**

This system is designed to sort large datasets efficiently by parallelizing the merge sort algorithm with threads. It dynamically allocates threads for sorting and merging operations, making use of multiple CPU cores to speed up the process.

#### Features:
- **Thread per Merge Operation:** Each thread handles a chunk of files or a merge operation.
- **Scalable:** The system can handle small, medium, and large datasets effectively by adjusting the number of threads.
- **Memory Usage:** Memory consumption increases with the number of threads and dataset size, optimized through careful thread management.

#### Performance:
- For **small datasets (30 files)**: Execution time of 0.000022 seconds.
- For **medium datasets (500 files)**: Execution time of 0.000276 seconds.
- For **large datasets (4000 files)**: Execution time of 0.002898 seconds.

### 2. **Copy-On-Write Fork in xv6**

The **Copy-On-Write (COW)** fork mechanism was implemented in the **xv6** operating system. COW allows processes to share memory pages initially, copying them only when a write occurs. This mechanism improves memory efficiency by reducing unnecessary memory duplication during forking.

#### Performance:
- **Read-only operations:** 4 page faults on average.
- **Write operations:** 14 page faults on average, as writes trigger COW.

#### Benefits:
- **Memory Efficiency:** Reduces memory usage by sharing pages between parent and child processes.
- **Faster Forking:** The fork process is faster since memory duplication occurs only when needed.
  
#### Areas for Optimization:
- Handling frequent writes to shared pages.
- Optimizing page fault handling to reduce overhead.

### 3. **LAZY Corp File Manager Simulation**

The **LAZY Corp** file manager simulates concurrency management for file operations (READ, WRITE, DELETE) in a system with limited resources. The system processes file access requests with concurrency constraints and handles user cancellations when requests are not processed in a timely manner.

#### Features:
- **Concurrency Management:** Controls access to files based on operation types.
- **User Request Handling:** Users can cancel requests if not processed within a given time.
- **File Locking:** Only one write operation is allowed at a time, and delete operations require exclusive access.


### 4. **LAZY Sorting**

The sorting system uses two algorithms depending on the number of files:
- **Distributed Count Sort** for small datasets.
- **Distributed Merge Sort** for larger datasets.

The system can dynamically switch between these sorting strategies based on the number of files.


### Requirements

- **Operating System:** xv6 or Linux (for testing the COW fork mechanism).
- **Programming Language:** C (for system-level implementations).
- **Libraries:** pthreads (for multi-threading), synchronization mechanisms like mutexes and condition variables.

## To Run

1. COW can be directly run from XV6.
2. For lazy read write compile using: `gcc -pthread -o lazy lazyrw.c`
3. For lazy sort: `gcc lazy-sort.c -pthread`
