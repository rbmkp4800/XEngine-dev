#pragma once

#include "XLib.h"

namespace XLib
{
	enum class WeakOrdering : uint8
	{
		Less,
		Equivalent,
		Greater,
	};

	template <typename NodeAdapter>
	class AVLTreeLogic
	{
	public:
		using NodeRef = NodeAdapter::NodeRef;
		static constexpr NodeRef ZeroNodeRef = NodeAdapter::ZeroNodeRef;

		struct InsertLocation
		{
			NodeRef parentNode;
			uint8 childIndex;
		};

		template <typename Key>
		static inline NodeRef Find(NodeAdapter& adapter, NodeRef root, const Key& searchKey);

		template <typename Key>
		static inline bool FindInsertLocation(NodeAdapter& adapter, NodeRef rootNode, const Key& insertionKey, InsertLocation& resultLocation);
		static inline NodeRef InsertAndRebalance(NodeAdapter& adapter, NodeRef rootNode, InsertLocation location, NodeRef nodeToInsert);
		static inline NodeRef RemoveAndRebalance(NodeAdapter& adapter, NodeRef rootNode, NodeRef nodeToRemove);

		static inline NodeRef FindExtreme(NodeAdapter& adapter, NodeRef rootNode, uint8 direction);
		static inline NodeRef Iterate(NodeAdapter& adapter, NodeRef node, uint8 direction);

	private:
		struct BalanceResult
		{
			NodeRef newSubtreeRootNode;
			bool subtreeHeightChanged;
		};

		// Rotation routines return new root of subtree.
		// Parent of returned node is not set. This should be done by caller.
		static inline NodeRef RotateSimple(NodeAdapter& adapter, NodeRef parentNode, NodeRef childNode, uint8 direction); // direction: 0 - left, 1 - right
		static inline NodeRef RotateDouble(NodeAdapter& adapter, NodeRef parentNode, NodeRef childNode, uint8 direction); // direction: 0 - right-left, 1 - left-right

		static constexpr inline uint8 BalanceFactorToAux2bits(sint32 balanceFactor) { return uint8(balanceFactor + 2) & 0x3; }
		static constexpr inline sint32 Aux2bitsToBalanceFactor(uint8 aux2bits) { return sint32(aux2bits) - 2; }
	};
}

// Node adapter template:
// 
//	class MyNodeAdater
//	{
//	private:
//		Foo bar;
// 
//	public:
//		using NodeRef = MyNode*;
//		static constexpr NodeRef ZeroNodeRef = nullptr;
//	
//		NodeRef getNodeParent(NodeRef node) const;
//		NodeRef getNodeChild(NodeRef node, uint8 childIndex) const;
//		uint8 getNodeAux2bits(NodeRef node) const;
//	
//		void setNodeParent(NodeRef node, NodeRef parentToSet);
//		void setNodeChild(NodeRef parent, uint8 childIndex, NodeRef childToSet);
//		void setNodeAux2bits(NodeRef node, uint8 aux2bits);
//	
//		void setNodeFull(NodeRef node, NodeRef parent, NodeRef leftChild, NodeRef rightChild, uint8 aux2bits);
//	
//		WeakOrdering compareNodeTo(NodeRef left, NodeRef right) const;
//		WeakOrdering compareNodeTo(NodeRef left, OtherKeyType right) const;
//	};


////////////////////////////////////////////////////////////////////////////////////////////////////
// Definition //////////////////////////////////////////////////////////////////////////////////////

namespace XLib
{
	template <typename NodeAdapter>
	template <typename Key>
	inline auto AVLTreeLogic<NodeAdapter>::Find(
		NodeAdapter& adapter, NodeRef root, const Key& searchKey) -> NodeRef
	{
		NodeRef currentNode = root;
		while (currentNode != ZeroNodeRef)
		{
			const WeakOrdering order = adapter.compareNodeTo(currentNode, searchKey);

			if (order == WeakOrdering::Equivalent)
				return currentNode;

			// Moving right if current node is less than search key and vice versa.
			const uint8 nextChildDirection = (order == WeakOrdering::Less) ? 1 : 0;
			currentNode = adapter.getNodeChild(currentNode, nextChildDirection);
		}
		return ZeroNodeRef;
	}

