#include "MemoryAllocator.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cassert>
#include <iostream>
#include <vector>
#include <set>

MemoryAllocator::~MemoryAllocator() {
	IF_DESTROYED
}

void MemoryAllocator::init() {
	init(_initAlloc);
}

void MemoryAllocator::init(std::size_t initialSize) {
	_allocInSystem(_initAlloc);
	_curSystemSize = initialSize;
	_assignBasePtrs();
	_initFsaRegion();
	_initCoalesceRegion();
	_isInited = true;
}
void MemoryAllocator::destroy() {
	IF_INITED
#ifdef _MA_DEBUG
	assert(_allocated_blocks == 0);
#endif
	_deallocInSystem();
	_isDestroyed = true;
}

void* MemoryAllocator::alloc(std::size_t size) {
	IF_INITED
	_allocated_blocks++;
	if (size <= _fsaSizes[_fsaLength - 1]) {
		return _allocInFSA(size);
	}
	if (size <= _maxAllocSize) {
		return _allocInCoalesce(ALIGN(size));
	}
	return _allocInOS(size);
}
void MemoryAllocator::free(void* p) {
	IF_INITED
	_allocated_blocks--;
	if (_base <= p && p < (_offset(_fsaBase, _fsaRegionSize()))) {
		_freeInFSA(p);
	}
	else if (p < (_offset(_fsaBase, _fsaRegionSize())) && p <= (_offset(_coalesceBase, _coalesceRegionSize))) {
		_freeInCoalesce(p);
	}
	else {
		_freeInOS(p);
	}
}

#ifdef _MA_DEBUG
void MemoryAllocator::dumpStat() const {
	IF_INITED
	std::cout << "--- MemoryAllocator STATS ---" << std::endl;

	std::cout << "-- FSA --" << std::endl;

	for (std::size_t i = 0; i < _fsaLength; i++) {
		std::cout << "- " << _fsaSizes[i] << " section -" << std::endl;
		auto curPtr = _fsaCurPtr[i];
		int busy = _fsaBlockAmount;
		while (curPtr != nullptr) {
			busy--;
			curPtr = _readEmptyFsaBlockNext(curPtr);
		}
		std::cout << "Busy or uninited: " << busy << " out of " << _fsaBlockAmount << std::endl;
	}

	std::cout << "-- Coalesce --" << std::endl;

	int block_idx = 0;
	auto curPtr = _coalesceBase;

	std::size_t busySize = 0;
	std::size_t busyAmount = 0;
	std::size_t freeSize = 0;
	std::size_t freeAmount = 0;

	while (curPtr < _offset(_coalesceBase, _coalesceRegionSize)) {
		std::cout << "- Block " << block_idx << " -" << std::endl;
		auto block_size = _readCoaBlockSize(curPtr);
		auto block_busy = _readCoaBlockBusy(curPtr);

		if (block_busy) {
			busyAmount++;
			busySize += block_size;
		}
		else {
			freeSize += block_size;
			freeAmount++;
		}
		curPtr = _offset(curPtr, block_size);
	}

	std::cout << "Busy size: " << busySize << std::endl;
	std::cout << "Free size: " << freeSize << std::endl;

	std::cout << "Busy amount: " << busyAmount << std::endl;
	std::cout << "Free amount: " << freeAmount << std::endl;

	std::cout << "--- MemoryAllocator STATS END ---" << std::endl;
}
void MemoryAllocator::dumpBlocks() const {
	IF_INITED
	std::cout << "--- MemoryAllocator BLOCKS ---" << std::endl;

	std::cout << "-- FSA --" << std::endl;

	for (std::size_t i = 0; i < _fsaLength; i++) {
		std::cout << "- " << _fsaSizes[i] << " section -" << std::endl;

		std::set<void*> frees = std::set<void*>();

		auto curPtr = _fsaCurPtr[i];
		while (curPtr != nullptr) {
			frees.insert(curPtr);
			curPtr = _readEmptyFsaBlockNext(curPtr);
		}

		auto base = _fsaBases[i];
		for (int j = 0; j < _fsaBlockAmount; j++) {
			auto ptr = _offset(base, j * _fsaSizes[i]);
			std::cout << "Block " << j << " is ";
			
			if (frees.contains(ptr)) {
				std::cout << "free";
			}
			else {
				std::cout << "busy or uninited";
			}
			std::cout << std::endl;
		}
	}

	std::cout << "-- Coalesce --" << std::endl;

	int block_idx = 0;
	auto curPtr = _coalesceBase;
	while (curPtr != _offset(_coalesceBase, _coalesceRegionSize)) {
		std::cout << "- Block " << block_idx++ << " -" << std::endl;
		auto block_size = _readCoaBlockSize(curPtr);
		auto block_busy = _readCoaBlockBusy(curPtr);

		std::cout << "Size: " << block_size << std::endl;
		std::cout << "Busy: " << std::boolalpha << block_busy << std::endl;
		curPtr = _offset(curPtr, block_size);
	}

	std::cout << "--- MemoryAllocator BLOCKS END ---" << std::endl;
}
#endif

