#include "AllAlgoComp.h"
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <random>
#include <vector>

#define print(x) std::cout << x << "\n";

struct BenchResult {
	double avgAllocNs = 0.0;
	double avgFreeNs = 0.0;
	double worstFreeUs = 0.0;
	double totalRunMs = 0.0;
};

using Clock = std::chrono::steady_clock;

static inline double NsBetween(Clock::time_point a, Clock::time_point b)
{
	return (double)std::chrono::duration_cast<std::chrono::nanoseconds>(b - a).count();
}

template<typename AllocFn, typename FreeFn>
BenchResult StressCore(std::size_t allocationCount, bool chaotic, AllocFn allocFn, FreeFn freeFn)
{
	std::vector<int*> liveBlocks;
	liveBlocks.reserve(allocationCount);

	double allocNsTotal = 0.0;
	double freeNsTotal = 0.0;
	double worstFreeNs = 0.0;
	std::size_t freeCount = 0;

	std::mt19937 rng(12345);
	std::uniform_int_distribution<int> coinFlip(0, 1);

	auto timedFree = [&](int* p) {
		auto freeStart = Clock::now();
		freeFn(p);
		auto freeEnd = Clock::now();
		double freeNs = NsBetween(freeStart, freeEnd);
		freeNsTotal += freeNs;
		freeCount += 1;
		if (freeNs > worstFreeNs)
			worstFreeNs = freeNs;
	};

	auto tStart = Clock::now();

	for (std::size_t i = 0; i < allocationCount; i++) {
		auto opStart = Clock::now();
		int* mem = allocFn();
		auto opEnd = Clock::now();
		allocNsTotal += NsBetween(opStart, opEnd);

		*mem = (int)i;
		liveBlocks.push_back(mem);

		bool shouldFree = chaotic ? (coinFlip(rng) == 1) : (i % 2 == 1);
		if (shouldFree && !liveBlocks.empty()) {
			std::size_t pickIndex = chaotic ? (rng() % liveBlocks.size()) : (liveBlocks.size() - 1);
			int* toFree = liveBlocks[pickIndex];
			liveBlocks.erase(liveBlocks.begin() + pickIndex);
			timedFree(toFree);
		}
	}

	for (int* mem : liveBlocks)
		timedFree(mem);

	auto tEnd = Clock::now();

	BenchResult result;
	result.avgAllocNs = allocNsTotal / (double)allocationCount;
	result.avgFreeNs = (freeCount > 0) ? (freeNsTotal / (double)freeCount) : 0.0;
	result.worstFreeUs = worstFreeNs / 1000.0;
	result.totalRunMs = NsBetween(tStart, tEnd) / 1000000.0;

	return result;
}

template<CoalesceAlgorithm Algo>
BenchResult StressTest(std::size_t arenaSize, std::size_t allocationCount, bool chaotic)
{
	Allocator<Algo> allocator(arenaSize);
	return StressCore(allocationCount, chaotic,
					  [&] { return allocator.template Allocate<int>(sizeof(int)); },
					  [&](int* p) { allocator.Free(p); });
}

BenchResult StressTestCppDefault(std::size_t allocationCount, bool chaotic)
{
	return StressCore(allocationCount, chaotic,
					  [] { return (int*)std::malloc(sizeof(int)); },
					  [](int* p) { std::free(p); });
}

struct Payload64 { char data[64]; };
struct Payload256 { char data[256]; };

constexpr int MixedTypeCount = 6;

const char* MixedTypeNames[MixedTypeCount] = {
	"char        ",
	"short       ",
	"int         ",
	"double      ",
	"64B struct  ",
	"256B struct "
};

struct MixedResult {
	double allocNs[MixedTypeCount] = {};
	double freeNs[MixedTypeCount] = {};
	std::size_t allocCount[MixedTypeCount] = {};
	std::size_t freeCount[MixedTypeCount] = {};
	double worstFreeUs = 0.0;
	double totalRunMs = 0.0;
};

template<CoalesceAlgorithm Algo>
void* MixedAllocate(Allocator<Algo>& allocator, int type)
{
	switch (type) {
	case 0: return allocator.template Allocate<char>(sizeof(char));
	case 1: return allocator.template Allocate<short>(sizeof(short));
	case 2: return allocator.template Allocate<int>(sizeof(int));
	case 3: return allocator.template Allocate<double>(sizeof(double));
	case 4: return allocator.template Allocate<Payload64>(sizeof(Payload64));
	default: return allocator.template Allocate<Payload256>(sizeof(Payload256));
	}
}

