#pragma once

#include "XLib.h"
#include "XLib.Containers.AVLTreeLogic.h"
#include "XLib.NonCopyable.h"

// TODO: Do smthing with const iterators.
// TODO: FlatBinaryTreeMap::KeyValuePair should return key by const-ref so user can't mess with it.

namespace XLib
{
	// FlatBinaryTreeMap ///////////////////////////////////////////////////////////////////////////

	// 8 bytes of aux data per element. 2^20 elements max.
	template <typename Key, typename Value>
	class FlatBinaryTreeMap : public XLib::NonCopyable
	{
	public:
		struct KeyValuePair
		{
			Key key;
			Value value;
		};

	private:
		class NodeAdapter;
		using TreeLogic = AVLTreeLogic<NodeAdapter>;

		struct Entry
		{
			KeyValuePair data;
			uint64 aux;
		};

	private:
		Entry* buffer = nullptr;
		uint32 capacity = 0;
		uint32 size = 0;
		uint32 rootNodeIndex = 0;

	public:
		class Iterator;

	public:
		FlatBinaryTreeMap() = default;
		~FlatBinaryTreeMap() = default;

		inline Iterator find(const Key& key) const;
		inline Value* findValue(const Key& key) const;

		inline Iterator insert(const Key& key, const Value& value);
		inline Iterator insertOrAssign(const Key& key, const Value& value);

		inline void remove(const Iterator& iterator);
		inline bool remove(const Key& key);

		inline uint32 getSize() const { return size; }
		inline bool isEmpty() const { return size == 0; }

		inline Iterator begin() const;
		inline Iterator end() const;
	};

	template <typename Key, typename Value>
	class FlatBinaryTreeMap<Key, Value>::Iterator
	{
		template <typename Key, typename Value>
		friend class FlatBinaryTreeMap;

	private:
		FlatBinaryTreeMap* parent = nullptr;
		uint32 index = 0;

	private:
		inline Iterator(FlatBinaryTreeMap* parent, uint32 index) : parent(parent), index(index) {}

	public:
		Iterator() = default;
		~Iterator() = default;

		inline void operator ++ ();
		inline void operator -- ();

		inline operator KeyValuePair* () const;
		inline KeyValuePair& operator * () const { return parent->buffer[index].data; }
		inline KeyValuePair* operator -> () const { return &parent->buffer[index].data; }
		inline bool operator == (const Iterator& that) const { return parent == that.parent && index == that.index; }
		inline bool operator != (const Iterator& that) const { return !this->operator==(); }
	};


	// IntrusiveBinaryTree /////////////////////////////////////////////////////////////////////////

	class IntrusiveBinaryTreeNodeHook : public XLib::NonCopyable
	{
		template <typename Node, IntrusiveBinaryTreeNodeHook(Node::*), typename NodeComparator>
		friend class IntrusiveBinaryTree;

	private:
		void* children[2] = {};
		uintptr parent_aux = 0;

	public:
		IntrusiveBinaryTreeNodeHook() = default;
		~IntrusiveBinaryTreeNodeHook() = default;
	};

	template <typename Node, IntrusiveBinaryTreeNodeHook(Node::* nodeHook), typename NodeComparator>
	class IntrusiveBinaryTree : public NonCopyable
	{
	private:
		class NodeAdapter;
		using TreeLogic = AVLTreeLogic<NodeAdapter>;

		static constexpr uintptr NodeParentPtrAuxMask = uintptr(0b11);
		static constexpr uintptr NodePtrMask = ~NodeParentPtrAuxMask;

	public:
		Node* root = nullptr;

	private:
		static inline IntrusiveBinaryTreeNodeHook& GetNodeHook(Node& node) { return ((node).*nodeHook); }

	public:
		class Iterator;

	public:
		IntrusiveBinaryTree() = default;
		~IntrusiveBinaryTree() = default;

		template <typename Key>
		inline Iterator find(const Key& key) const;

		inline Iterator insert(Node& node, Iterator* outExistingNode = nullptr);
		inline Iterator remove(Node& node);

		inline void clear();

		inline Iterator begin() const;
		inline Iterator end() const;

