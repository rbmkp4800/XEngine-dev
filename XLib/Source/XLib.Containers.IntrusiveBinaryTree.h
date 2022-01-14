#pragma once

#include "XLib.h"
#include "XLib.Containers.AVLTreeLogic.h"
#include "XLib.NonCopyable.h"

namespace XLib
{
	class IntrusiveBinaryTreeNodeHook : public XLib::NonCopyable
	{
		template <typename Node, IntrusiveBinaryTreeNodeHook(Node::*)>
		friend class IntrusiveBinaryTree;

	private:
		void* children[2] = {};
		uintptr parent_aux = 0;

	public:
		IntrusiveBinaryTreeNodeHook() = default;
		~IntrusiveBinaryTreeNodeHook() = default;
	};

	template <typename Node, IntrusiveBinaryTreeNodeHook(Node::*nodeHook)>
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
		static inline IntrusiveBinaryTreeNodeHook& GetNodeHook(Node& node) { return (((node).*nodeHook).parent); }

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

	template <typename Node, IntrusiveBinaryTreeNodeHook(Node::*nodeHook)>
	class IntrusiveBinaryTree<Node, nodeHook>::Iterator
	{
	private:
		Node* node = nullptr;

	public:
		Iterator() = default;
		~Iterator() = default;

		inline void operator ++ ();
		inline void operator -- ();

		inline Node& operator * () { return *node; }
		inline bool operator == (const Iterator& that) const { return node == that.node; }
		inline bool operator != (const Iterator& that) const { return node != that.node; }
	};
}

#if 0

// TODO: implement const iterator
// TODO: specify that hook should be 8 bytes aligned and store height diff in lower pointer bits

#define hookLeft(node)		(((node).*(Hook (Node::*))nodeHook).left)
#define hookRight(node)		(((node).*(Hook (Node::*))nodeHook).right)
#define hookParent(node)	(((node).*(Hook (Node::*))nodeHook).parent)
#define hookHeight(node)	(((node).*(Hook (Node::*))nodeHook).height)

namespace XLib
{
	class IntrusiveBinaryTreeNodeHook
	{
		template <typename Node, IntrusiveBinaryTreeNodeHook(Node::*)>
		friend class IntrusiveBinaryTree;

	private:
		void* left = nullptr;
		void* right = nullptr;
		void* parent = nullptr;
		uint8 height = 0;

	public:
		IntrusiveBinaryTreeNodeHook() = default;
		~IntrusiveBinaryTreeNodeHook() = default;

		inline bool isHooked() const { return left != nullptr || right != nullptr || parent != nullptr; }
	};

	template <typename Node, IntrusiveBinaryTreeNodeHook(Node::*nodeHook)>
	class IntrusiveBinaryTree : public NonCopyable
	{
	private:
		struct Hook
		{
			Node *left, *right, *parent;
			uint8 height;
		};

		static_assert(sizeof(Hook) == sizeof(IntrusiveBinaryTreeNodeHook), "invalid private hook size");

	public:
		class Iterator
		{
			friend class IntrusiveBinaryTree;

		private:
			Node *node = nullptr;

			inline Iterator(Node *node) : node(node) {}

		public:
			Iterator() = default;
			~Iterator() = default;

			inline Node& get() { return *node; }
			inline void next() { node = hookRight(*node); }
			inline void prev() { node = hookLeft(*node); }
			inline bool isValid() { return node != nullptr; }

			inline Node& operator * () { return *node; }
			inline bool operator == (const Iterator& that) const { return node == that.node; }
			inline bool operator != (const Iterator& that) const { return node != that.node; }
			inline void operator ++ () { next(); }
			inline void operator -- () { prev(); }
		};

	private:
		Node *root = nullptr;

		static inline void resetHeight(Node* self)
		{
			hookHeight(*self) = max(
				hookRight(*self) ? hookHeight(*hookRight(*self)) : 0,
				hookLeft(*self) ? hookHeight(*hookLeft(*self)) : 0) + 1;
		}
		static inline sint8 subtreesHeightDelta(const Node* self)
		{
			return (hookRight(*self) ? hookHeight(*hookRight(*self)) : 0) -
				(hookLeft(*self) ? hookHeight(*hookLeft(*self)) : 0);
		}

