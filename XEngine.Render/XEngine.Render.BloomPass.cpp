#include "XEngine.XStringHash.h"
#include "XEngine.Render.ShaderLibrary.h"

#include "XEngine.Render.BloomPass.h"

using namespace XEngine::Render;
using namespace XEngine::Render::HAL;
using namespace XEngine::Render::Scheduler;

void BloomPass::Executor(PipelineSchedulerBroker& schedulerBroker,
	Device& device, CommandList& commandList, const void* userDataPtr)
{
	const UserData& userData = *(UserData*)userDataPtr;

	const ShaderLibrary& shaderLibrary = ...;

	const PipelineHandle bloomPassPipeline = shaderLibrary.getPipeline("BloomPass"_xsh);
	const DescriptorSetLayoutHandle bloomInputDescriptorSetLayout = shaderLibrary.getDescriptorSetLayout("BloomInput"_xsh);

	const ResourceViewHandle inputTextureView = schedulerBroker.getTransientTextureView(userData.inputTexture);
	const RenderTargetViewHandle outputRenderTargetView = schedulerBroker.getTransientRenderTargetView(userData.outputTexture);

	const DescriptorSetReference bloomInputDescriptorSet = schedulerBroker.allocateTransientDescriptorSet(bloomInputDescriptorSetLayout);
	device.writeDescriptor(bloomInputDescriptorSet, "input"_xsh, inputTextureView);

	commandList.setPipelineType(PipelineType::Graphics);
	commandList.setPipeline(bloomPassPipeline);
	commandList.setRenderTarget(outputRenderTargetView);
	commandList.bindDescriptorSet("input"_xsh, bloomInputDescriptorSet);
	commandList.draw(3);
}

BloomPass::Output BloomPass::AddToPipeline(Pipeline& pipeline, PipelineTextureHandle inputTexture)
{
	const PipelineTextureHandle bloomTexture = pipeline.createTransientTexture();

	PassTextureDependencyDesc textureDependencies[] =
	{
		{ inputTexture, TextureAccess::ShaderRead },
		{ bloomTexture, TextureAccess::RenderTarget },
	};

	PassDependenciesDesc dependencies = {};
	dependencies.textureDependencies = textureDependencies;
	dependencies.textureDependencyCount = countof(textureDependencies);

	UserData* userData = (UserData*)pipeline.allocateUserData(sizeof(UserData));
	*userData = UserData {};
	userData->inputTexture = inputTexture;
	userData->outputTexture = bloomTexture;

	pipeline.addPass(PassDeviceWorkloadType::Graphics, dependencies, &BloomPass::Executor, userData);
}
