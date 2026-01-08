LINK TO THE VIDEO OF THE PROJECT: https://www.youtube.com/watch?v=17XCUx5z0oE

## Memory Access Pipeline

The simulator models the complete operating system memory access pipeline.
All memory accesses follow the sequence below:

1. Virtual Address Generation  
   The user issues a virtual memory access using a virtual address.

2. TLB Lookup  
   The Translation Lookaside Buffer (TLB) is checked first.
   - On a TLB hit, the physical frame number is retrieved immediately.
   - On a TLB miss, the page table is consulted.

3. Page Table Lookup  
   The page table maps virtual pages to physical frames.
   - If the page is not present, a page fault occurs.
   - Page replacement (FIFO or LRU) is applied if no free frame is available.

4. Physical Address Formation  
   The physical address is constructed as:
   physical_address = frame_number × page_size + offset

5. Cache Hierarchy Access  
   The physical address is accessed through a multilevel cache hierarchy:
   - L1 Cache (first level)
   - L2 Cache (second level)
   On an L1 miss, the access propagates to L2.
   On an L2 miss, data is fetched from physical memory.
   Retrieved data is promoted back to higher cache levels.

6. Physical Memory Access  
   If all cache levels miss, the access is serviced from physical memory.

This pipeline accurately reflects the memory hierarchy of modern operating systems.