		inline bool isEmpty() const { return root == nullptr; }
	};

	template <typename Node, IntrusiveBinaryTreeNodeHook(Node::* nodeHook), typename NodeComparator>
	class IntrusiveBinaryTree<Node, nodeHook, NodeComparator>::Iterator
	{
		template <typename Node, IntrusiveBinaryTreeNodeHook(Node::*), typename NodeComparator>
		friend class IntrusiveBinaryTree;

	private:
		Node* node = nullptr;

	private:
		inline Iterator(Node* node) : node(node) {}

	public:
		Iterator() = default;
		~Iterator() = default;

		inline void operator ++ ();
		inline void operator -- ();

		inline operator Node* () const { return node; }
		inline Node& operator * () const { return *node; }
		inline Node* operator -> () const { return node; }
		inline bool operator == (const Iterator& that) const { return node == that.node; }
		inline bool operator != (const Iterator& that) const { return node != that.node; }
	};
}

// Node comparator template:
//
//	class MyNodeComparator abstract final
//	{
//	public:
//		static ordering Compare(const MyNode& left, const MyNode& right);
//		static ordering Compare(const MyNode& left, const SomeOtherKey& right);
//	};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Definition //////////////////////////////////////////////////////////////////////////////////////

namespace XLib
{
	// FlatBinaryTreeMap::NodeAdapter //////////////////////////////////////////////////////////////

	template <typename Key, typename Value>
	class FlatBinaryTreeMap<Key, Value>::NodeAdapter
	{
	private:
		Entry* entries;
		uint32 entryCount;

	private:
		static constexpr uint32 NodeRefMask = 0xF'FF'FF;
		static constexpr uint64 NodeRefMaskU64 = NodeRefMask;

	public:
		using NodeRef = uint32;
		static constexpr NodeRef ZeroNodeRef = NodeRef(NodeRefMask);

		inline NodeRef getNodeParent(NodeRef node) const;
		inline NodeRef getNodeChild(NodeRef node, uint8 childIndex) const;
		inline sint8 getNodeBalanceFactor(NodeRef node) const;

		inline void setNodeParent(NodeRef node, NodeRef parentToSet);
		inline void setNodeChild(NodeRef node, uint8 childIndex, NodeRef childToSet);
		inline void setNodeBalanceFactor(NodeRef node, sint8 balanceFactor);

		inline void setNodeFull(NodeRef node, NodeRef parent, NodeRef leftChild, NodeRef rightChild, sint8 balanceFactor);

		inline ordering compareNodeTo(NodeRef left, NodeRef right) const;

	public:
		inline NodeAdapter(Entry* entries, uint32 entryCount) : entries(entries), entryCount(entryCount) { XAssert(entryCount < NodeRefMask); }
	};

	template <typename Key, typename Value>
	inline auto FlatBinaryTreeMap<Key, Value>::NodeAdapter::getNodeParent(NodeRef node) const -> NodeRef
	{
		XAssert(node < entryCount);
		return NodeRef(entries[node].aux & NodeRefMask);
	}

	template <typename Key, typename Value>
	inline auto FlatBinaryTreeMap<Key, Value>::NodeAdapter::getNodeChild(NodeRef node, uint8 childIndex) const -> NodeRef
	{
		XAssert(childIndex < 2);
		XAssert(node < entryCount);
		return NodeRef((entries[node].aux >> (childIndex * 20 + 20)) & NodeRefMask);
	}

	template <typename Key, typename Value>
	inline auto FlatBinaryTreeMap<Key, Value>::NodeAdapter::getNodeBalanceFactor(NodeRef node) const -> sint8
	{
		XAssert(node < entryCount);
		return NodeRef(((entries[node].aux >> 60) & 3) - 1);
	}

	template <typename Key, typename Value>
	inline void FlatBinaryTreeMap<Key, Value>::NodeAdapter::setNodeParent(NodeRef node, NodeRef parentToSet)
	{
		XAssert(node < entryCount);
		XAssert(parentToSet < entryCount || parentToSet == ZeroNodeRef);
		entries[node].aux &= ~NodeRefMaskU64;
		entries[node].aux |= parentToSet;
	}

