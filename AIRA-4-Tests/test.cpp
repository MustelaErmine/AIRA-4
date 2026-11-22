#include "pch.h"

#include "../AIRA-4/MemoryAllocator.h"

TEST(FSA, Simple) {
	// Arrange
	auto ma = MemoryAllocator();
	ma.init();

	ma.dumpBlocks();
	// Act
	int* pa = static_cast<int*>(ma.alloc(16));
	int* pb = static_cast<int*>(ma.alloc(16));
	int* pc = static_cast<int*>(ma.alloc(16));
	*pa = 42;
	*pb = INT_MAX;
	*pc = 44;

	// Assert
	EXPECT_EQ(*pa, 42);
	EXPECT_EQ(*pb, INT_MAX);
	EXPECT_EQ(*pc, 44);

	//ma.dumpStat();
	ma.dumpBlocks();

	ma.free(pa);
	ma.free(pb);
	ma.free(pc);

	ma.dumpStat();

	ma.destroy();
}

TEST(FSA, All) {
	// Arrange
	auto ma = MemoryAllocator();
	ma.init();

	// Act
	int* ps[128];

	for (int size = 16; size <= 512; size *= 2) {
		for (int i = 0; i < 128; i++) {
			ps[i] = static_cast<int*>(ma.alloc(size));
			*ps[i] = i;
		}
		for (int i = 0; i < 128; i++) {

			// Assert
			EXPECT_EQ(*ps[i], i);
		}
		ma.dumpStat();
		for (int i = 0; i < 128; i++) {
			ma.free(ps[i]);
		}
	}

	ma.dumpStat();

	ma.destroy();
}

TEST(System, Simple) {
	// Arrange
	auto ma = MemoryAllocator();

	ma.init();
	
	//Act
	void* ptr = ma.alloc(11 * 1024 * 1024);
	ma.free(ptr);
	
	ma.destroy();
}

TEST(Coallesce, Simple) {
	// Arrange
	auto ma = MemoryAllocator();

	ma.init();
	
	// Act
	int* ptr = static_cast<int*>(ma.alloc(1024));

	ptr[0] = 42;
	ptr[1] = 1042;

	// Assert
	EXPECT_EQ(ptr[0], 42);
	EXPECT_EQ(ptr[1], 1042);

	ma.free(ptr);
	
	ma.destroy();
}

TEST(Coallesce, Simple2) {
	// Arrange
	auto ma = MemoryAllocator();

	ma.init();
	
	// Act
	int* ptra = static_cast<int*>(ma.alloc(1024));
	int* ptrb = static_cast<int*>(ma.alloc(1024));
	int* ptrc = static_cast<int*>(ma.alloc(1024));

	ptra[0] = 42;
	ptrb[1] = 1042;
	ptrc[2] = 2084;

	// Assert
	EXPECT_EQ(ptra[0], 42);
	EXPECT_EQ(ptrb[1], 1042);
	EXPECT_EQ(ptrc[2], 2084);
	
	ma.dumpBlocks();

	ma.free(ptrc);
	ma.free(ptra);
	ma.free(ptrb);

	ma.dumpBlocks();
	
	ma.destroy();
}

TEST(Coallesce, Full) {
	// Arrange
	auto ma = MemoryAllocator();

	ma.init();

	// Act
	int* ptra = static_cast<int*>(ma.alloc(104728552));

	ptra[0] = 42;

	// Assert
	EXPECT_EQ(ptra[0], 42);

	ma.dumpBlocks();

	ma.free(ptra);

	ma.dumpBlocks();

	ma.destroy();
}

TEST(Coallesce, Coal) {
	// Arrange
	auto ma = MemoryAllocator();

	ma.init();

	// Act
	int* ptra = static_cast<int*>(ma.alloc(1024));
	int* ptrb = static_cast<int*>(ma.alloc(1024));
	int* ptrc = static_cast<int*>(ma.alloc(1024));
	int* ptrd = static_cast<int*>(ma.alloc(1024));
	int* ptre = static_cast<int*>(ma.alloc(1024));

	// Assert

	ma.free(ptrb);
	ma.free(ptrd);

	ma.dumpBlocks();

	ma.free(ptrc);

	ma.dumpBlocks();

	ma.free(ptra);
	ma.free(ptre);

	ma.destroy();
}