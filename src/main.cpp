#include <iostream>
#include <vector>
#include <string>
#include <queue>
#include <unordered_map>
#include <climits>
#include <iomanip>

struct MemoryBlock {
    int start;
    int size;
    bool free;
    int id;
};

struct PageTableEntry {
    bool valid;
    int frameNumber;
};

struct TLBEntry {
    int pageNumber;
    int frameNumber;
    bool valid;
};

enum class AllocationStrategy {
    FIRST_FIT,
    BEST_FIT,
    WORST_FIT
};

enum class ReplacementPolicy {
    FIFO,
    LRU
};

struct CacheLine {
    int tag;
    bool valid;
};


struct Cache {
    int numLines;
    std::vector<CacheLine> lines;
    int hits = 0;
    int misses = 0;

    Cache(int n) : numLines(n), lines(n, {-1, false}) {}
};

ReplacementPolicy currentPolicy = ReplacementPolicy::FIFO;


AllocationStrategy currentStrategy = AllocationStrategy::FIRST_FIT;
int nextAllocId = 1;
const int PAGE_SIZE = 64;      // bytes
const int NUM_PAGES = 32;      // virtual pages
const int NUM_FRAMES = 16;     // physical frames

// Global
int timeCounter = 0;

int totalAllocRequests = 0;
int successfulAllocs = 0;
int failedAllocs = 0;

const int CACHE_BLOCK_SIZE = 64;   // bytes (same as page size)
const int L1_LINES = 8;
const int L2_LINES = 16;

bool allocateMemory(std::vector<MemoryBlock>& memory, int requestSize) {
    if (requestSize <= 0) return false;
    if (memory.empty()) {
        std::cout << "Error: Memory not initialized. Use 'init memory <size>'.\n";
        return false;
    }

    int chosenIndex = -1;
    totalAllocRequests++;

    for (size_t i = 0; i < memory.size(); i++) {
        if (!memory[i].free || memory[i].size < requestSize)
            continue;

        if (currentStrategy == AllocationStrategy::FIRST_FIT) {
            chosenIndex = i;
            break;
        }

        if (currentStrategy == AllocationStrategy::BEST_FIT) {
            if (chosenIndex == -1 || memory[i].size < memory[chosenIndex].size)
                chosenIndex = i;
        }

        if (currentStrategy == AllocationStrategy::WORST_FIT) {
            if (chosenIndex == -1 || memory[i].size > memory[chosenIndex].size)
                chosenIndex = i;
        }
    }

    if (chosenIndex == -1) {
        failedAllocs++;
        return false;
    }

    successfulAllocs++;
    MemoryBlock allocated = {
        memory[chosenIndex].start,
        requestSize,
        false,
        nextAllocId++
    };

    memory[chosenIndex].start += requestSize;
    memory[chosenIndex].size -= requestSize;

    if (memory[chosenIndex].size == 0)
        memory.erase(memory.begin() + chosenIndex);

    memory.insert(memory.begin() + chosenIndex, allocated);
    return true;
}


bool tlbLookup(int pageNumber,
               std::vector<TLBEntry>& tlb,
               int& frameNumber);

void updateTLB(int pageNumber,
               int frameNumber,
               std::vector<TLBEntry>& tlb);


bool freeMemory(std::vector<MemoryBlock>& memory, int id) {
    for (size_t i = 0; i < memory.size(); i++) {
        if (!memory[i].free && memory[i].id == id) {
            memory[i].free = true;
            memory[i].id = -1;

            // Coalesce with previous
            if (i > 0 && memory[i - 1].free) {
                memory[i - 1].size += memory[i].size;
                memory.erase(memory.begin() + i);
                i--;
            }

            // Coalesce with next
            if (i + 1 < memory.size() && memory[i + 1].free) {
                memory[i].size += memory[i + 1].size;
                memory.erase(memory.begin() + i + 1);
            }

            return true;
        }
    }
    return false;
}

void dumpMemory(const std::vector<MemoryBlock>& memory) {
    std::cout << "\nMemory Dump:\n";

    for (const auto& block : memory) {
        std::cout << "[0x"
                  << std::setw(4) << std::setfill('0') << std::hex << block.start
                  << " - 0x"
                  << std::setw(4) << std::hex << (block.start + block.size - 1)
                  << "] ";

        if (block.free)
            std::cout << "FREE\n";
        else
            std::cout << "USED (id=" << std::dec << block.id << ")\n";
    }

    std::cout << std::dec << std::endl;
}

