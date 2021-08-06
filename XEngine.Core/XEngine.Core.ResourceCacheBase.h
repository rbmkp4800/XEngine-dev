#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>
#include <XLib.PoolAllocator.h>
#include <XLib.Containers.FlatHashMap.h>
#include <XLib.Delegate.h>

// TODO: replace set with hash-set

namespace XEngine::Core
{
	using ResourceUID = uint64;

	using ResourceHandle = uint32;

	enum class QueryResourceResultType : uint8
	{
		ResourceReady = 0,
		ResourceQueryPending,
		InvalidResource,
	};

	struct QueryResourceResult
	{
		union
		{
			ResourceHandle readyResourceHandle;
			ResourceQueryHandle pendingResourceQueryHandle;
		};

		QueryResourceResultType type;
	};

	template <typename Resource, typename ConcreteCache>
	class ResourceCacheBase abstract : public XLib::NonCopyable
	{
	private:
		struct Entry
		{
			Resource resource;

			uint16 referenceCount = 0;
			uint16 handleGeneration = 0;
		};

		using EntriesAllocator = XLib::PoolAllocator<Entry,
			XLib::PoolAllocatorHeapUsagePolicy::MultipleStaticChunks<4, 14>>;

		using UIDMap = XLib::FlatHashMap<ResourceUID, uint32 /*resource handle OR resource loading context handle*/>;

	protected:
		enum class LoadingResult;

	private:
		UIDMap uidMap;
		EntriesAllocator entriesAllocator;

	protected:
		inline void concreteCache_prepareLoading(Resource& resource, ResourceUID uid);
		inline void concreteCache_startLoading(Resource& resource, ResourceUID uid, float32 priority, void* asyncContext);
		inline void concreteCache_setLoadingPriority(float32 priority, void* asyncContext);
		inline void concreteCache_cancelLoading(void* asyncContext);
		inline void concreteCache_unload(Resource& resource);
		inline void concreteCache_cache(Resource& resource);
		inline void concreteCache_startReloadingFromCache(Resource& resource, /*asyncContext*/);

		void onLoadingComplete(LoadingResult loadingResult, void* asyncContext, Resource& resource);

		const Resource& get(ResourceHandle handle);

	public:
		ResourceCache() = default;
		~ResourceCache() = default;

		QueryResourceResult query(ResourceUID uid, float32 priority, callback);
		void cancelQuery(ResourceQueryHandle handle);
		void setQueryPriority(ResourceQueryHandle handle, float32 priority);

		ResourceHandle find(ResourceUID uid, bool addReference = true);

		void addReference(ResourceHandle handle);
		void removeReference(ResourceHandle handle);
		bool isValidHandle(ResourceHandle handle);
	};
}


// Definition //////////////////////////////////////////////////////////////////////////////////

namespace XEngine::Core
{
	template <typename Resource, typename ConcreteCache>
	QueryResourceResult ResourceCacheBase<Resource, ConcreteCache>::query(ResourceUID uid, float32 priority, callback)
	{

	}
}
