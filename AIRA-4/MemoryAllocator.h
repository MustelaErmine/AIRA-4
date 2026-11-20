#pragma once

#include <queue>
#include <deque>

#define _MA_DEBUG

#define _MA_ALIGN 8

#ifdef _MA_DEBUG
#define IF_INITED if(!_isInited){throw std::exception("MA not inited (Debug)");}
#else
#define IF_INITED 
#endif
#ifdef _MA_DEBUG
#define IF_DESTROYED if(!_isDestroyed){throw std::exception("MA not destroyed (Debug)");}
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

	void* _base;
	void* _fsaBase;
	void* _coalesceBase;
	std::size_t _coalesceRegionSize;

	void* _fsaBases[_fsaLength];

	void* _fsaCurPtr[_fsaLength];
	void* _fsaLastInited[_fsaLength];


	class CoaHeapPtr {
	public:
		std::size_t data_size;
		void* ptr;
		CoaHeapPtr() = default;
		CoaHeapPtr(void* ptr, std::size_t data_size) : data_size(data_size), ptr(ptr) {}
	};
	class CoaHeapPtrLess
	{
	public:
		bool operator()(CoaHeapPtr right, CoaHeapPtr left) const
		{
			return right.data_size > left.data_size;
		}
	};

	std::priority_queue < CoaHeapPtr, std::vector<CoaHeapPtr>, CoaHeapPtrLess > _coaPq;


	std::size_t _curSystemSize;

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
	void _assignCoaBlock(void* block, std::size_t size, bool busy);
	void* _getCoaBlockDataPtr(void* block) const;
	void* _readEmptyFsaBlockNext(void* block) const;
	void* _readEmptyFsaBlockPrev(void* block) const;
	std::size_t _readCoaBlockSize(void* block) const;
	bool _readCoaBlockBusy(void* block) const;

	inline uintptr_t _asAddr(void* ptr) const;
	inline void* _asPtr(uintptr_t addr) const;

	inline void* _offset(void* base, std::size_t size) const;
	inline void* _negoffset(void* base, std::size_t size) const;
	
	const std::size_t _initAlloc = 100 * 1024 * 1024; // 100 Mb;

	const std::size_t _fsaSizes[_fsaLength] = { 16, 32, 64, 128, 256, 512 };
	static const std::size_t _fsaBlockAmount = 128;

	constexpr std::size_t _fsaRegionSize() const;

	const std::size_t _maxAllocSize = 10 * 1024 * 1024; // 10 Mb

	static const std::size_t _coaHeaderSize = 16;
	static const std::size_t _coaFooterSize = 8;
	static const std::size_t _coaHeadFootSize = _coaFooterSize + _coaHeaderSize;

	bool _isInited = false;
	bool _isDestroyed = false;
};

