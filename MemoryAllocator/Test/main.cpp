#include <iostream>
#include <Platform.h>
#include <allocator.h>
#include "AllAlgoComp.h"
int main(int argc, char** argv)
{
	RunAllAlgoComparisionBenchmark();
	RunMixedTypeComparisionBenchmark();
	return 0;
}