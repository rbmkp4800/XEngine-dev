#pragma once

#include "XLib.h"
#include "XLib.NonCopyable.h"
#include "XLib.Heap.h"
#include "XLib.Debug.h"

namespace XLib
{
	struct PoolAllocatorHeapUsagePolicy abstract final
	{
		class SingleDynamicChunk abstract final {};	// elements can be rebased

		template <uint32 minBufferSizeLog2, uint32 maxBufferSizeLog2>
		class MultipleStaticChunks abstract final // elements are static
			{ static_assert(minBufferSizeLog2 <= maxBufferSizeLog2, "invalid params"); };
	};

	template <typename Type, typename HeapUsagePolicy>
	class PoolAllocator;

	template <typename Type>
	class PoolAllocator<Type, PoolAllocatorHeapUsagePolicy::SingleDynamicChunk> : public NonCopyable
	{
	private:
		struct Block
		{
			union
			{
				Type value;
				uint32 nextFreeBlock;
			};
		};

		HeapPtr<Block> buffer;
		uint32 bufferSize;
		uint32 firstFreeBlock = 0;

	public:
		inline PoolAllocator(uint32 initialBufferSize = 16)
			: buffer(initialBufferSize), bufferSize(initialBufferSize)
		{
			for (uint32 i = 0; i < bufferSize - 1; i++)
				buffer[i].nextFreeBlock = i + 1;
			buffer[bufferSize - 1].nextFreeBlock = uint32(-1);
		}
		~PoolAllocator() = default;

		uint32 allocate()
		{
			if (firstFreeBlock == uint32(-1))
			{
				uint32 newBufferSize = bufferSize * 2;
				buffer.resize(newBufferSize);
				for (uint32 i = bufferSize + 1; i < newBufferSize - 1; i++)
					buffer[i].nextFreeBlock = i + 1;
				buffer[newBufferSize - 1].nextFreeBlock = uint32(-1);
				uint32 allocatedBlock = bufferSize;
				firstFreeBlock = bufferSize + 1;
				bufferSize = newBufferSize;
				return allocatedBlock;
			}
			else
			{
				uint32 allocatedBlock = firstFreeBlock;
				firstFreeBlock = buffer[firstFreeBlock].nextFreeBlock;
				return allocatedBlock;
			}
		}
		inline void release(uint32 blockId)
		{
			XASSERT(blockId < bufferSize, "invalid blockId");

			buffer[blockId].nextFreeBlock = firstFreeBlock;
			firstFreeBlock = blockId;
		}
		inline Type& getBlock(uint32 blockId) { return buffer[blockId].value; }
	};

	template <typename Type, uint32 minBufferSizeLog2, uint32 maxBufferSizeLog2>
	class PoolAllocator<Type, PoolAllocatorHeapUsagePolicy::MultipleStaticChunks<
		minBufferSizeLog2, maxBufferSizeLog2>> : public NonCopyable
	{
	private:
		static constexpr uint32 chunksLimit = maxBufferSizeLog2 - minBufferSizeLog2 + 1;

		union Block
		{
			Type value;
			uint32 nextFreeBlockId;
		};
		struct Chunk
		{
			Block *buffer;
			uint32 firstFreeBlockId;
		};

		inline uint32 computeChunkSize(uint8 chunkIndex)
			{ return 1 << (minBufferSizeLog2 + chunkIndex); }

		Chunk chunks[chunksLimit];
		uint8 allocatedChunkCount;

	public:
		PoolAllocator()
		{
			uint32 bufferSize = computeChunkSize(0);
			chunks[0].buffer = Heap::Allocate<Block>(bufferSize);
			chunks[0].firstFreeBlockId = 0;
			allocatedChunkCount = 1;

			Block *buffer = chunks[0].buffer;
			for (uint32 i = 0; i < bufferSize - 1; i++)
				buffer[i].nextFreeBlockId = i + 1;
			buffer[bufferSize - 1].nextFreeBlockId = uint32(-1);
		}
		~PoolAllocator()
		{
			for (uint8 i = 0; i < allocatedChunkCount; i++)
				Heap::Release(chunks[i].buffer);
			allocatedChunkCount = 0;
		}

		Type* allocate()
		{
			for (uint8 i = 0; i < allocatedChunkCount; i++)
			{
				Chunk &chunk = chunks[i];
				if (chunk.firstFreeBlockId != uint32(-1))
				{
					uint32 blockId = chunk.firstFreeBlockId;
					Block &block = chunk.buffer[blockId];
					chunk.firstFreeBlockId = block.nextFreeBlockId;
					construct(block.value);
					return &block.value;
				}
			}

			if (allocatedChunkCount >= chunksLimit)
				return nullptr;

			Chunk &newChunk = chunks[allocatedChunkCount];
			uint32 bufferSize = computeChunkSize(allocatedChunkCount);
			newChunk.buffer = Heap::Allocate<Block>(bufferSize);
			newChunk.firstFreeBlockId = 1;
			allocatedChunkCount++;

			Block *buffer = newChunk.buffer;
			for (uint32 i = 0; i < bufferSize - 1; i++)
				buffer[i].nextFreeBlockId = i + 1;
			buffer[bufferSize - 1].nextFreeBlockId = uint32(-1);

			construct(buffer[0].value);
			return &buffer[0].value;
		}

		void release(Type* _block)
		{
			Block *block = to<Block*>(_block);
			for (uint8 i = 0; i < allocatedChunkCount; i++)
			{
				Chunk &chunk = chunks[i];
				if (block >= chunk.buffer && block < chunk.buffer + computeChunkSize(i))
				{
					uintptr offset = uintptr(block) - uintptr(chunk.buffer);
					XASSERT(offset % sizeof(Block) == 0, "invalid pointer");

					block->nextFreeBlockId = chunk.firstFreeBlockId;
					chunk.firstFreeBlockId = uint32(offset / sizeof(Block));
					return;
				}
			}

			XFATAL("invalid pointer");
		}
	};
}