		static inline Node* rotateRight(Node* self)
		{
			Node* newParent = hookLeft(*self);
			hookLeft(*self) = hookRight(*newParent);
			if (hookLeft(*self))
				hookParent(*hookLeft(*self)) = self;
			hookRight(*newParent) = self;
			hookParent(*newParent) = hookParent(*self);
			hookParent(*self) = newParent;
			resetHeight(self);
			resetHeight(newParent);
			return newParent;
		}
		static inline Node* rotateLeft(Node* self)
		{
			Node* newParent = hookRight(*self);
			hookRight(*self) = hookLeft(*newParent);
			if (hookRight(*self))
				hookParent(*hookRight(*self)) = self;
			hookLeft(*newParent) = self;
			hookParent(*newParent) = hookParent(*self);
			hookParent(*self) = newParent;
			resetHeight(self);
			resetHeight(newParent);
			return newParent;
		}
		static inline Node* balance(Node* self)
		{
			resetHeight(self);
			if (subtreesHeightDelta(self) == 2)
			{
				if (subtreesHeightDelta(hookRight(*self)) < 0)
					hookRight(*self) = rotateRight(hookRight(*self));
				return rotateLeft(self);
			}
			if (subtreesHeightDelta(self) == -2)
			{
				if (subtreesHeightDelta(hookLeft(*self)) > 0)
					hookLeft(*self) = rotateLeft(hookLeft(*self));
				return rotateRight(self);
			}
			return self;
		}

	public:
		IntrusiveBinaryTree() = default;
		~IntrusiveBinaryTree() = default;

		template <typename KeyType>
		inline Iterator find(const KeyType& key)
		{

		}

		inline Iterator insert(Node& node)
		{
			if (!root)
			{
				root = &node;
				hookLeft(node) = nullptr;
				hookRight(node) = nullptr;
				hookParent(node) = nullptr;
				hookHeight(node) = 0;
				return Iterator(&node);
			}

			// insert new node
			Node *current = root;
			for (;;)
			{
				if (*current < node)
				{
					if (hookRight(*current))
						current = hookRight(*current);
					else
					{
						hookRight(*current) = &node;
						break;
					}
				}
				else if (node < *current)
				{
					if (hookLeft(*current))
						current = hookLeft(*current);
					else
					{
						hookLeft(*current) = &node;
						break;
					}
				}
				else // element with the same key exists
					return Iterator();
			}

			hookLeft(node) = nullptr;
			hookRight(node) = nullptr;
			hookParent(node) = current;
			hookHeight(node) = 0;	// TODO: ??

			// restore balance
			while (hookParent(*current))
			{
				Node* newCurrent = balance(current);
				Node *parent = hookParent(*newCurrent);
				if (hookLeft(*parent) == current)
					hookLeft(*parent) = newCurrent;
				else if (hookRight(*parent) == current)
					hookRight(*parent) = newCurrent;
				current = parent;
			}

			return Iterator(&node);
		}

		inline bool remove(Node& node)
		{

		}

		template <typename KeyType>
		inline bool remove(const KeyType& key)
		{

		}

		inline Iterator begin()
		{
			if (!root)
				return Iterator();

			Node *current = root;
			for (;;)
			{
				Node *next = hookLeft(*current);
				if (!next)
					return Iterator(current);
				current = next;
			}
		}

		inline Iterator end() { return Iterator(); }
	};
}

#undef hookLeft
#undef hookRight
#undef hookParent
#undef hookHeight

#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
// Definition //////////////////////////////////////////////////////////////////////////////////////

namespace XLib
{
	template <typename Node, IntrusiveBinaryTreeNodeHook(Node::*nodeHook)>
	class IntrusiveBinaryTree<Node, nodeHook>::NodeAdapter
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

