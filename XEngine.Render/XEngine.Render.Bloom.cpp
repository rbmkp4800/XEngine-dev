#include "XEngine.XStringHash.h"
#include "XEngine.Render.h"

#include "XEngine.Render.Bloom.h"

using namespace XEngine::Render;
using namespace XEngine::Render::HAL;

struct Bloom::PassUserData
{
	Scheduler::TextureHandle inputLuminanceTexture;
	Scheduler::TextureHandle outputBloomMapTexture;
};

void Bloom::PassExecutor(Scheduler::PassExecutionBroker& broker,
	Device& device, CommandList& commandList, const void* userDataPtr)
{
	const PassUserData& userData = *(PassUserData*)userDataPtr;

	const ShaderLibrary& shaderLibrary = Shaders::GetLibrary();
	const PipelineHandle bloomPipeline = shaderLibrary.getPipeline("Bloom"_xsh);
	const DescriptorSetLayoutHandle inputDSL = shaderLibrary.getDescriptorSetLayout("BloomInput"_xsh);

	const ResourceViewHandle inputDescriptor = broker.getTransientTextureView(userData.inputLuminanceTexture);
	const RenderTargetViewHandle outputRT = broker.getTransientRenderTargetView(userData.outputBloomMapTexture);

	const DescriptorSetReference inputDescriptorSet = broker.allocateTransientDescriptorSet(inputDSL);
	device.writeDescriptor(inputDescriptorSet, "Luminance"_xsh, inputDescriptor);

	commandList.setPipelineType(PipelineType::Graphics);
	commandList.setPipeline(bloomPipeline);
	commandList.setRenderTarget(outputRT);
	commandList.bindDescriptorSet("input"_xsh, inputDescriptorSet);
	commandList.draw(3);
}

Bloom::Output Bloom::Schedule(Scheduler::Pipeline& pipeline, Scheduler::TextureHandle inputTexture)
{
	const Scheduler::TextureHandle bloomTexture = pipeline.createTransientTexture();

	Scheduler::PassTextureDependencyDesc textureDependencies[] =
	{
		{ inputTexture, Scheduler::TextureAccess::ShaderRead },
		{ bloomTexture, Scheduler::TextureAccess::RenderTarget },
	};

	Scheduler::PassDependenciesDesc dependencies = {};
	dependencies.textureDependencies = textureDependencies;
	dependencies.textureDependencyCount = countof(textureDependencies);

	PassUserData* userData = (PassUserData*)pipeline.allocateUserData(sizeof(PassUserData));
	*userData = PassUserData {};
	userData->inputLuminanceTexture = inputTexture;
	userData->outputBloomMapTexture = bloomTexture;

	pipeline.schedulePass(Scheduler::PassType::Graphics, dependencies, &PassExecutor, userData);
}