	template <typename Key, typename Value>
	inline void FlatBinaryTreeMap<Key, Value>::NodeAdapter::setNodeChild(NodeRef node, uint8 childIndex, NodeRef childToSet)
	{
		XAssert(childIndex < 2);
		XAssert(node < entryCount);
		XAssert(childToSet < entryCount || childToSet == ZeroNodeRef);
		const uint8 offset = (childIndex * 20 + 20);
		entries[node].aux &= ~(NodeRefMaskU64 << offset);
		entries[node].aux |= childToSet << offset;
	}

	template <typename Key, typename Value>
	inline void FlatBinaryTreeMap<Key, Value>::NodeAdapter::setNodeBalanceFactor(NodeRef node, sint8 balanceFactor)
	{
		XAssert(node < entryCount);
		XAssert(balanceFactor >= -1 && balanceFactor <= +1);
		entries[node].aux &= ~(uint64(3) << 60);
		entries[node].aux |= uint64(balanceFactor + 1) << 60;
	}

	template <typename Key, typename Value>
	inline void FlatBinaryTreeMap<Key, Value>::NodeAdapter::
		setNodeFull(NodeRef node, NodeRef parent, NodeRef leftChild, NodeRef rightChild, sint8 balanceFactor)
	{
		XAssert(node < entryCount);
		XAssert(parent < entryCount || parent == ZeroNodeRef);
		XAssert(leftChild < entryCount || leftChild == ZeroNodeRef);
		XAssert(rightChild < entryCount || rightChild == ZeroNodeRef);
		XAssert(balanceFactor >= -1 && balanceFactor <= +1);
		const uint64 aux =
			uint64(parent) |
			(uint64(leftChild) << 20) |
			(uint64(rightChild) << 40) |
			(uint64(balanceFactor + 1) << 60);
		entries[node].aux = entries;
	}


	// FlatBinaryTreeMap ///////////////////////////////////////////////////////////////////////////

	template <typename Key, typename Value>
	inline auto FlatBinaryTreeMap<Key, Value>::find(const Key& key) const -> Iterator
	{
		if (!size)
			return Iterator();

		NodeAdapter nodeAdapter(buffer, size);
		const NodeAdapter::NodeRef foundNodeRef = TreeLogic::Find(nodeAdapter, rootNodeIndex, key);
		if (foundNodeRef == NodeAdapter::ZeroNodeRef)
			return Iterator();

		XAssert(foundNodeRef < size);
		return Iterator(this, foundNodeRef);
	}

	template <typename Key, typename Value>
	inline auto FlatBinaryTreeMap<Key, Value>::findValue(const Key& key) const -> Value*
	{
		if (!size)
			return nullptr;

		NodeAdapter nodeAdapter(buffer, size);
		const NodeAdapter::NodeRef foundNodeRef = TreeLogic::Find(nodeAdapter, rootNodeIndex, key);
		if (foundNodeRef == NodeAdapter::ZeroNodeRef)
			return nullptr;

		XAssert(foundNodeRef < size);
		return &buffer[foundNodeRef].data.value;
	}

	template <typename Key, typename Value>
	inline auto FlatBinaryTreeMap<Key, Value>::begin() const -> Iterator
	{
		if (!size)
			return Iterator();

		NodeAdapter nodeAdapter(buffer, size);
		const NodeAdapter::NodeRef beginNodeRef = TreeLogic::FindExtreme(nodeAdapter, rootNodeIndex, 0);
		XAssert(beginNodeRef < size);
		return Iterator(this, beginNodeRef);
	}

	template <typename Key, typename Value>
	inline auto FlatBinaryTreeMap<Key, Value>::end() const -> Iterator
	{
		return Iterator();
	}


	// FlatBinaryTreeMap::Iterator /////////////////////////////////////////////////////////////////

	template <typename Key, typename Value>
	inline void FlatBinaryTreeMap<Key, Value>::Iterator::operator ++ ()
	{
		XAssertUnreachableCode();
	}