void MemoryAllocator::_allocInSystem(std::size_t size) {
	_base = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
}
void MemoryAllocator::_deallocInSystem() {
	VirtualFree(_base, 0, MEM_RELEASE);
}
void* MemoryAllocator::_allocInFSA(std::size_t size) {
	auto idx = _detBlockSizeIdx(size);
	auto block_size = _fsaSizes[idx];
	auto& ptr = _fsaCurPtr[idx];
	auto base = _fsaBases[idx];

	if (ptr == nullptr) {
		return nullptr;
	}

	auto next = _readEmptyFsaBlockNext(ptr);

	if (next == nullptr) {
		auto last_ptr = ptr;
		ptr = nullptr;
		return last_ptr;
	}

	auto last_ptr = ptr;

	if (_fsaLastInited[idx] == ptr) {
		if (next == _offset(base, block_size * (_fsaBlockAmount - 1))) {
			_assignEmptyFsaBlock(next, nullptr);
		}
		else {
			_assignEmptyFsaBlock(next, _offset(next, block_size));
		}
		_fsaLastInited[idx] = next;
	}

	ptr = next;
	return last_ptr;
}
void MemoryAllocator::_freeInFSA(void* p) {
	auto idx = _detBlockSizeIdx(p);
	auto block_size = _fsaSizes[idx];
	auto& ptr = _fsaCurPtr[idx];
	
	_assignEmptyFsaBlock(p, ptr);
	ptr = p;
}
void* MemoryAllocator::_allocInCoalesce(std::size_t size) {
	if (_coaPq.empty())
		return nullptr;

	auto temp_stack = std::deque<CoaHeapPtr>();
	
	while (!_coaPq.empty() && _coaPq.top().data_size < size) {
		temp_stack.push_back(_coaPq.top());
		_coaPq.pop();
	}
	if (_coaPq.empty()) {
		while (!temp_stack.empty()) {
			_coaPq.push(temp_stack.back());
			temp_stack.pop_back();
		}
		return nullptr;
	}

	auto free_block = _coaPq.top();
	_coaPq.pop();
	auto free_block_size = free_block.data_size + _coaHeadFootSize;

	while (!temp_stack.empty()) {
		_coaPq.push(temp_stack.back());
		temp_stack.pop_back();
	}

	auto alloc_ptr = free_block.ptr;
	auto alloc_block_size = size + _coaHeadFootSize;

	auto new_empty_ptr = _offset(alloc_ptr, alloc_block_size);

	auto new_empty_block_size = free_block_size - alloc_block_size;

	if (new_empty_block_size > _coaHeadFootSize) {
		_assignCoaBlock(new_empty_ptr, new_empty_block_size, false);

		auto new_empty_data_size = new_empty_block_size - _coaHeadFootSize;
		auto new_empty_block = CoaHeapPtr(new_empty_ptr, new_empty_data_size);

		_coaPq.push(new_empty_block);
	}
	else {
		alloc_block_size += new_empty_block_size;
	}

	_assignCoaBlock(alloc_ptr, alloc_block_size, true);

	auto alloc_data_ptr = _offset(alloc_ptr, _coaHeaderSize);

	return alloc_data_ptr;
}
void MemoryAllocator::_freeInCoalesce(void* ptr) {
	auto real_ptr = _negoffset(ptr, _coaHeaderSize);
	auto block_size = _readCoaBlockSize(real_ptr);

	void* new_empty_ptr = real_ptr;
	std::size_t new_empty_block_size = block_size;

	if (real_ptr != _coalesceBase) {
		auto block_int = static_cast<uintptr_t*>(real_ptr);

		auto prev_size = block_int[-1];
		
		auto prev_ptr = _negoffset(real_ptr, prev_size);
		auto prev_busy = _readCoaBlockBusy(prev_ptr);

		if (!prev_busy) {
			new_empty_ptr = prev_ptr;
			new_empty_block_size += prev_size;
		}
	}
	if (_offset(real_ptr, block_size) != _offset(_coalesceBase, _coalesceRegionSize)) {
		auto next_ptr = _offset(real_ptr, block_size);
		auto next_size = _readCoaBlockSize(next_ptr);
		auto next_busy = _readCoaBlockBusy(next_ptr);

		if (!next_busy) {
			new_empty_block_size += next_size;
		}
	}

	_assignCoaBlock(new_empty_ptr, new_empty_block_size, false);
	auto new_empty_data_size = new_empty_block_size - _coaHeadFootSize;

	_coaPq.push(CoaHeapPtr(new_empty_ptr, new_empty_data_size));
}
void* MemoryAllocator::_allocInOS(std::size_t size) {
	return std::malloc(size);
}
void MemoryAllocator::_freeInOS(void* p) {
	return std::free(p);
}
std::size_t MemoryAllocator::_detBlockSizeIdx(void* p) const {
	for (std::size_t i = _fsaLength; i > 0; --i) {
		if (p >= _fsaBases[i - 1]) {
			return i - 1;
		}
	}
	return SIZE_MAX;
}
std::size_t MemoryAllocator::_detBlockSizeIdx(std::size_t size) const {
	for (std::size_t i = 0; i < _fsaLength; i++) {
		if (size <= _fsaSizes[i]) {
			return i;
		}
	}
	return SIZE_MAX;
}

