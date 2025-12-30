## Canonical Memory Access Pipeline

The simulator follows a strict, OS-inspired memory access pipeline.
Every memory access is processed in the following order:

User Request  
↓  
Virtual Address (if virtual memory is enabled)  
↓  
TLB Lookup  
↓  
Page Table Lookup  
↓  
Page Replacement (on page fault, using FIFO or LRU)  
↓  
Physical Address  
↓  
L1 Cache  
↓  
L2 Cache  
↓  
Physical Memory  

This order is enforced explicitly in the simulator’s control flow.
Caches are accessed only after successful virtual-to-physical address
translation. Page replacement is triggered only on page faults when no
free physical frame is available.


## System Architecture

+----------------------+
|        User          |
+----------+-----------+
           |
           v
+----------------------+
|   Virtual Address    |
+----------+-----------+
           |
           v
+----------------------+
|         TLB          |
+----------+-----------+
           |
           v
+----------------------+
|     Page Table       |
+----------+-----------+
           |
           v
+----------------------+
|  Page Replacement   |
|   (FIFO / LRU)      |
+----------+-----------+
           |
           v
+----------------------+
|  Physical Address   |
+----------+-----------+
           |
           v
+----------------------+
|      L1 Cache        |
+----------+-----------+
           |
           v
+----------------------+
|      L2 Cache        |
+----------+-----------+
           |
           v
+----------------------+
|  Physical Memory    |
+----------------------+


## Design Decisions

- Memory is simulated at byte granularity.
- Physical memory is contiguous and dynamically partitioned.
- Allocation strategies (First Fit, Best Fit, Worst Fit) are policy-driven and
  separated from memory representation.
- Paging is implemented using fixed-size pages.
- TLB is modeled as a small FIFO-based cache.
- Cache hierarchy uses direct-mapped L1 and L2 caches.
- Cache replacement uses FIFO, which is permitted by the specification.
- Internal fragmentation is zero because memory is allocated exactly as requested.


## Specification Coverage

| Feature | Status |
|------|--------|
| Physical Memory Simulation | Implemented |
| Allocation Strategies | Implemented |
| Allocation Interface | Implemented |
| Fragmentation Metrics | Implemented |
| Multilevel Cache | Implemented (L1 + L2) |
| Virtual Memory | Implemented (Optional) |
| Buddy Allocator | Not Implemented (Optional) |