	template <typename NodeAdapter>
	template <typename Key>
	inline auto AVLTreeLogic<NodeAdapter>::FindInsertLocation(
		NodeAdapter& adapter, NodeRef rootNode, const Key& insertionKey, InsertLocation& resultLocation) -> bool
	{
		if (rootNode == ZeroNodeRef)
		{
			resultLocation.parentNode = ZeroNodeRef;
			resultLocation.childIndex = 0;
			return true;
		}

		NodeRef currentNode = rootNode;
		for (;;)
		{
			const WeakOrdering order = adapter.compareNodeTo(currentNode, insertionKey);

			if (order == WeakOrdering::Equivalent)
				return false;

			// Moving right if current node is less than search key and vice versa.
			const uint8 nextChildDirection = (order == WeakOrdering::Less) ? 1 : 0;
			const NodeRef nextNode = adapter.getNodeChild(currentNode, nextChildDirection);

			if (nextNode == ZeroNodeRef)
			{
				resultLocation.parentNode = currentNode;
				resultLocation.childIndex = nextChildDirection;
				return true;
			}

			currentNode = nextNode;
		}
	}

	template <typename NodeAdapter>
	inline auto AVLTreeLogic<NodeAdapter>::InsertAndRebalance(
		NodeAdapter& adapter, NodeRef rootNode, InsertLocation location, NodeRef nodeToInsert) -> NodeRef
	{
		XAssert(location.childIndex < 2);
		XAssert((location.parentNode == ZeroNodeRef) == (rootNode == ZeroNodeRef));

		adapter.setNodeFull(nodeToInsert, location.parentNode, ZeroNodeRef, ZeroNodeRef, BalanceFactorToAux2bits(0));

		if (location.parentNode == ZeroNodeRef)
			return nodeToInsert;

		XAssert(adapter.getNodeChild(location.parentNode, location.childIndex) == ZeroNodeRef);
		adapter.setNodeChild(location.parentNode, location.childIndex, nodeToInsert);

		// Retrace
		NodeRef currentNode = nodeToInsert;
		sint32 currentNodeBalanceFactor = 0;
		for (;;)
		{
			const NodeRef parentNode = adapter.getNodeParent(currentNode);
			if (parentNode == ZeroNodeRef)
			{
				XAssert(currentNode == rootNode);
				break;
			}

			XAssert(
				adapter.getNodeChild(parentNode, 0) == currentNode ||
				adapter.getNodeChild(parentNode, 1) == currentNode);
			const bool currentNodeIsLeft = (adapter.getNodeChild(parentNode, 0) == currentNode);

			const sint32 balanceFactorToThisSide = currentNodeIsLeft ? -1 : +1;
			const sint32 balanceFactorToOtherSide = currentNodeIsLeft ? +1 : -1;
			const uint8 directionToOtherSide = currentNodeIsLeft ? 1 : 0;

			const sint32 parentNodeBalanceFactor = Aux2bitsToBalanceFactor(adapter.getNodeAux2bits(parentNode));
			XAssert(parentNodeBalanceFactor >= -1 && parentNodeBalanceFactor <= +1);

			if (parentNodeBalanceFactor == balanceFactorToThisSide)
			{
				// This subtree was already higher and now it also gets +1 due to insertion.
				// So total height diff to this side becomes 2. Rebalancing.

				const bool needDoubleRotation = (currentNodeBalanceFactor == balanceFactorToOtherSide);

				const NodeRef parentsParentNode = adapter.getNodeParent(parentNode);
				const NodeRef newSubtreeRootNode = needDoubleRotation ?
					RotateDouble(adapter, parentNode, currentNode, directionToOtherSide) :
					RotateSimple(adapter, parentNode, currentNode, directionToOtherSide);

				adapter.setNodeParent(newSubtreeRootNode, parentsParentNode);
				if (parentsParentNode == ZeroNodeRef)
				{
					// Reached root.
					XAssert(parentNode == rootNode);
					rootNode = newSubtreeRootNode;
				}
				else
				{
					XAssert(
						adapter.getNodeChild(parentsParentNode, 0) == parentNode ||
						adapter.getNodeChild(parentsParentNode, 1) == parentNode);
					const bool parentNodeIsLeft = (adapter.getNodeChild(parentsParentNode, 0) == parentNode);
					adapter.setNodeChild(parentsParentNode, parentNodeIsLeft ? 0 : 1, newSubtreeRootNode);
				}

				break;
			}

			if (parentNodeBalanceFactor == balanceFactorToOtherSide)
			{
				// Subtree becomes balanced after insertion.
				adapter.setNodeAux2bits(parentNode, BalanceFactorToAux2bits(0));

				break;
			}
			
			XAssert(parentNodeBalanceFactor == 0);

			// Subtree was balanced before insertion so height diff becomes 1 after insertion.
			adapter.setNodeAux2bits(parentNode, BalanceFactorToAux2bits(balanceFactorToThisSide));

			currentNode = parentNode;
			currentNodeBalanceFactor = balanceFactorToThisSide;
		}

		return rootNode;
	}