		inline WeakOrdering compareNodeTo(Node* left, Node* right) const;
	};

	template <typename Node, IntrusiveBinaryTreeNodeHook(Node::*nodeHook)>
	inline auto IntrusiveBinaryTree<Node, nodeHook>::NodeAdapter::getNodeParent(Node* node) const -> Node*
	{
		return (Node*)(GetNodeHook(*node).parent_aux & NodePtrMask)
	}

	template <typename Node, IntrusiveBinaryTreeNodeHook(Node::*nodeHook)>
	inline auto IntrusiveBinaryTree<Node, nodeHook>::NodeAdapter::getNodeChild(Node* node, uint8 childIndex) const -> Node*
	{
		XAssert(childIndex < 2);
		return GetNodeHook(*node).children[childIndex];
	}

	template <typename Node, IntrusiveBinaryTreeNodeHook(Node::*nodeHook)>
	inline auto IntrusiveBinaryTree<Node, nodeHook>::NodeAdapter::getNodeBalanceFactor(Node* node) const -> sint8
	{
		const uintptr aux = GetNodeHook(*node).parent_aux & NodeParentPtrAuxMask;
		XAssert(aux >= 1 && aux <= 3);
		return sint8(aux) - 2;
	}

	template <typename Node, IntrusiveBinaryTreeNodeHook(Node::*nodeHook)>
	inline void IntrusiveBinaryTree<Node, nodeHook>::NodeAdapter::setNodeParent(Node* node, Node* parentToSet)
	{
		XAssert((uintptr(parentToSet) & ~NodePtrMask) == 0);
		IntrusiveBinaryTreeNodeHook& hook = GetNodeHook(*node);
		hook.parent_aux &= NodePtrMask;
		hook.parent_aux |= uintptr(parentToSet);
	}

	template <typename Node, IntrusiveBinaryTreeNodeHook(Node::*nodeHook)>
	inline void IntrusiveBinaryTree<Node, nodeHook>::NodeAdapter::setNodeChild(Node* parent, uint8 childIndex, Node* childToSet)
	{
		XAssert(childIndex < 2);
		XAssert((uintptr(childToSet) & ~NodePtrMask) == 0);
		GetNodeHook(*parent).children[childIndex] = childToSet;
	}

	template <typename Node, IntrusiveBinaryTreeNodeHook(Node::*nodeHook)>
	inline void IntrusiveBinaryTree<Node, nodeHook>::NodeAdapter::setNodeBalanceFactor(Node* node, sint8 balanceFactor)
	{
		XAssert(balanceFactor >= -1 && balanceFactor <= +1);
		IntrusiveBinaryTreeNodeHook& hook = GetNodeHook(*node);
		hook.parent_aux &= NodeParentPtrAuxMask;
		hook.parent_aux |= uintptr(balanceFactor + 2);
	}

	template <typename Node, IntrusiveBinaryTreeNodeHook(Node::*nodeHook)>
	inline void IntrusiveBinaryTree<Node, nodeHook>::NodeAdapter::setNodeFull(
		Node* node, Node* parent, Node* leftChild, Node* rightChild, sint8 balanceFactor)
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


	template <typename Node, IntrusiveBinaryTreeNodeHook(Node::*nodeHook)>
	template <typename Key>
	inline auto IntrusiveBinaryTree<Node, nodeHook>::find(const Key& key) -> Iterator
	{
		return Iterator(TreeLogic::Find<Key>(NodeAdapter(), root, key));
	}

	template <typename Node, IntrusiveBinaryTreeNodeHook(Node::*nodeHook)>
	inline auto IntrusiveBinaryTree<Node, nodeHook>::insert(Node& node, Iterator* outExistingNode) -> Iterator
	{
		XAssert(!GetNodeHook(node).isHooked());

		TreeLogic::InsertLocation insertLocation = {};
		Node* existingNode = TreeLogic::FindInsertLocation<Node*>(NodeAdapter(), root, &node, insertLocation);
		if (existingNode)
		{
			// Node with such key exists. Can't insert.
			if (outExistingNode)
				*outExistingNode = existingNode;
			return Iterator();
		}
		root = TreeLogic::InsertAndRebalance(NodeAdapter(), root, insertLocation, &node);
		return Iterator(&node);
	}

	template <typename Node, IntrusiveBinaryTreeNodeHook(Node::*nodeHook)>
	inline auto IntrusiveBinaryTree<Node, nodeHook>::begin() const -> Iterator
	{
		return Iterator(TreeLogic::FindExtreme(NodeAdapter(), root, 0));
	}

	template <typename Node, IntrusiveBinaryTreeNodeHook(Node::*nodeHook)>
	inline auto IntrusiveBinaryTree<Node, nodeHook>::end() const -> Iterator
	{
		return Iterator(nullptr);
	}
}