template<CoalesceAlgorithm Algo>
void MixedFree(Allocator<Algo>& allocator, void* ptr, int type)
{
	switch (type) {
	case 0: allocator.Free(static_cast<char*>(ptr)); break;
	case 1: allocator.Free(static_cast<short*>(ptr)); break;
	case 2: allocator.Free(static_cast<int*>(ptr)); break;
	case 3: allocator.Free(static_cast<double*>(ptr)); break;
	case 4: allocator.Free(static_cast<Payload64*>(ptr)); break;
	default: allocator.Free(static_cast<Payload256*>(ptr)); break;
	}
}

inline std::size_t MixedTypeSize(int type)
{
	switch (type) {
	case 0: return sizeof(char);
	case 1: return sizeof(short);
	case 2: return sizeof(int);
	case 3: return sizeof(double);
	case 4: return sizeof(Payload64);
	default: return sizeof(Payload256);
	}
}

template<typename AllocFn, typename FreeFn>
MixedResult MixedCore(std::size_t allocationCount, bool chaotic, AllocFn allocFn, FreeFn freeFn)
{
	struct LiveBlock {
		void* ptr;
		int type;
	};

	std::vector<LiveBlock> liveBlocks;
	liveBlocks.reserve(allocationCount);

	MixedResult result;
	double allocNsSum[MixedTypeCount] = {};
	double freeNsSum[MixedTypeCount] = {};
	double worstFreeNs = 0.0;

	std::mt19937 rng(12345);
	std::uniform_int_distribution<int> coinFlip(0, 1);
	std::uniform_int_distribution<int> typePick(0, MixedTypeCount - 1);

	auto timedFree = [&](LiveBlock block) {
		auto freeStart = Clock::now();
		freeFn(block.ptr, block.type);
		auto freeEnd = Clock::now();
		double freeNs = NsBetween(freeStart, freeEnd);
		freeNsSum[block.type] += freeNs;
		result.freeCount[block.type] += 1;
		if (freeNs > worstFreeNs)
			worstFreeNs = freeNs;
	};

	auto tStart = Clock::now();

	for (std::size_t i = 0; i < allocationCount; i++) {
		int type = typePick(rng);

		auto opStart = Clock::now();
		void* mem = allocFn(type);
		auto opEnd = Clock::now();
		allocNsSum[type] += NsBetween(opStart, opEnd);
		result.allocCount[type] += 1;

		*static_cast<char*>(mem) = (char)i;
		liveBlocks.push_back({ mem, type });

		bool shouldFree = chaotic ? (coinFlip(rng) == 1) : (i % 2 == 1);
		if (shouldFree && !liveBlocks.empty()) {
			std::size_t pickIndex = chaotic ? (rng() % liveBlocks.size()) : (liveBlocks.size() - 1);
			LiveBlock toFree = liveBlocks[pickIndex];
			liveBlocks.erase(liveBlocks.begin() + pickIndex);
			timedFree(toFree);
		}
	}

	for (LiveBlock block : liveBlocks)
		timedFree(block);

	auto tEnd = Clock::now();

	for (int t = 0; t < MixedTypeCount; t++) {
		result.allocNs[t] = (result.allocCount[t] > 0) ? (allocNsSum[t] / (double)result.allocCount[t]) : 0.0;
		result.freeNs[t] = (result.freeCount[t] > 0) ? (freeNsSum[t] / (double)result.freeCount[t]) : 0.0;
	}
	result.worstFreeUs = worstFreeNs / 1000.0;
	result.totalRunMs = NsBetween(tStart, tEnd) / 1000000.0;

	return result;
}

template<CoalesceAlgorithm Algo>
MixedResult MixedStressTest(std::size_t arenaSize, std::size_t allocationCount, bool chaotic)
{
	Allocator<Algo> allocator(arenaSize);
	return MixedCore(allocationCount, chaotic,
					 [&](int type) { return MixedAllocate(allocator, type); },
					 [&](void* p, int type) { MixedFree(allocator, p, type); });
}