	template <typename NodeAdapter>
	inline auto AVLTreeLogic<NodeAdapter>::RemoveAndRebalance(
		NodeAdapter& adapter, NodeRef rootNode, NodeRef nodeToRemove) -> NodeRef
	{

	}

	template <typename NodeAdapter>
	inline auto AVLTreeLogic<NodeAdapter>::FindExtreme(NodeAdapter& adapter, NodeRef rootNode, uint8 direction) -> NodeRef
	{
		XAssert(direction == 0 || direction == 1);

		NodeRef currentNode = rootNode;
		for (;;)
		{
			const NodeRef childNode = adapter.getNodeChild(currentNode, direction);
			if (childNode == ZeroNodeRef)
				return currentNode;
			currentNode = childNode;
		}
	}

	template <typename NodeAdapter>
	inline auto AVLTreeLogic<NodeAdapter>::Iterate(NodeAdapter& adapter, NodeRef node, uint8 direction) -> NodeRef
	{
		XAssert(direction == 0 || direction == 1);
		const uint8 directionToOtherSide = direction ^ 1;

		const NodeRef iterationDirectionChildNode = adapter.getNodeChild(node, direction);
		if (iterationDirectionChildNode != ZeroNodeRef)
			return FindExtreme(adapter, iterationDirectionChildNode, directionToOtherSide);

		NodeRef currentNode = node;
		for (;;)
		{
			const NodeRef parentNode = adapter.getNodeParent(currentNode);
			if (parentNode == ZeroNodeRef)
				return ZeroNodeRef;

			XAssert(
				adapter.getNodeChild(parentNode, 0) == currentNode ||
				adapter.getNodeChild(parentNode, 1) == currentNode);
			const bool isTurnToOtherSide = (adapter.getNodeChild(parentNode, directionToOtherSide) == currentNode);
			if (isTurnToOtherSide)
				return parentNode;

			currentNode = parentNode;
		}
	}

	template <typename NodeAdapter>
	inline auto AVLTreeLogic<NodeAdapter>::RotateSimple(
		NodeAdapter& adapter, NodeRef parentNode, NodeRef childNode, uint8 direction) -> NodeRef
	{
		// Left rotation (direction = 0):
		//    X              Z'
		//   / \            / \
		//  a   Z    =>    X'  c
		//     / \        / \
		//    b   c      a   b
		//
		// Preconditions:
		//   Z is right child of X
		//   height(Z) = height(a) + 2   =>   balanceFactor(X) = +2
		//   height(c) >= height(b)   =>   balanceFactor(Z) in [0,+1]
		//
		// balanceFactor(Z) =  0   =>   height(a) + 1 = height(b) = height(c)   =>   balanceFactor(Z') = -1 and balanceFactor(X') = +1
		// balanceFactor(Z) = +1   =>   height(a) = height(b) = height(c) - 1   =>   balanceFactor(Z') =  0 and balanceFactor(X') =  0

		// Right rotation (direction = 1):
		//      X          Z'
		//     / \        / \
		//    Z   c  =>  a   X'
		//   / \            / \
		//  a   b          b   c
		//
		// Preconditions:
		//   Z is left child of X
		//   height(Z) = height(c) + 2   =>   balanceFactor(X) = -2
		//   height(a) <= height(b)   =>   balanceFactor(Z) in [-1,0]
		//
		// balanceFactor(Z) = -1   =>   height(a) - 1 = height(b) = height(c)   =>   balanceFactor(Z') =  0 and balanceFactor(X') =  0
		// balanceFactor(Z) =  0   =>   height(a) = height(b) = height(c) + 1   =>   balanceFactor(Z') = +1 and balanceFactor(X') = -1

		XAssert(direction == 0 || direction == 1);
		XAssert(adapter.getNodeChild(parentNode, direction ^ 1) == childNode);

		const NodeRef X = parentNode;
		const NodeRef Z = childNode;

		const NodeRef b = adapter.getNodeChild(Z, direction);

		adapter.setNodeChild(X, direction ^ 1, b);
		if (b != ZeroNodeRef)
			adapter.setNodeParent(b, X);

		adapter.setNodeChild(Z, direction, X);
		adapter.setNodeParent(X, Z);

		const sint32 zBalanceFactor = Aux2bitsToBalanceFactor(adapter.getNodeAux2bits(Z));
		XAssert(zBalanceFactor == 0 || zBalanceFactor == (direction ? -1 : +1));
		const sint32 Q = (zBalanceFactor == 0) ? (direction ? +1 : -1) : 0;
		adapter.setNodeAux2bits(X, BalanceFactorToAux2bits(-Q));
		adapter.setNodeAux2bits(Z, BalanceFactorToAux2bits(+Q));

		return Z;
	}