	template <typename Key, typename Value>
	inline void FlatBinaryTreeMap<Key, Value>::Iterator::operator -- ()
	{
		XAssertUnreachableCode();
	}


	// IntrusiveBinaryTree::NodeAdapter ////////////////////////////////////////////////////////////

	template <typename Node, IntrusiveBinaryTreeNodeHook(Node::* nodeHook), typename NodeComparator>
	class IntrusiveBinaryTree<Node, nodeHook, NodeComparator>::NodeAdapter
	{
	public:
		using NodeRef = Node*;
		static constexpr NodeRef ZeroNodeRef = nullptr;

		inline Node* getNodeParent(Node* node) const;
		inline Node* getNodeChild(Node* node, uint8 childIndex) const;
		inline sint8 getNodeBalanceFactor(Node* node) const;

		inline void setNodeParent(Node* node, Node* parentToSet);
		inline void setNodeChild(Node* node, uint8 childIndex, Node* childToSet);
		inline void setNodeBalanceFactor(Node* node, sint8 balanceFactor);

		inline void setNodeFull(Node* node, Node* parent, Node* leftChild, Node* rightChild, sint8 balanceFactor);

		inline ordering compareNodeTo(Node* left, Node* right) const { return NodeComparator::Compare(*left, *right); }

		template <typename Key>
		inline ordering compareNodeTo(Node* left, const Key& right) const { return NodeComparator::Compare(*left, right); }
	};

	template <typename Node, IntrusiveBinaryTreeNodeHook(Node::* nodeHook), typename NodeComparator>
	inline auto IntrusiveBinaryTree<Node, nodeHook, NodeComparator>::NodeAdapter::
		getNodeParent(Node* node) const -> Node*
	{
		return (Node*)(GetNodeHook(*node).parent_aux & NodePtrMask);
	}

	template <typename Node, IntrusiveBinaryTreeNodeHook(Node::* nodeHook), typename NodeComparator>
	inline auto IntrusiveBinaryTree<Node, nodeHook, NodeComparator>::NodeAdapter::
		getNodeChild(Node* node, uint8 childIndex) const -> Node*
	{
		XAssert(childIndex < 2);
		return (Node*)GetNodeHook(*node).children[childIndex];
	}

	template <typename Node, IntrusiveBinaryTreeNodeHook(Node::* nodeHook), typename NodeComparator>
	inline auto IntrusiveBinaryTree<Node, nodeHook, NodeComparator>::NodeAdapter::
		getNodeBalanceFactor(Node* node) const -> sint8
	{
		const uintptr aux = GetNodeHook(*node).parent_aux & NodeParentPtrAuxMask;
		XAssert(aux >= 1 && aux <= 3);
		return sint8(aux) - 2;
	}

	template <typename Node, IntrusiveBinaryTreeNodeHook(Node::* nodeHook), typename NodeComparator>
	inline void IntrusiveBinaryTree<Node, nodeHook, NodeComparator>::NodeAdapter::
		setNodeParent(Node* node, Node* parentToSet)
	{
		XAssert((uintptr(parentToSet) & ~NodePtrMask) == 0);
		IntrusiveBinaryTreeNodeHook& hook = GetNodeHook(*node);
		hook.parent_aux &= ~NodePtrMask;
		hook.parent_aux |= uintptr(parentToSet);
	}

	template <typename Node, IntrusiveBinaryTreeNodeHook(Node::* nodeHook), typename NodeComparator>
	inline void IntrusiveBinaryTree<Node, nodeHook, NodeComparator>::NodeAdapter::
		setNodeChild(Node* node, uint8 childIndex, Node* childToSet)
	{
		XAssert(childIndex < 2);
		XAssert((uintptr(childToSet) & ~NodePtrMask) == 0);
		GetNodeHook(*node).children[childIndex] = childToSet;
	}

	template <typename Node, IntrusiveBinaryTreeNodeHook(Node::* nodeHook), typename NodeComparator>
	inline void IntrusiveBinaryTree<Node, nodeHook, NodeComparator>::NodeAdapter::
		setNodeBalanceFactor(Node* node, sint8 balanceFactor)
	{
		XAssert(balanceFactor >= -1 && balanceFactor <= +1);
		IntrusiveBinaryTreeNodeHook& hook = GetNodeHook(*node);
		hook.parent_aux &= ~NodeParentPtrAuxMask;
		hook.parent_aux |= uintptr(balanceFactor + 2);
	}

