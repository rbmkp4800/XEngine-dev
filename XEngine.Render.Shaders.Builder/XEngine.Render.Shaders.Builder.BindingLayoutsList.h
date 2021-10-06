#pragma once

#include <XLib.h>
#include <XLib.Containers.ArrayList.h>
#include <XLib.Containers.IntrusiveBinaryTree.h>
#include <XLib.NonCopyable.h>
#include <XLib.String.h>

#include <XEngine.Render.HAL.ShaderCompiler.h>

namespace XEngine::Render::Shaders::Builder
{
	class BindingLayoutsList;

	using BindingLayoutRef = uint16;

	class BindingLayoutsList : public XLib::NonCopyable
	{
	private:
		struct BindPoint
		{
			XLib::IntrusiveBinaryTreeNodeHook searchTreeHook;
			XLib::InplaceString<28, uint8> name;
			HAL::ShaderCompiler::BindPointType type;
			uint8 constantCount;
			uint8 shaderVisibility;
		};

		using BindPointsSearchTree = XLib::IntrusiveBinaryTree<BindPoint, &BindPoint::searchTreeHook>;
		using BindPointsStorageList = XLib::StaticSegmentedArrayList<BindPoint, 5, 12>;

		struct BindingLayout
		{
			XLib::IntrusiveBinaryTreeNodeHook searchTreeHook;
			XLib::InplaceString<63, uint8> name;
			BindPointsSearchTree bindPointsSearchTree;
			bool isFinalized;
		};

		using BindingLayoutsSearchTree = XLib::IntrusiveBinaryTree<BindingLayout, &BindingLayout::searchTreeHook>;
		using BindingLayoutsStorageList = XLib::StaticSegmentedArrayList<BindingLayout, 4, 10>;

	private:
		BindingLayoutsSearchTree bindingLayoutSearchTree;
		BindingLayoutsStorageList bindingLayouts;
		BindPointsStorageList bindPoints;

	public:
		BindingLayoutsList() = default;
		~BindingLayoutsList() = default;

		BindingLayoutRef createBindingLayout(const char* name);
		void pushBindPoint(BindingLayoutRef layout, const HAL::ShaderCompiler::BindPointDesc& bindPointDesc);
		void finalizeBindingLayout(BindingLayoutRef layout);

		BindingLayoutRef findBindingLayout(const char* name) const;
	};
}