	template <typename NodeAdapter>
	inline auto AVLTreeLogic<NodeAdapter>::RotateDouble(
		NodeAdapter& adapter, NodeRef parentNode, NodeRef childNode, uint8 direction) -> NodeRef
	{
		// Right-left rotation (direction = 0):
		//    X
		//   / \               Y
		//  a   Z            /   \
		//     / \   =>   X|P     Z|Q
		//    Y   d       / \     / \
		//   / \         a   b   c   d
		//  b   c
		//
		// Preconditions:
		//   Z is right child of X
		//   height(Z) = height(a) + 2   =>   balanceFactor(X) = +2
		//   height(Y) > height(d)   =>   balanceFactor(Z) = -1
		//   balanceFactor(Y) in [-1..+1]

		// Left-right rotation (direction = 1):
		//      X
		//     / \             Y
		//    Z   d          /   \
		//   / \     =>   Z|P     X|Q
		//  a   Y         / \     / \
		//     / \       a   b   c   d
		//    b   c

		XAssert(direction == 0 || direction == 1);
		XAssert(adapter.getNodeChild(parentNode, direction ^ 1) == childNode);

		const NodeRef X = parentNode;
		const NodeRef Z = childNode;
		const NodeRef Y = adapter.getNodeChild(Z, direction);

		const NodeRef b = adapter.getNodeChild(Y, 0);
		const NodeRef c = adapter.getNodeChild(Y, 1);

		// P and Q are left and right Y children after rotation:
		//    Y
		//   / \
		//  P   Q
		const NodeRef P = direction ? Z : X;
		const NodeRef Q = direction ? X : Z;

		adapter.setNodeChild(P, 1, b);
		if (b != ZeroNodeRef)
			adapter.setNodeParent(b, P);

		adapter.setNodeChild(Q, 0, c);
		if (c != ZeroNodeRef)
			adapter.setNodeParent(c, Q);

		// balanceFactor(Y) = -1   =>   height(a) = height(b) = height(c) + 1 = height(d)   =>   balanceFactor(P) =  0 and balanceFactor(Q) = +1
		// balanceFactor(Y) =  0   =>   height(a) = height(b) = height(c)     = height(d)   =>   balanceFactor(P) =  0 and balanceFactor(Q) =  0
		// balanceFactor(Y) = +1   =>   height(a) = height(b) + 1 = height(c) = height(d)   =>   balanceFactor(P) = -1 and balanceFactor(Q) =  0

		const sint32 yBalanceFactor = Aux2bitsToBalanceFactor(adapter.getNodeAux2bits(Y));
		XAssert(yBalanceFactor >= -1 && yBalanceFactor <= +1);
		adapter.setNodeAux2bits(P, BalanceFactorToAux2bits(-max<sint32>(yBalanceFactor, 0)));
		adapter.setNodeAux2bits(Q, BalanceFactorToAux2bits(-min<sint32>(yBalanceFactor, 0)));

		adapter.setNodeChild(Y, 0, P);
		adapter.setNodeChild(Y, 1, Q);
		adapter.setNodeAux2bits(Y, BalanceFactorToAux2bits(0));

		adapter.setNodeParent(P, Y);
		adapter.setNodeParent(Q, Y);

		return Y;
	}
}
