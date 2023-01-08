#include "XEngine.XStringHash.h"
#include "XEngine.Render.h"

#include "XEngine.Render.PostComposition.h"

using namespace XEngine::Render;
using namespace XEngine::Render::HAL;

struct PostComposition::PassUserData
{
	Scheduler::TextureHandle inputLuminanceTexture;
	Scheduler::TextureHandle outputColorTexture;
};

void PostComposition::PassExecutor(Scheduler::PassExecutionBroker& broker,
	Device& device, CommandList& commandList, const void* userDataPtr)
{
	const PassUserData& userData = *(PassUserData*)userDataPtr;

	const ShaderLibrary& shaderLibrary = Shaders::GetLibrary();
	const PipelineHandle postCompositionPipeline = shaderLibrary.getPipeline("PostComposition"_xsh);
	const DescriptorSetLayoutHandle inputDSL = shaderLibrary.getDescriptorSetLayout("PostCompositionInput"_xsh);

	const ResourceViewHandle inputLuminanceDescriptor = broker.getTransientTextureView(userData.inputLuminanceTexture);
	const RenderTargetViewHandle outputColorRT = broker.getTransientRenderTargetView(userData.outputColorTexture);

	const DescriptorSetReference inputDescriptorSet = broker.allocateTransientDescriptorSet(inputDSL);
	device.writeDescriptor(inputDescriptorSet, "Luminance"_xsh, inputLuminanceDescriptor);

	commandList.setPipelineType(PipelineType::Graphics);
	commandList.setPipeline(postCompositionPipeline);
	commandList.setRenderTarget(outputColorRT);
	commandList.bindDescriptorSet("input"_xsh, inputDescriptorSet);
	commandList.draw(3);
}

void PostComposition::Schedule(Scheduler::Pipeline& pipeline,
	Scheduler::TextureHandle inputLuminanceTexture,
	Scheduler::TextureHandle outputColorTexture)
{
	Scheduler::PassTextureDependencyDesc textureDependencies[] =
	{
		{ inputLuminanceTexture, Scheduler::TextureAccess::ShaderRead },
		{ outputColorTexture, Scheduler::TextureAccess::RenderTarget },
	};

	Scheduler::PassDependenciesDesc dependencies = {};
	dependencies.textureDependencies = textureDependencies;
	dependencies.textureDependencyCount = countof(textureDependencies);

	PassUserData* userData = (PassUserData*)pipeline.allocateUserData(sizeof(PassUserData));
	*userData = PassUserData {};
	userData->inputLuminanceTexture = inputLuminanceTexture;
	userData->outputColorTexture = outputColorTexture;

	pipeline.schedulePass(Scheduler::PassType::Graphics, dependencies, &PassExecutor, userData);
}