MixedResult MixedStressTestCppDefault(std::size_t allocationCount, bool chaotic)
{
	return MixedCore(allocationCount, chaotic,
					 [](int type) { return std::malloc(MixedTypeSize(type)); },
					 [](void* p, int) { std::free(p); });
}

void PrintMixedResult(const char* name, const MixedResult& r)
{
	print(name);
	for (int t = 0; t < MixedTypeCount; t++)
		print("  " << MixedTypeNames[t] << " alloc: " << r.allocNs[t] << " ns, free: " << r.freeNs[t] << " ns");
	print("  worst free spike: " << r.worstFreeUs << " us");
	print("  total run time:   " << r.totalRunMs << " ms\n");
}

void RunAllAlgoComparisionBenchmark()
{
	std::size_t arenaSize = StandardMemoryUnits::MB * 512;
	std::size_t allocationCount = 250000;

	print("Allocator Benchmark =====================\n");

	print("Running LinkPrevious (heavy/chaotic)...");
	BenchResult linkPrevHeavy = StressTest<CoalesceAlgorithm::LinkPrevious>(arenaSize, allocationCount, true);

	print("Running LinkPrevious (light/orderly)...");
	BenchResult linkPrevLight = StressTest<CoalesceAlgorithm::LinkPrevious>(arenaSize, allocationCount, false);

	print("Running SearchFromHead (heavy/chaotic)...");
	BenchResult searchHeadHeavy = StressTest<CoalesceAlgorithm::SearchFromHead>(arenaSize, allocationCount, true);

	print("Running SearchFromHead (light/orderly)...");
	BenchResult searchHeadLight = StressTest<CoalesceAlgorithm::SearchFromHead>(arenaSize, allocationCount, false);

	print("Running FixedSize_NoHeader (heavy/chaotic)...");
	BenchResult fixedHeavy = StressTest<CoalesceAlgorithm::FixedSize_NoHeader>(arenaSize, allocationCount, true);

	print("Running FixedSize_NoHeader (light/orderly)...");
	BenchResult fixedLight = StressTest<CoalesceAlgorithm::FixedSize_NoHeader>(arenaSize, allocationCount, false);

	print("Running CPP_Default -- malloc/free (heavy/chaotic)...");
	BenchResult cppDefaultHeavy = StressTestCppDefault(allocationCount, true);

	print("Running CPP_Default -- malloc/free (light/orderly)...");
	BenchResult cppDefaultLight = StressTestCppDefault(allocationCount, false);

	print("\nResults ==================================\n");

	print("Free() avg cost -- heavy/chaotic use:");
	print("  LinkPrevious:       " << linkPrevHeavy.avgFreeNs << " ns");
	print("  SearchFromHead:     " << searchHeadHeavy.avgFreeNs << " ns");
	print("  FixedSize_NoHeader: " << fixedHeavy.avgFreeNs << " ns");
	print("  CPP_Default:        " << cppDefaultHeavy.avgFreeNs << " ns");

	print("\nFree() avg cost -- light/orderly use:");
	print("  LinkPrevious:       " << linkPrevLight.avgFreeNs << " ns");
	print("  SearchFromHead:     " << searchHeadLight.avgFreeNs << " ns");
	print("  FixedSize_NoHeader: " << fixedLight.avgFreeNs << " ns");
	print("  CPP_Default:        " << cppDefaultLight.avgFreeNs << " ns");

	print("\nFree() worst-case spike -- heavy use:");
	print("  LinkPrevious:       " << linkPrevHeavy.worstFreeUs << " us");
	print("  SearchFromHead:     " << searchHeadHeavy.worstFreeUs << " us");
	print("  FixedSize_NoHeader: " << fixedHeavy.worstFreeUs << " us");
	print("  CPP_Default:        " << cppDefaultHeavy.worstFreeUs << " us");

	print("\nAllocate() avg cost:");
	print("  LinkPrevious   heavy: " << linkPrevHeavy.avgAllocNs << " ns, light: " << linkPrevLight.avgAllocNs << " ns");
	print("  SearchFromHead heavy: " << searchHeadHeavy.avgAllocNs << " ns, light: " << searchHeadLight.avgAllocNs << " ns");
	print("  FixedSize      heavy: " << fixedHeavy.avgAllocNs << " ns, light: " << fixedLight.avgAllocNs << " ns");
	print("  CPP_Default    heavy: " << cppDefaultHeavy.avgAllocNs << " ns, light: " << cppDefaultLight.avgAllocNs << " ns");

	print("\nTotal run time -- heavy/chaotic use:");
	print("  LinkPrevious:       " << linkPrevHeavy.totalRunMs << " ms");
	print("  SearchFromHead:     " << searchHeadHeavy.totalRunMs << " ms");
	print("  FixedSize_NoHeader: " << fixedHeavy.totalRunMs << " ms");
	print("  CPP_Default:        " << cppDefaultHeavy.totalRunMs << " ms");

	print("\nTotal run time -- light/orderly use:");
	print("  LinkPrevious:       " << linkPrevLight.totalRunMs << " ms");
	print("  SearchFromHead:     " << searchHeadLight.totalRunMs << " ms");
	print("  FixedSize_NoHeader: " << fixedLight.totalRunMs << " ms");
	print("  CPP_Default:        " << cppDefaultLight.totalRunMs << " ms");

	print("\nMemory overhead per block:");
	print("  LinkPrevious:       " << sizeof(BlockHeader<CoalesceAlgorithm::LinkPrevious>) << " bytes");
	print("  SearchFromHead:     " << sizeof(BlockHeader<CoalesceAlgorithm::SearchFromHead>) << " bytes");
	print("  FixedSize_NoHeader: 1 bit");

	print("\nTotal header memory tax -- heavy use (" << allocationCount << " allocations):");
	print("  LinkPrevious:   " << (sizeof(BlockHeader<CoalesceAlgorithm::LinkPrevious>) * allocationCount) / (double)StandardMemoryUnits::MB << " MB");
	print("  SearchFromHead: " << (sizeof(BlockHeader<CoalesceAlgorithm::SearchFromHead>) * allocationCount) / (double)StandardMemoryUnits::MB << " MB");
	print("  FixedSize_NoHeader: " << ((1/8) * allocationCount) / (double)StandardMemoryUnits::MB << " MB");
	print("\nNote: CPP_Default (malloc) header overhead is implementation-defined and not directly measurable, so it's excluded from the memory tax comparison above.");
	print("Also FixedSize_NoHeader uses 1 bit of memory per alloc in external bitmap metadata, no in memory header");

	print("\n===========================================");
}

