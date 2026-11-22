#pragma once

#include <queue>
#include <deque>

#define _MA_DEBUG

#define _MA_ALIGN 8

#ifdef _MA_DEBUG
#define IF_INITED do { if(!_isInited){throw std::exception("MA not inited (Debug)");} } while (0)
#else
#define IF_INITED 
#endif
#ifdef _MA_DEBUG
#define IF_DESTROYED do { if(!_isDestroyed){throw std::exception("MA not destroyed (Debug)");} } while (0)
#else
#define IF_DESTROYED 
#endif

#define CEIL(a, b) (((a) + (b) - 1) / (b))
#define ALIGN(a) CEIL(a, _MA_ALIGN)

class MemoryAllocator
{
public:
	MemoryAllocator() = default;
	virtual ~MemoryAllocator();

	MemoryAllocator(const MemoryAllocator&) = delete;
	MemoryAllocator(MemoryAllocator&&) = default;
	
	virtual void init();
	virtual void init(std::size_t initialSize);
	virtual void destroy();

	virtual void* alloc(std::size_t size);
	virtual void free(void* p);

#ifdef _MA_DEBUG
	virtual void dumpStat() const;
	virtual void dumpBlocks() const;
#endif
private:
	static const std::size_t _fsaLength = 6;

	void* _base = nullptr;
	void* _fsaBase = nullptr;
	void* _coalesceBase = nullptr;
	std::size_t _coalesceRegionSize = 0;

	void* _fsaBases[_fsaLength] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};

	void* _fsaCurPtr[_fsaLength] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
	void* _fsaLastInited[_fsaLength] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };

	std::size_t _curSystemSize = 0;

	void** _coaFreeList = nullptr;
	std::size_t _coaFreeListLength = 0;

	void _allocInSystem(std::size_t size);
	void _deallocInSystem();

	void* _allocInFSA(std::size_t size);
	void _freeInFSA(void* p);
	void* _allocInCoalesce(std::size_t size);
	void _freeInCoalesce(void* p);
	void* _allocInOS(std::size_t size);
	void _freeInOS(void* p);

	std::size_t _detBlockSizeIdx(void* p) const;
	std::size_t _detBlockSizeIdx(std::size_t size) const;

	void _assignBasePtrs();
	void _initFsaRegion();
	void _initCoalesceRegion();

	void _assignEmptyFsaBlock(void* block, void* next);
	void* _readEmptyFsaBlockNext(void* block) const;

	void _assignCoaBlock(void* block, std::size_t size, bool busy);
	std::size_t _readCoaBlockSize(void* block) const;
	bool _readCoaBlockBusy(void* block) const;
	void* _getCoaBlockDataPtr(void* block) const;

	inline uintptr_t _asAddr(void* ptr) const;
	inline void* _asPtr(uintptr_t addr) const;

	inline void* _offset(void* base, std::size_t size) const;
	inline void* _negoffset(void* base, std::size_t size) const;

	inline void*& _freeListAt(std::size_t idx);
	inline bool _freeListEmpty();
	void _freeListInsert(void* elem);
	void _freeListInsertAt(void* elem, std::size_t idx);
	void _freeListRemoveAt(std::size_t idx);
	std::size_t _freeListFind(void* elem);
	
	const std::size_t _initAlloc = 100 * 1024 * 1024; // 100 Mb;

	const std::size_t _fsaSizes[_fsaLength] = { 16, 32, 64, 128, 256, 512 };
	static const std::size_t _fsaBlockAmount = 128;

	std::size_t _fsaRegionSize() const;

	static const std::size_t _maxAllocSize = 10 * 1024 * 1024; // 10 Mb

	static const std::size_t _coaHeaderSize = 16;
	static const std::size_t _coaFooterSize = 8;
	static const std::size_t _coaHeadFootSize = _coaFooterSize + _coaHeaderSize;

	static const std::size_t _coaFreeListCapacity = 1024;
	static const std::size_t _coaFreeListSize = _coaFreeListCapacity * sizeof(void**);

	bool _isInited = false;
	bool _isDestroyed = false;

	int _allocated_blocks = 0;
};