	template <typename Node, IntrusiveBinaryTreeNodeHook(Node::* nodeHook), typename NodeComparator>
	inline void IntrusiveBinaryTree<Node, nodeHook, NodeComparator>::NodeAdapter::
		setNodeFull(Node* node, Node* parent, Node* leftChild, Node* rightChild, sint8 balanceFactor)
	{
		XAssert((uintptr(parent) & ~NodePtrMask) == 0);
		XAssert((uintptr(leftChild) & ~NodePtrMask) == 0);
		XAssert((uintptr(rightChild) & ~NodePtrMask) == 0);
		XAssert(balanceFactor >= -1 && balanceFactor <= +1);

		IntrusiveBinaryTreeNodeHook& hook = GetNodeHook(*node);
		hook.children[0] = leftChild;
		hook.children[1] = rightChild;
		hook.parent_aux = uintptr(parent) | uintptr(balanceFactor + 2);
	}


	// IntrusiveBinaryTree /////////////////////////////////////////////////////////////////////////

	template <typename Node, IntrusiveBinaryTreeNodeHook(Node::* nodeHook), typename NodeComparator>
	template <typename Key>
	inline auto IntrusiveBinaryTree<Node, nodeHook, NodeComparator>::
		find(const Key& key) const -> Iterator
	{
		NodeAdapter nodeAdapter;
		return Iterator(TreeLogic::Find(nodeAdapter, root, key));
	}

	template <typename Node, IntrusiveBinaryTreeNodeHook(Node::* nodeHook), typename NodeComparator>
	inline auto IntrusiveBinaryTree<Node, nodeHook, NodeComparator>::
		insert(Node& node, Iterator* outExistingNode) -> Iterator
	{
		//XAssert(!GetNodeHook(node).isHooked());
		XAssert((uintptr(&node) & ~NodePtrMask) == 0);

		NodeAdapter nodeAdapter;
		typename TreeLogic::InsertLocation insertLocation = {};
		Node* existingNode = TreeLogic::FindInsertLocation(nodeAdapter, root, &node, insertLocation);
		if (existingNode)
		{
			// Node with such key exists. Can't insert.
			if (outExistingNode)
				*outExistingNode = Iterator(existingNode);
			return Iterator();
		}
		root = TreeLogic::InsertAndRebalance(nodeAdapter, root, insertLocation, &node);
		return Iterator(&node);
	}

	template <typename Node, IntrusiveBinaryTreeNodeHook(Node::* nodeHook), typename NodeComparator>
	inline auto IntrusiveBinaryTree<Node, nodeHook, NodeComparator>::begin() const -> Iterator
	{
		NodeAdapter nodeAdapter;
		return Iterator(TreeLogic::FindExtreme(nodeAdapter, root, 0));
	}

	template <typename Node, IntrusiveBinaryTreeNodeHook(Node::* nodeHook), typename NodeComparator>
	inline auto IntrusiveBinaryTree<Node, nodeHook, NodeComparator>::end() const -> Iterator
	{
		return Iterator(nullptr);
	}


	// IntrusiveBinaryTree::Iterator ///////////////////////////////////////////////////////////////

	template <typename Node, IntrusiveBinaryTreeNodeHook(Node::* nodeHook), typename NodeComparator>
	inline void IntrusiveBinaryTree<Node, nodeHook, NodeComparator>::Iterator::operator++()
	{
		NodeAdapter nodeAdapter;
		node = TreeLogic::Iterate(nodeAdapter, node, 1);
	}

	template <typename Node, IntrusiveBinaryTreeNodeHook(Node::* nodeHook), typename NodeComparator>
	inline void IntrusiveBinaryTree<Node, nodeHook, NodeComparator>::Iterator::operator--()
	{
		NodeAdapter nodeAdapter;
		node = TreeLogic::Iterate(nodeAdapter, node, 0);
	}
}
