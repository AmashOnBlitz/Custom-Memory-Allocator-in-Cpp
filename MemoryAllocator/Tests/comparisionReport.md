# Allocator Benchmark Results

modes tested: **LinkPrevious** (each block stores a direct pointer to its prev block)
			  **SearchFromHead** (has to walk the list to find its prev mem). 

LinkPrevious pays extra memory per block for that pointer;
SearchFromHead saves the memory but pays for it by searching.

## Results

| Metric | LinkPrevious | SearchFromHead | Delta |
|---|---|---|---|
| Free() avg cost -- heavy/chaotic use | 26.1 ns | 569.6 ns | SearchFromHead is **2082% slower** |
| Free() avg cost -- light/orderly use | 23.4 ns | 87.7 ns | SearchFromHead is **275% slower** |
| Free() worst-case spike -- heavy use | 80.6 us | 3,399.6 us | SearchFromHead is **4118% slower** at its worst |
| Allocate() avg cost | ~roughly equal both scenarios | ~roughly equal both scenarios | no meaningful difference |
| Total run time -- heavy/chaotic use | 372.05 ms | 563.41 ms | LinkPrevious **51% faster** overall |
| Total run time -- light/orderly use | 8.64 ms | 9.07 ms | LinkPrevious **5% faster** overall |
| Memory overhead per block | 32 B | 24 B | LinkPrevious costs **33% more** memory per block |
| Total header memory tax -- heavy use (~250k allocations) | 7.63 MB | 5.72 MB | LinkPrevious spends ~2 MB more overall |

## TL;DR

LinkPrevious wins on speed everywhere, and the gap gets huge (~2000%+) the
messier your alloc/free pattern gets. The only thing it costs you is a fixed
8 bytes extra per block, that's the whole trade off.


**Pick LinkPrevious unless** you're allocating a massive number of very
small objects or you are in a memory scarce environment and every byte wasted actually matters.

Note: Reports as of September 21st 2026, Commit no - 20