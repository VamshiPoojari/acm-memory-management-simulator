# Design and Implementation of a Memory Management Simulator

## 1. Overview
This simulator models a complete Operating System memory management subsystem. It provides a user-space environment to experiment with dynamic memory allocation, virtual-to-physical address translation, and multi-level cache performance.

The system is designed with a modular pipeline where every memory access must pass through translation and caching layers before reaching the simulated physical memory.

---

## 2. System Architecture

### 2.1 The Memory Access Pipeline
The simulator enforces a strict "OS-inspired" flow for every memory request:
1. **Virtual Address Generation**: User requests an access at a virtual offset.
2. **TLB Lookup**: A small, fast cache (4 entries) is checked for the page mapping.
3. **Page Table Lookup**: If TLB misses, the Page Table is consulted.
4. **Page Fault Handling**: If the page is not in memory, a frame is allocated (potentially triggering eviction via FIFO/LRU).
5. **Physical Address Formation**: `(Frame Number * Page Size) + Offset`.
6. **L1 Cache Access**: Direct-mapped lookup.
7. **L2 Cache Access**: Checked on L1 miss; promotes data to L1 on hit.
8. **Physical Memory Access**: Final fallback if all caches miss.

---

## 3. Physical Memory Management

### 3.1 Data Structures
Memory is represented as a `std::vector` of `MemoryBlock` structures:
- `start`: Starting byte address.
- `size`: Size of the block in bytes.
- `free`: Boolean flag indicating availability.
- `id`: Unique identifier for allocated blocks.

### 3.2 Allocation Strategies
The simulator implements three primary algorithms to find a suitable `MemoryBlock`:
- **First Fit**: Scans from the beginning and picks the first block that is large enough. (Fastest)
- **Best Fit**: Scans the entire list to find the smallest block that fits the request. (Minimizes wasted space in the chosen block)
- **Worst Fit**: Scans the entire list to find the largest block. (Leaves behind larger, more usable fragments)

### 3.3 Deallocation and Coalescing
When a block is freed, the simulator automatically checks its immediate neighbors. If the preceding or succeeding blocks are also free, they are merged into a single contiguous block to combat **External Fragmentation**.

---

## 4. Virtual Memory & Paging

### 4.1 Configuration
- **Page Size**: 64 Bytes.
- **Virtual Address Space**: 16 Pages (1024 Bytes).
- **Physical Frames**: 16 Frames (1024 Bytes).

### 4.2 Replacement Policies
When all physical frames are occupied and a new page must be loaded, the simulator uses:
- **FIFO (First-In-First-Out)**: Evicts the page that was loaded earliest.
- **LRU (Least Recently Used)**: Tracks access timestamps and evicts the page that hasn't been touched for the longest time.

---

## 5. Cache Hierarchy

### 5.1 Design
The simulator uses a **Direct-Mapped** cache architecture:
- **L1 Cache**: 8 lines, 64-byte block size.
- **L2 Cache**: 16 lines, 64-byte block size.

### 5.2 Promotion Logic
On an L1 miss but L2 hit, the block is "promoted" to L1. This simulates the temporal locality principle where recently accessed data is moved closer to the CPU.

---

## 6. Metrics and Statistics

### 6.1 Fragmentation
- **Internal Fragmentation**: Calculated as the difference between allocated block size and requested size. (Currently 0% as blocks are split exactly).
- **External Fragmentation**: Calculated as:
  $$1 - \left( \frac{\text{Largest Free Block}}{\text{Total Free Memory}} \right) \times 100$$
  This represents the inability to use free memory because it is broken into small, non-contiguous pieces.

### 6.2 Performance
- **Memory Utilization**: Percentage of total memory currently marked as `USED`.
- **Cache Hit Ratio**: $\frac{\text{Hits}}{\text{Hits} + \text{Misses}}$ for both L1 and L2.

---

## 7. Limitations and Future Scope
- **Single Process**: The current model supports only one page table.
- **Direct Mapping**: Caches are not yet set-associative.
- **Buddy System**: Not yet implemented (planned for future release).
- **Write Policy**: Currently models "Read" access; write-back/write-through logic is not simulated.