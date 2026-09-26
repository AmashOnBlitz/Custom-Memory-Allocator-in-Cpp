#include "AllAlgoComp.h"
#include <Windows.h>
#include <iostream>
#include <vector>
#include <random>

#define print(x) std::cout << x << "\n";

struct BenchResult {
	double avgAllocNs = 0.0;
	double avgFreeNs = 0.0;
	double worstFreeUs = 0.0;
	double totalRunMs = 0.0;
};

double ElapsedMs(LARGE_INTEGER start, LARGE_INTEGER end, LARGE_INTEGER freq)
{
	return (double)(end.QuadPart - start.QuadPart) * 1000.0 / (double)freq.QuadPart;
}

double ElapsedNs(LARGE_INTEGER start, LARGE_INTEGER end, LARGE_INTEGER freq)
{
	return (double)(end.QuadPart - start.QuadPart) * 1000000000.0 / (double)freq.QuadPart;
}

template<CoalesceAlgorithm Algo>
BenchResult StressTest(SIZE_T arenaSize, SIZE_T allocationCount, bool chaotic)
{
	LARGE_INTEGER freq, tStart, tEnd;
	QueryPerformanceFrequency(&freq);

	Allocator<Algo> allocator(arenaSize);

	std::vector<int*> liveBlocks;
	liveBlocks.reserve(allocationCount);

	double allocNsTotal = 0.0;
	double freeNsTotal = 0.0;
	double worstFreeNs = 0.0;
	SIZE_T freeCount = 0;

	std::mt19937 rng(12345);
	std::uniform_int_distribution<int> coinFlip(0, 1);

	QueryPerformanceCounter(&tStart);

	for (SIZE_T i = 0; i < allocationCount; i++) {
		LARGE_INTEGER opStart, opEnd;
		QueryPerformanceCounter(&opStart);
		int* mem = allocator.template Allocate<int>(sizeof(int));
		QueryPerformanceCounter(&opEnd);
		allocNsTotal += ElapsedNs(opStart, opEnd, freq);

		*mem = (int)i;
		liveBlocks.push_back(mem);

		bool shouldFree = chaotic ? (coinFlip(rng) == 1) : (i % 2 == 1);
		if (shouldFree && !liveBlocks.empty()) {
			SIZE_T pickIndex = chaotic ? (rng() % liveBlocks.size()) : (liveBlocks.size() - 1);
			int* toFree = liveBlocks[pickIndex];
			liveBlocks.erase(liveBlocks.begin() + pickIndex);

			LARGE_INTEGER freeStart, freeEnd;
			QueryPerformanceCounter(&freeStart);
			allocator.Free(toFree);
			QueryPerformanceCounter(&freeEnd);
			double freeNs = ElapsedNs(freeStart, freeEnd, freq);
			freeNsTotal += freeNs;
			freeCount += 1;
			if (freeNs > worstFreeNs)
				worstFreeNs = freeNs;
		}
	}

	for (int* mem : liveBlocks) {
		LARGE_INTEGER freeStart, freeEnd;
		QueryPerformanceCounter(&freeStart);
		allocator.Free(mem);
		QueryPerformanceCounter(&freeEnd);
		double freeNs = ElapsedNs(freeStart, freeEnd, freq);
		freeNsTotal += freeNs;
		freeCount += 1;
		if (freeNs > worstFreeNs)
			worstFreeNs = freeNs;
	}

	QueryPerformanceCounter(&tEnd);

	BenchResult result;
	result.avgAllocNs = allocNsTotal / (double)allocationCount;
	result.avgFreeNs = (freeCount > 0) ? (freeNsTotal / (double)freeCount) : 0.0;
	result.worstFreeUs = worstFreeNs / 1000.0;
	result.totalRunMs = ElapsedMs(tStart, tEnd, freq);

	return result;
}

BenchResult StressTestCppDefault(SIZE_T allocationCount, bool chaotic)
{
	LARGE_INTEGER freq, tStart, tEnd;
	QueryPerformanceFrequency(&freq);

	std::vector<int*> liveBlocks;
	liveBlocks.reserve(allocationCount);

	double allocNsTotal = 0.0;
	double freeNsTotal = 0.0;
	double worstFreeNs = 0.0;
	SIZE_T freeCount = 0;

	std::mt19937 rng(12345);
	std::uniform_int_distribution<int> coinFlip(0, 1);

	QueryPerformanceCounter(&tStart);

	for (SIZE_T i = 0; i < allocationCount; i++) {
		LARGE_INTEGER opStart, opEnd;
		QueryPerformanceCounter(&opStart);
		int* mem = (int*)std::malloc(sizeof(int));
		QueryPerformanceCounter(&opEnd);
		allocNsTotal += ElapsedNs(opStart, opEnd, freq);

		*mem = (int)i;
		liveBlocks.push_back(mem);

		bool shouldFree = chaotic ? (coinFlip(rng) == 1) : (i % 2 == 1);
		if (shouldFree && !liveBlocks.empty()) {
			SIZE_T pickIndex = chaotic ? (rng() % liveBlocks.size()) : (liveBlocks.size() - 1);
			int* toFree = liveBlocks[pickIndex];
			liveBlocks.erase(liveBlocks.begin() + pickIndex);

			LARGE_INTEGER freeStart, freeEnd;
			QueryPerformanceCounter(&freeStart);
			std::free(toFree);
			QueryPerformanceCounter(&freeEnd);
			double freeNs = ElapsedNs(freeStart, freeEnd, freq);
			freeNsTotal += freeNs;
			freeCount += 1;
			if (freeNs > worstFreeNs)
				worstFreeNs = freeNs;
		}
	}

	for (int* mem : liveBlocks) {
		LARGE_INTEGER freeStart, freeEnd;
		QueryPerformanceCounter(&freeStart);
		std::free(mem);
		QueryPerformanceCounter(&freeEnd);
		double freeNs = ElapsedNs(freeStart, freeEnd, freq);
		freeNsTotal += freeNs;
		freeCount += 1;
		if (freeNs > worstFreeNs)
			worstFreeNs = freeNs;
	}

	QueryPerformanceCounter(&tEnd);

	BenchResult result;
	result.avgAllocNs = allocNsTotal / (double)allocationCount;
	result.avgFreeNs = (freeCount > 0) ? (freeNsTotal / (double)freeCount) : 0.0;
	result.worstFreeUs = worstFreeNs / 1000.0;
	result.totalRunMs = ElapsedMs(tStart, tEnd, freq);

	return result;
}

void RunAllAlgoComparisionBenchmark()
{
	SIZE_T arenaSize = StandardMemoryUnits::MB * 512;
	SIZE_T allocationCount = 250000;

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
	print("\nNote: CPP_Default (malloc) header overhead is implementation-defined and not directly measurable, so it's excluded from the memory tax comparison above.");
	print("Also FixedSize_NoHeader uses 1 bit of memory per alloc in external bitmap metadata, no in memory header");

	print("\n===========================================");
}