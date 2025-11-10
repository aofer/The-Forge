#include "GbufferPass.h"

#include "../../../Visibility_Buffer/src/SanMiguel.h"

GbufferPass::GbufferPass(Renderer* renderer, TiledShadingSample* app)
{
	m_app = app;
	m_renderer = renderer;
}

bool GbufferPass::Init()
{
	return true;
}

bool GbufferPass::Load(ReloadDesc* pReloadDesc)
{
	if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
	{
		// GBuffer
		const char* pStaticSamplerNames[] = { "defaultSampler" };
		Sampler* pStaticSamplers[] = { m_app->GetBilinearSampler() };

		RootSignatureDesc gBuffersRootDesc = { &m_gbuffersShader, 1 };
		gBuffersRootDesc.mStaticSamplerCount = 1;
		gBuffersRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
		gBuffersRootDesc.ppStaticSamplers = pStaticSamplers;

		addRootSignature(m_renderer, &gBuffersRootDesc, &m_gbufferRootSignature);
		m_mapIDRootConstantIndex = getDescriptorIndexFromName(m_gbufferRootSignature, "cbTextureRootConstants"); 


		// GBuffer
		DescriptorSetDesc setDesc;
		setDesc = { m_gbufferRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };

		addDescriptorSet(m_renderer, &setDesc, &m_gbufferDescriptorSet[0]);
		setDesc = { m_gbufferRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(m_renderer, &setDesc, &m_gbufferDescriptorSet[1]);
		setDesc = { m_gbufferRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_DRAW, 2 };
		addDescriptorSet(m_renderer, &setDesc, &m_gbufferDescriptorSet[2]);
	}
	if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
	{
		TinyImageFormat deferredFormats[DEFERRED_RT_COUNT] = {};
		for (uint32_t i = 0; i < DEFERRED_RT_COUNT; ++i)
		{
			deferredFormats[i] = m_app->GetGbuffers()[i]->mFormat;
		}

		// Create depth state and rasterizer state
		DepthStateDesc depthStateDesc = {};
		depthStateDesc.mDepthTest = true;
		depthStateDesc.mDepthWrite = true;
		depthStateDesc.mDepthFunc = CMP_LEQUAL;

		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_NONE;

		PipelineDesc desc = {};
		desc.mType = PIPELINE_TYPE_GRAPHICS;
		GraphicsPipelineDesc& deferredPassPipelineSettings = desc.mGraphicsDesc;
		deferredPassPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		deferredPassPipelineSettings.mRenderTargetCount = DEFERRED_RT_COUNT;
		deferredPassPipelineSettings.pDepthState = &depthStateDesc;

		deferredPassPipelineSettings.pColorFormats = deferredFormats;

		deferredPassPipelineSettings.mSampleCount = m_app->GetGbuffers()[0]->mSampleCount;
		deferredPassPipelineSettings.mSampleQuality = m_app->GetGbuffers()[0]->mSampleQuality;

		deferredPassPipelineSettings.mDepthStencilFormat = m_app->GetDepthBuffer()->mFormat;
		deferredPassPipelineSettings.pRootSignature = m_gbufferRootSignature;
		deferredPassPipelineSettings.pShaderProgram = m_gbuffersShader;
		deferredPassPipelineSettings.pVertexLayout = m_app->GetVertexLayout();
		deferredPassPipelineSettings.pRasterizerState = &rasterizerStateDesc;
		addPipeline(m_renderer, &desc, &m_gbufferPipeline);
	}
	return true;
}

void GbufferPass::Unload(ReloadDesc* pReloadDesc)
{
	if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
	{
		removePipeline(m_renderer, m_gbufferPipeline);
	}
	if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
	{
		removeDescriptorSet(m_renderer, m_gbufferDescriptorSet[0]);
		removeDescriptorSet(m_renderer, m_gbufferDescriptorSet[1]);
		removeDescriptorSet(m_renderer, m_gbufferDescriptorSet[2]);
		removeRootSignature(m_renderer, m_gbufferRootSignature);
		removeShader(m_renderer, m_gbuffersShader);
	}
}

void GbufferPass::Exit()
{

}

void GbufferPass::Draw(Cmd* cmd)
{
	//temp get scene
    Scene* scene = m_app->GetScene();

	cmdBindIndexBuffer(cmd, scene->geom->pIndexBuffer, scene->geom->mIndexType, 0);
    Buffer* PVertexBuffer[] = { scene->geom->pVertexBuffers[0] };
    cmdBindVertexBuffer(cmd, 1, PVertexBuffer, scene->geom->mVertexStrides, NULL);

	//struct MaterialMaps
	//{
	//	uint textureMaps;
	//} data;

	const uint32_t drawCount = (uint32_t)scene->geom->mDrawArgCount;

	cmdBindPipeline(cmd, m_gbufferPipeline);
    cmdBindDescriptorSet(cmd, 0, m_gbufferDescriptorSet[0]);
    cmdBindDescriptorSet(cmd, m_app->GetFrameIndex(), m_gbufferDescriptorSet[1]);
    cmdBindDescriptorSet(cmd, 0, m_gbufferDescriptorSet[2]);

	for (uint32_t i = 0; i < drawCount; ++i)
	{
		//int materialID = gMaterialIds[i];
		//materialID *= 5;    //because it uses 5 basic textures for redering BRDF

		//data.textureMaps = ((gSponzaTextureIndexforMaterial[materialID + 0] & 0xFF) << 0) |
		//	((gSponzaTextureIndexforMaterial[materialID + 1] & 0xFF) << 8) |
		//	((gSponzaTextureIndexforMaterial[materialID + 2] & 0xFF) << 16) |
		//	((gSponzaTextureIndexforMaterial[materialID + 3] & 0xFF) << 24);

		//cmdBindPushConstants(cmd, m_gbufferRootSignature, m_mapIDRootConstantIndex, &data);
        IndirectDrawIndexArguments& cmdData = scene->geom->pDrawArgs[i];
		cmdDrawIndexed(cmd, cmdData.mIndexCount, cmdData.mStartIndex, cmdData.mVertexOffset);
	}
}

void GbufferPass::Update(float deltaTime)
{
	if (deltaTime > 0.1) //TODO remove - this is only to get rid of a warning
	{

	}
}
