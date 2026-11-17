#pragma once

#include <algorithm>

#define _MA_ALIGN 8

class MemoryAllocator
{
public:
	MemoryAllocator();
	virtual ~MemoryAllocator();
	
	virtual void init();
	virtual void destroy();

	virtual void* alloc(std::size_t size);
	virtual void free(void* p);

	virtual void dumpStat() const;
	virtual void dumpBlocks() const;
private:

	void* _base;
	void allocSystem(std::size_t size);
	void deallocSystem();
};