void RunMixedTypeComparisionBenchmark()
{
	std::size_t arenaSize = StandardMemoryUnits::MB * 512;
	std::size_t allocationCount = 250000;

	print("Mixed Type Benchmark ====================\n");

	print("Running LinkPrevious (heavy/chaotic)...");
	MixedResult linkPrevHeavy = MixedStressTest<CoalesceAlgorithm::LinkPrevious>(arenaSize, allocationCount, true);

	print("Running LinkPrevious (light/orderly)...");
	MixedResult linkPrevLight = MixedStressTest<CoalesceAlgorithm::LinkPrevious>(arenaSize, allocationCount, false);

	print("Running SearchFromHead (heavy/chaotic)...");
	MixedResult searchHeadHeavy = MixedStressTest<CoalesceAlgorithm::SearchFromHead>(arenaSize, allocationCount, true);

	print("Running SearchFromHead (light/orderly)...");
	MixedResult searchHeadLight = MixedStressTest<CoalesceAlgorithm::SearchFromHead>(arenaSize, allocationCount, false);

	print("Running CPP_Default -- malloc/free (heavy/chaotic)...");
	MixedResult cppDefaultHeavy = MixedStressTestCppDefault(allocationCount, true);

	print("Running CPP_Default -- malloc/free (light/orderly)...");
	MixedResult cppDefaultLight = MixedStressTestCppDefault(allocationCount, false);

	print("\nResults ==================================\n");

	print("Heavy/chaotic use ------------------------\n");
	PrintMixedResult("LinkPrevious:", linkPrevHeavy);
	PrintMixedResult("SearchFromHead:", searchHeadHeavy);
	PrintMixedResult("CPP_Default:", cppDefaultHeavy);

	print("Light/orderly use ------------------------\n");
	PrintMixedResult("LinkPrevious:", linkPrevLight);
	PrintMixedResult("SearchFromHead:", searchHeadLight);
	PrintMixedResult("CPP_Default:", cppDefaultLight);

	print("===========================================");
}