bool translateAddress(
    int virtualAddress,
    std::vector<PageTableEntry>& pageTable,
    std::vector<TLBEntry>& tlb,
    std::unordered_map<int, int>& lastUsed,
    int& physicalAddress,
    int& tlbHits,
    int& tlbMisses
) {
    int pageNumber = virtualAddress / PAGE_SIZE;
    int offset = virtualAddress % PAGE_SIZE;

    if (pageNumber < 0 || pageNumber >= NUM_PAGES) {
        std::cout << "Invalid virtual address.\n";
        return false;
    }

    int frame;

    // TLB lookup
    if (tlbLookup(pageNumber, tlb, frame)) {
        tlbHits++;
        physicalAddress = frame * PAGE_SIZE + offset;
        return true;
    }

    tlbMisses++;

    // Page table lookup
    if (!pageTable[pageNumber].valid) {
        std::cout << "Page fault at page " << pageNumber << ".\n";
        return false;
    }

    frame = pageTable[pageNumber].frameNumber;
    updateTLB(pageNumber, frame, tlb);
    lastUsed[pageNumber] = timeCounter++;
    physicalAddress = frame * PAGE_SIZE + offset;
    return true;
}

int evictPage(std::vector<PageTableEntry>& pageTable,
              std::vector<bool>& frameUsed,
              std::queue<int>& fifoQueue,
              std::unordered_map<int, int>& lastUsed);

bool mapPage(int pageNumber,
             std::vector<PageTableEntry>& pageTable,
             std::vector<bool>& frameUsed,
             std::queue<int>& fifoQueue,
             std::unordered_map<int, int>& lastUsed) {

    // Try free frame first
    for (int f = 0; f < NUM_FRAMES; f++) {
        if (!frameUsed[f]) {
            frameUsed[f] = true;
            pageTable[pageNumber] = {true, f};
            fifoQueue.push(pageNumber);
            lastUsed[pageNumber] = timeCounter++;
            return true;
        }
    }

    // Need eviction
    int frame = evictPage(pageTable, frameUsed, fifoQueue, lastUsed);

    frameUsed[frame] = true;
    pageTable[pageNumber] = {true, frame};
    fifoQueue.push(pageNumber);
    lastUsed[pageNumber] = timeCounter++;
    return true;
}

int evictPage(std::vector<PageTableEntry>& pageTable,
              std::vector<bool>& frameUsed,
              std::queue<int>& fifoQueue,
              std::unordered_map<int, int>& lastUsed) {

    int victimPage = -1;

    if (currentPolicy == ReplacementPolicy::FIFO) {
        while (!fifoQueue.empty()) {
            victimPage = fifoQueue.front();
            fifoQueue.pop();
            if (pageTable[victimPage].valid) break;
        }
    } else {
        int oldestTime = INT_MAX;
        for (auto const& [page, time] : lastUsed) {
            if (pageTable[page].valid && time < oldestTime) {
                oldestTime = time;
                victimPage = page;
            }
        }
    }

    if (victimPage == -1 || !pageTable[victimPage].valid) {
        // Fallback: just find any valid page
        for (int i = 0; i < NUM_PAGES; i++) {
            if (pageTable[i].valid) {
                victimPage = i;
                break;
            }
        }
    }

    int frame = pageTable[victimPage].frameNumber;
    pageTable[victimPage].valid = false;
    pageTable[victimPage].frameNumber = -1;
    frameUsed[frame] = false;
    lastUsed.erase(victimPage);

    std::cout << "Evicted page " << victimPage << ".\n";
    return frame;
}

bool tlbLookup(int pageNumber,
               std::vector<TLBEntry>& tlb,
               int& frameNumber) {
    for (auto& entry : tlb) {
        if (entry.valid && entry.pageNumber == pageNumber) {
            frameNumber = entry.frameNumber;
            return true;
        }
    }
    return false;
}

void initMemory(std::vector<MemoryBlock>& memory, int size) {
    memory.clear();
    memory.push_back({0, size, true, -1});
    nextAllocId = 1;
}

void updateTLB(int pageNumber,
               int frameNumber,
               std::vector<TLBEntry>& tlb) {
    static int next = 0;
    tlb[next] = {pageNumber, frameNumber, true};
    next = (next + 1) % tlb.size();
}

bool freeByAddress(std::vector<MemoryBlock>& memory, int address) {
    for (size_t i = 0; i < memory.size(); i++) {
        if (!memory[i].free && memory[i].start == address) {
            int id = memory[i].id;
            return freeMemory(memory, id);
        }
    }
    return false;
}

