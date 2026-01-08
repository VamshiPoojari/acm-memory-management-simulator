#!/bin/bash

# Build the project
echo "Building project..."
make clean && make

if [ ! -f ./memory_simulator ]; then
    echo "Build failed!"
    exit 1
fi

echo -e "\n========================================"
echo "RUNNING ALLOCATION TEST"
echo "========================================"
./memory_simulator < tests/test_allocation.txt

echo -e "\n========================================"
echo "RUNNING VIRTUAL MEMORY TEST"
echo "========================================"
./memory_simulator < tests/test_virtual_memory.txt

echo -e "\n========================================"
echo "RUNNING CACHE TEST"
echo "========================================"
./memory_simulator < tests/test_cache.txt

echo -e "\n========================================"
echo "RUNNING STRESS TEST (Fragmentation, Thrashing, Conflicts)"
echo "========================================"
./memory_simulator < tests/test_stress.txt

echo -e "\nTests completed."
