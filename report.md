# Part 1 Distributed Sorting System Performance Report

## 1. **Implementation Analysis**

In this report, we describe the implementation of a distributed sorting system based on the **Merge Sort** algorithm, which is parallelized using threads. Each thread is responsible for sorting a chunk of files or performing a merge operation.

### **Approach: Thread per Merge Operation**
We chose to implement the distributed merge sort by creating a thread per merge operation. This approach was chosen to leverage the power of parallelism for handling large datasets efficiently.

#### **Why this Approach?**
- **Parallelism:** The system creates threads to perform sorting and merging operations concurrently. This allows us to take advantage of multiple CPU cores, improving sorting performance, especially for large datasets.
- **Scalability:** As the dataset size increases, the system dynamically allocates threads for different chunks and merges. This approach scales well as it can handle larger datasets by distributing the load across multiple threads and nodes.

#### **Pros:**
- **Increased Performance:** Parallelism speeds up the sorting process, particularly for large datasets.
- **Scalability:** The system is designed to scale with the increase in dataset size, using multiple threads for chunk sorting and merging.
- **Efficient Resource Utilization:** Nodes are allocated dynamically based on the workload, ensuring that system resources are used efficiently.

#### **Cons:**
- **Thread Overhead:** Creating and managing many threads introduces overhead, which can affect performance, especially for smaller datasets where parallelism may not provide much benefit.
- **Memory Usage:** Each thread requires memory for its stack, and the system’s memory footprint increases with the number of threads. This could be problematic on systems with limited memory.
- **Synchronization Overhead:** Mutex locks are used to synchronize the merge operations and manage active nodes, which can introduce delays when multiple threads access shared resources.

## 2. **Execution Time Analysis**

To assess the performance of the distributed sorting system, we measured the execution time of the **Distributed Merge Sort** for different dataset sizes: small, medium, and large.

### **Dataset Sizes:**
- **Small Dataset (30 files):** Execution Time: 0.000022 seconds
- **Medium Dataset (500 files):** Execution Time: 0.000276 seconds
- **Large Dataset (4000 files):** Execution Time: 0.002898 seconds.

### **Execution Time Observations:**
- The **distributed merge sort** outperforms non-parallel sorting methods for larger datasets.
- **Thread management** and **merging** stages dominate the execution time, but they scale better with increasing dataset sizes.
- **Scalability:** As the dataset grows, the sorting time increases, but the rate of increase is slower due to parallelism.

## 3. **Memory Usage Overview**

The memory usage of the system depends on both the dataset size and the number of threads created. Below is an overview of memory consumption for both small and large datasets.

### **Small Dataset:**
- Memory consumption is relatively low, mainly allocated for the array of `File` structs and thread stacks. The memory used for sorting is manageable, as fewer threads are required.

### **Large Dataset:**
- Memory usage increases due to the larger dataset and the increased number of threads. Each thread consumes memory for its stack, and the merge process requires temporary storage for chunks of data.
- The system’s memory consumption is proportional to the dataset size and the number of threads, making it essential to optimize thread management to minimize memory overhead.

## 4. **Summary**

### **Performance Summary:**
- The **Distributed Merge Sort** shows significant performance improvements as dataset sizes increase. The parallelism in sorting and merging allows it to scale efficiently and reduce execution time for large datasets.
- For **small datasets**, the overhead of thread creation and synchronization might negate the benefits of parallelism, but as the dataset grows, the system's ability to handle multiple threads improves execution times.
  
### **Memory Usage Summary:**
- **Distributed Merge Sort** uses more memory than **Distributed Count Sort**, particularly for large datasets, due to the need for storing temporary chunks and managing multiple threads. Memory usage grows with dataset size and thread count, but careful management of resources can mitigate excessive memory consumption.



### **Optimizations for Large Datasets:**
- **Thread Pooling:** Instead of creating new threads for each task, a thread pool can be used to reduce thread creation overhead.
- **Optimized Merging:** The merging process can be optimized to minimize memory allocation and reduce the impact of synchronization overhead.
- **Adaptive Chunking:** Dynamically adjusting chunk sizes based on the dataset characteristics can optimize both memory usage and processing time.
- **Memory Management:** By implementing better memory reuse techniques and reducing the number of temporary allocations, memory usage can be kept in check.

# Part 2 Copy-On-Write (COW) Fork Performance Analysis

## 1. **Page fault frequency**

- Created 2 different tests for this. One for read only and other for that modify memory.
- For read only part, few page_faults were recorded which is expected.
- For the other case, a significant number of page faults occurred because the xv6 COW mechanism is designed to share pages as read-only and only copy them upon write. As a result, every write operation triggers a page fault, leading to the allocation of a new page for the child process.
- The result was analysed by using a global counter and running cowtest.
- For given cowtest, an anerage of 4 page faults were counted during read only and 14 for write part.

## 2. Analysis
### Benefits:

- **Memory Efficiency**: COW allows parent and child processes to share memory initially, reducing memory usage. A copy is made only when a process modifies a page, conserving resources.

- **Faster Forking:** The fork process is faster since it doesn’t duplicate memory until necessary, reducing overhead.

- **Resource Optimization****: COW is particularly beneficial in environments with many processes performing read-only operations, as it avoids unnecessary memory duplication.

### Areas for Optimization:

- **Handling Frequent Writes**: Multiple page faults for frequent writes can increase overhead. Optimizing write handling could reduce performance hits.

- **Granularity of Copying**: Copying entire pages may be inefficient; more granular copying could save memory.

- **Shared Memory:** COW may not be ideal for shared memory regions, so improving handling for these cases could enhance efficiency.

- **Page Fault Handling**: Further optimization in handling page faults could reduce latency and improve performance.