int totalMemorySize(const std::vector<MemoryBlock>& memory) {
    int total = 0;
    for (const auto& block : memory)
        total += block.size;
    return total;
}

int usedMemory(const std::vector<MemoryBlock>& memory) {
    int used = 0;
    for (const auto& block : memory)
        if (!block.free)
            used += block.size;
    return used;
}

int totalFreeMemory(const std::vector<MemoryBlock>& memory) {
    int freeMem = 0;
    for (const auto& block : memory)
        if (block.free)
            freeMem += block.size;
    return freeMem;
}

int largestFreeBlock(const std::vector<MemoryBlock>& memory) {
    int maxBlock = 0;
    for (const auto& block : memory)
        if (block.free && block.size > maxBlock)
            maxBlock = block.size;
    return maxBlock;
}

double memoryUtilization(const std::vector<MemoryBlock>& memory) {
    int total = totalMemorySize(memory);
    if (total == 0) return 0.0;
    return (double)usedMemory(memory) / total * 100.0;
}

double externalFragmentation(const std::vector<MemoryBlock>& memory) {
    int freeMem = totalFreeMemory(memory);
    if (freeMem == 0) return 0.0;

    int largest = largestFreeBlock(memory);
    return (1.0 - (double)largest / freeMem) * 100.0;
}

double internalFragmentation() {
    // In this allocator, blocks are allocated exactly as requested
    return 0.0;
}

bool accessCache(Cache& cache, int physicalAddress) {
    int blockNumber = physicalAddress / CACHE_BLOCK_SIZE;
    int index = blockNumber % cache.numLines;
    int tag = blockNumber / cache.numLines;

    if (cache.lines[index].valid && cache.lines[index].tag == tag) {
        cache.hits++;
        return true;
    }

    cache.misses++;
    cache.lines[index] = {tag, true};  // FIFO for direct-mapped
    return false;
}

void accessMemoryHierarchy(int physicalAddress,
                           Cache& L1,
                           Cache& L2) {
    // L1 lookup
    if (accessCache(L1, physicalAddress))
        return;

    // L2 lookup
    if (accessCache(L2, physicalAddress)) {
        // Promote to L1
        accessCache(L1, physicalAddress);
        return;
    }

    // Miss in both → fetch from memory
    // Fill L2 then L1
    accessCache(L2, physicalAddress);
    accessCache(L1, physicalAddress);
}