inline uintptr_t MemoryAllocator::_asAddr(void* ptr) const {
	return reinterpret_cast<uintptr_t>(ptr);
}
inline void* MemoryAllocator::_asPtr(uintptr_t addr) const {
	return reinterpret_cast<void*>(addr);
}
void MemoryAllocator::_assignEmptyFsaBlock(void* block, void* next) {
	uintptr_t* blockInt = static_cast<uintptr_t*>(block);
	blockInt[0] = _asAddr(next);
}
std::size_t MemoryAllocator::_readCoaBlockSize(void* block) const {
	uintptr_t* blockInt = static_cast<uintptr_t*>(block);
	return static_cast<std::size_t>(blockInt[0]);
}
bool MemoryAllocator::_readCoaBlockBusy(void* block) const {
	uintptr_t* blockInt = static_cast<uintptr_t*>(block);
	return static_cast<bool>(blockInt[1]);
}
void MemoryAllocator::_assignCoaBlock(void* block, std::size_t size, bool busy) {
	std::size_t* _blockInt = static_cast<std::size_t*>(block);
	_blockInt[0] = size; // size
	_blockInt[1] = static_cast<std::size_t>(busy); // is busy
	static_cast<std::size_t*>(_offset(block, size))[-1] = size;
}
void* MemoryAllocator::_getCoaBlockDataPtr(void* block) const {
	return _offset(block, _coaHeaderSize);
}
//next empty
void* MemoryAllocator::_readEmptyFsaBlockNext(void* block) const {
	uintptr_t* blockInt = static_cast<uintptr_t*>(block);
	return _asPtr(blockInt[0]);
}

void MemoryAllocator::_assignBasePtrs() {
	_fsaBase = _base;
	_coalesceBase = _offset(_base, _fsaRegionSize());
	_coalesceRegionSize = _curSystemSize - _fsaRegionSize();
}
// empty fsa block: [nextemptyblockptr]/8 ...
void MemoryAllocator::_initFsaRegion() {
	std::size_t current_offset = 0;
	for (std::size_t i = 0; i < _fsaLength; i++) {
		std::size_t curSize = _fsaSizes[i];
		void* curBase = _offset(_base, current_offset);
		_fsaBases[i] = curBase;
		_fsaCurPtr[i] = curBase;

		_assignEmptyFsaBlock(curBase, _offset(curBase, curSize));
		
		current_offset += _fsaBlockAmount * curSize;

		_fsaLastInited[i] = curBase;
	}
}
// colaesce region: [region size (w/ head & foot)]/8 [is busy]/8 [data...] [region size]/8
void MemoryAllocator::_initCoalesceRegion() {
	_coaPq = std::priority_queue< CoaHeapPtr, std::vector<CoaHeapPtr>, CoaHeapPtrLess>();

	//only priority queue (min-heap)
	//on free - look for neighbours through the pq
	//on alloic - look for top

	auto data_size = _coalesceRegionSize - _coaHeadFootSize;

	_assignCoaBlock(_coalesceBase, _coalesceRegionSize, false);
	_coaPq.push(CoaHeapPtr(_coalesceBase, data_size));
}

void* MemoryAllocator::_offset(void* base, std::size_t size) const {
	return static_cast<void*>(static_cast<uint8_t* > (base)+size);
}

void* MemoryAllocator::_negoffset(void* base, std::size_t size) const {
	return static_cast<void*>(static_cast<uint8_t* > (base)-size);
}

constexpr std::size_t MemoryAllocator::_fsaRegionSize() const {
	std::size_t out = 0;
	for (std::size_t i = 0; i < _fsaLength; i++) {
		out += _fsaBlockAmount * _fsaSizes[i];
	}
	return out;
}