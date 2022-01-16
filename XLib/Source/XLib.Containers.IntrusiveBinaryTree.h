#pragma once

#include "XLib.h"
#include "XLib.Containers.AVLTreeLogic.h"
#include "XLib.NonCopyable.h"

namespace XLib
{
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
		inline Iterator find(const Key& key);

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

		inline Node& operator * () { return *node; }
		inline Node* operator -> () { return node; }
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
		inline void setNodeChild(Node* parent, uint8 childIndex, Node* childToSet);
		inline void setNodeBalanceFactor(Node* node, sint8 balanceFactor);

		inline void setNodeFull(Node* node, Node* parent, Node* leftChild, Node* rightChild, sint8 balanceFactor);

		inline ordering compareNodeTo(Node* left, Node* right) const { return NodeComparator::Compare(*left, *right); }

		template <typename Key>
		inline ordering compareNodeTo(Node* left, Key right) const { return NodeComparator::Compare(*left, right); }
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
		setNodeChild(Node* parent, uint8 childIndex, Node* childToSet)
	{
		XAssert(childIndex < 2);
		XAssert((uintptr(childToSet) & ~NodePtrMask) == 0);
		GetNodeHook(*parent).children[childIndex] = childToSet;
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
		find(const Key& key) -> Iterator
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