int main() {
    std::cout << "Memory Management Simulator (Skeleton)\n";
    std::cout << "Type 'help' to see commands.\n";

    std::string command;
    const int TLB_SIZE = 4;
    std::vector<TLBEntry> tlb(TLB_SIZE, {-1, -1, false});

    int tlbHits = 0;
    int tlbMisses = 0;

    
    std::vector<MemoryBlock> memory;
    std::vector<bool> frameUsed(NUM_FRAMES, false);
    std::queue<int> fifoQueue;              // page numbers
    std::unordered_map<int, int> lastUsed;  // page -> timestamp
  
    std::vector<PageTableEntry> pageTable(NUM_PAGES);

    for (int i = 0; i < NUM_PAGES; i++) {
        pageTable[i].valid = false;
        pageTable[i].frameNumber = -1;
    }
    Cache L1(L1_LINES);
    Cache L2(L2_LINES);

    while (true) {
        std::cout << ">> ";
        std::getline(std::cin, command);

        if (command == "exit") {
            std::cout << "Exiting simulator.\n";
            break;
        }
        else if (command == "help") {
            std::cout << "Supported commands:\n";
            std::cout << "  alloc <size>\n";
            std::cout << "  free <id>\n";
            std::cout << "  show\n";
            std::cout << "  stats\n";
            std::cout << "  strategy first|best|worst\n";
            std::cout << "  policy fifo|lru\n";
            std::cout << "  load <page>\n";
            std::cout << "  translate <virtual_address>\n";
            std::cout << "  exit\n";

        }

        else if (command.empty()) {
            continue;
        }
        else if (command == "dump memory" || command == "show") {
            dumpMemory(memory);
        }
        else if (command.rfind("init memory", 0) == 0) {
            int size;
            try {
                size = std::stoi(command.substr(12));
            } catch (...) {
                std::cout << "Usage: init memory <size>\n";
                continue;
            }

            initMemory(memory, size);
            std::cout << "Initialized memory with size " << size << " bytes.\n";
        }
        else if (command.rfind("policy", 0) == 0) {
            std::string p = command.substr(7);

            if (p == "fifo") {
                currentPolicy = ReplacementPolicy::FIFO;
                std::cout << "Replacement policy: FIFO\n";
            } else if (p == "lru") {
                currentPolicy = ReplacementPolicy::LRU;
                std::cout << "Replacement policy: LRU\n";
            } else {
                std::cout << "Unknown policy. Use fifo | lru\n";
            }
        }

        else if (command.rfind("translate", 0) == 0) {
            int vaddr;
            try {
                vaddr = std::stoi(command.substr(10));
            } catch (...) {
                std::cout << "Invalid translate command.\n";
                continue;
            }

            int paddr;
            if (translateAddress(vaddr, pageTable, tlb, lastUsed, paddr, tlbHits, tlbMisses)) {
                accessMemoryHierarchy(paddr, L1, L2);

                std::cout << "Virtual Address " << vaddr
                        << " -> Physical Address " << paddr << "\n";
            }
        }
        else if (command.rfind("load", 0) == 0) {
            int page;
            try {
                page = std::stoi(command.substr(5));
            } catch (...) {
                std::cout << "Invalid load command.\n";
                continue;
            }

            if (page < 0 || page >= NUM_PAGES) {
                std::cout << "Invalid page number. Valid range: 0 - "
                        << NUM_PAGES - 1 << "\n";
                continue;
            }

            if (mapPage(page, pageTable, frameUsed, fifoQueue, lastUsed)) {
                std::cout << "Page " << page << " loaded into memory.\n";
            }
        }
        else if (command == "stats") {
            std::cout << "\n--- Memory Statistics ---\n";

            std::cout << "Total memory: "
                    << totalMemorySize(memory) << " bytes\n";

            std::cout << "Used memory: "
                    << usedMemory(memory) << " bytes\n";

            std::cout << "Memory utilization: "
                    << std::fixed << std::setprecision(2)
                    << memoryUtilization(memory) << "%\n";

            std::cout << "External fragmentation: "
                    << externalFragmentation(memory) << "%\n";

            std::cout << "Internal fragmentation: "
                    << internalFragmentation() << "%\n";

            std::cout << "Allocation requests: "
                    << totalAllocRequests << "\n";

            std::cout << "Successful allocations: "
                    << successfulAllocs << "\n";

            std::cout << "Failed allocations: "
                    << failedAllocs << "\n\n";
            
            std::cout << "\n--- Cache Statistics ---\n";
            std::cout << "L1 Hits: " << L1.hits << "\n";
            std::cout << "L1 Misses: " << L1.misses << "\n";
            std::cout << "L2 Hits: " << L2.hits << "\n";
            std::cout << "L2 Misses: " << L2.misses << "\n";

        }


        else if (command.rfind("strategy", 0) == 0) {
            std::string type = command.substr(9);

            if (type == "first") {
                currentStrategy = AllocationStrategy::FIRST_FIT;
                std::cout << "Strategy set to First Fit.\n";
            } else if (type == "best") {
                currentStrategy = AllocationStrategy::BEST_FIT;
                std::cout << "Strategy set to Best Fit.\n";
            } else if (type == "worst") {
                currentStrategy = AllocationStrategy::WORST_FIT;
                std::cout << "Strategy set to Worst Fit.\n";
            } else {
                std::cout << "Unknown strategy. Use: first | best | worst\n";
            }
        }

        else if (command.rfind("free", 0) == 0) {
            std::string arg = command.substr(5);

            try {
                // address-based free
                if (arg.rfind("addr", 0) == 0) {
                    int addr = std::stoi(arg.substr(5));
                    if (freeByAddress(memory, addr))
                        std::cout << "Block at address " << addr << " freed.\n";
                    else
                        std::cout << "Invalid address.\n";
                }
                // id-based free
                else {
                    int id = std::stoi(arg);
                    if (freeMemory(memory, id))
                        std::cout << "Block " << id << " freed.\n";
                    else
                        std::cout << "Invalid block id.\n";
                }
            } catch (...) {
                std::cout << "Usage: free <id> OR free addr <address>\n";
            }
        }

        else if (command.rfind("alloc", 0) == 0 ||
         command.rfind("malloc", 0) == 0) {
            int offset = (command.rfind("alloc", 0) == 0) ? 6 : 7;
            int size;

            try {
                size = std::stoi(command.substr(offset));
            } catch (...) {
                std::cout << "Invalid alloc/malloc command format.\n";
                continue;
            }    

            if (allocateMemory(memory, size)) {
                std::cout << "Allocated " << size << " bytes.\n";
            } else {
                std::cout << "Allocation failed: Not enough memory.\n";
            }
        }

        else {
            std::cout << "Received command: " << command << "\n";
        }
    }

    return 0;
}
