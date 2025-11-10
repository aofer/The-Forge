#include "SkyBoxPass.h"

SkyboxPass::SkyboxPass(const char* skyboxPath, Renderer* renderer, TiledShadingSample* app)
{
	m_skyboxPath = skyboxPath;
	m_app = app;
	m_renderer = renderer;
}

bool SkyboxPass::Init()
{
	//Generate sky box vertex buffer
	float skyBoxPoints[] = {
		0.5f,  -0.5f, -0.5f, 1.0f,    // -z
		-0.5f, -0.5f, -0.5f, 1.0f,  -0.5f, 0.5f,  -0.5f, 1.0f,  -0.5f, 0.5f,
		-0.5f, 1.0f,  0.5f,  0.5f,  -0.5f, 1.0f,  0.5f,  -0.5f, -0.5f, 1.0f,

		-0.5f, -0.5f, 0.5f,  1.0f,    //-x
		-0.5f, -0.5f, -0.5f, 1.0f,  -0.5f, 0.5f,  -0.5f, 1.0f,  -0.5f, 0.5f,
		-0.5f, 1.0f,  -0.5f, 0.5f,  0.5f,  1.0f,  -0.5f, -0.5f, 0.5f,  1.0f,

		0.5f,  -0.5f, -0.5f, 1.0f,    //+x
		0.5f,  -0.5f, 0.5f,  1.0f,  0.5f,  0.5f,  0.5f,  1.0f,  0.5f,  0.5f,
		0.5f,  1.0f,  0.5f,  0.5f,  -0.5f, 1.0f,  0.5f,  -0.5f, -0.5f, 1.0f,

		-0.5f, -0.5f, 0.5f,  1.0f,    // +z
		-0.5f, 0.5f,  0.5f,  1.0f,  0.5f,  0.5f,  0.5f,  1.0f,  0.5f,  0.5f,
		0.5f,  1.0f,  0.5f,  -0.5f, 0.5f,  1.0f,  -0.5f, -0.5f, 0.5f,  1.0f,

		-0.5f, 0.5f,  -0.5f, 1.0f,    //+y
		0.5f,  0.5f,  -0.5f, 1.0f,  0.5f,  0.5f,  0.5f,  1.0f,  0.5f,  0.5f,
		0.5f,  1.0f,  -0.5f, 0.5f,  0.5f,  1.0f,  -0.5f, 0.5f,  -0.5f, 1.0f,

		0.5f,  -0.5f, 0.5f,  1.0f,    //-y
		0.5f,  -0.5f, -0.5f, 1.0f,  -0.5f, -0.5f, -0.5f, 1.0f,  -0.5f, -0.5f,
		-0.5f, 1.0f,  -0.5f, -0.5f, 0.5f,  1.0f,  0.5f,  -0.5f, 0.5f,  1.0f,
	};

	uint64_t       skyBoxDataSize = 4 * 6 * 6 * sizeof(float);
	BufferLoadDesc skyboxVbDesc = {};

	skyboxVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
	skyboxVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	skyboxVbDesc.mDesc.mStartState = RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
	skyboxVbDesc.mDesc.mSize = skyBoxDataSize;
	skyboxVbDesc.pData = skyBoxPoints;
    skyboxVbDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	skyboxVbDesc.ppBuffer = &pSkyboxVertexBuffer;
	addResource(&skyboxVbDesc, NULL);

	// Uniform buffer for camera data
	BufferLoadDesc ubCamDesc = {};
	ubCamDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	ubCamDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	ubCamDesc.mDesc.mSize = sizeof(UniformCamData);
	ubCamDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	ubCamDesc.pData = NULL;

	for (uint32_t i = 0; i < gImageCount; ++i)
	{
		ubCamDesc.ppBuffer = &pBufferUniformCameraSky[i];
		addResource(&ubCamDesc, NULL);
	}

	ShaderLoadDesc skyboxShaderDesc = {};
    skyboxShaderDesc.mStages[0].pFileName = "skybox.vert";
    skyboxShaderDesc.mStages[1].pFileName = "skybox.frag";
	addShader(m_renderer, &skyboxShaderDesc, &pSkyboxShader);

	const char* pSkyboxamplerName = "skyboxSampler";
	RootSignatureDesc skyboxRootDesc = { &pSkyboxShader, 1 };
	skyboxRootDesc.mStaticSamplerCount = 1;
	skyboxRootDesc.ppStaticSamplerNames = &pSkyboxamplerName;
	Sampler* sampler = m_app->GetBilinearSampler();
	skyboxRootDesc.ppStaticSamplers = &sampler;
	addRootSignature(m_renderer, &skyboxRootDesc, &pSkyboxRootSignature);

	// Load the skybox panorama texture.
	SyncToken       token = {};
	TextureLoadDesc panoDesc = {};
	panoDesc.pFileName = m_skyboxPath;
	panoDesc.ppTexture = &pPanoSkybox;
	addResource(&panoDesc, &token);

	/*
            // Load the skybox panorama texture.
        SyncToken       token = {};
        TextureLoadDesc skyboxDesc = {};
        skyboxDesc.pFileName = skyboxNames[skyboxIndex];
        skyboxDesc.ppTexture = &pTextureSkybox;
        addResource(&skyboxDesc, &token);

	*/

	//TODO should be configurable
	static const uint32_t gSkyboxSize = 1024;
	static const uint32_t gSkyboxMips = 11;

	TextureDesc skyboxImgDesc = {};
	skyboxImgDesc.mArraySize = 6;
	skyboxImgDesc.mDepth = 1;
	skyboxImgDesc.mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
	skyboxImgDesc.mHeight = gSkyboxSize;
	skyboxImgDesc.mWidth = gSkyboxSize;
	skyboxImgDesc.mMipLevels = gSkyboxMips;
	skyboxImgDesc.mSampleCount = SAMPLE_COUNT_1;
	skyboxImgDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
	skyboxImgDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE_CUBE | DESCRIPTOR_TYPE_RW_TEXTURE;
	skyboxImgDesc.pName = "skyboxImgBuff";

	TextureLoadDesc skyboxLoadDesc = {};
	skyboxLoadDesc.pDesc = &skyboxImgDesc;
	skyboxLoadDesc.ppTexture = &pSkyboxIrradianceMap;
	addResource(&skyboxLoadDesc, NULL);

	return true;
}

bool SkyboxPass::Load(ReloadDesc* pReloadDesc)
{
    if (pReloadDesc) // this is just to fix the warning need to fix better later
	{

	}
	//layout and pipeline for skybox draw
	VertexLayout vertexLayoutSkybox = {};
	vertexLayoutSkybox.mAttribCount = 1;
    vertexLayoutSkybox.mBindingCount = 1;
	vertexLayoutSkybox.mAttribs[0].mSemantic = SEMANTIC_POSITION;
	vertexLayoutSkybox.mAttribs[0].mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
	vertexLayoutSkybox.mAttribs[0].mBinding = 0;
	vertexLayoutSkybox.mAttribs[0].mLocation = 0;
	vertexLayoutSkybox.mAttribs[0].mOffset = 0;

	RasterizerStateDesc rasterizerStateCullNoneDesc = {};
    rasterizerStateCullNoneDesc.mCullMode = CULL_MODE_NONE;

	PipelineDesc desc = {};
	desc.mType = PIPELINE_TYPE_GRAPHICS;
	GraphicsPipelineDesc& graphicsPipelineDesc = desc.mGraphicsDesc;

	graphicsPipelineDesc = {};
	graphicsPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
	graphicsPipelineDesc.pDepthState = NULL;

	graphicsPipelineDesc.mRenderTargetCount = 1;
    graphicsPipelineDesc.pColorFormats = &m_app->GetMainRenderTarget()->mFormat;
    graphicsPipelineDesc.mSampleCount = m_app->GetMainRenderTarget()->mSampleCount;  //  pRenderTargetDeferredPass[0][0]->mSampleCount;
    graphicsPipelineDesc.mSampleQuality = m_app->GetMainRenderTarget()->mSampleQuality; // pRenderTargetDeferredPass[0][0]->mSampleQuality;

	graphicsPipelineDesc.mDepthStencilFormat = m_app->GetDepthBuffer()->mFormat;
	graphicsPipelineDesc.pRootSignature = pSkyboxRootSignature;
	graphicsPipelineDesc.pShaderProgram = pSkyboxShader;
	graphicsPipelineDesc.pVertexLayout = &vertexLayoutSkybox;
    graphicsPipelineDesc.pRasterizerState = &rasterizerStateCullNoneDesc;
	addPipeline(m_renderer, &desc, &pSkyboxPipeline);

		// Skybox
    DescriptorSetDesc setDesc = { pSkyboxRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
    addDescriptorSet(m_renderer, &setDesc, &pDescriptorSetSkybox[0]);
    setDesc = { pSkyboxRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
    addDescriptorSet(m_renderer, &setDesc, &pDescriptorSetSkybox[1]);

    DescriptorData skyParams[1] = {};
    skyParams[0].pName = "skyboxTex";
    // TODO complete this
    skyParams[0].ppTextures = &pPanoSkybox;
    updateDescriptorSet(m_renderer, 0, pDescriptorSetSkybox[0], 1, skyParams);
    for (uint32_t i = 0; i < gImageCount; ++i)
    {
        skyParams[0].pName = "uniformBlock";
        skyParams[0].ppBuffers = &pBufferUniformCameraSky[i];
        updateDescriptorSet(m_renderer, i, pDescriptorSetSkybox[1], 1, skyParams);
    }

	return true;
}

void SkyboxPass::Unload(ReloadDesc* pReloadDesc)
{
	removeShader(m_renderer, pSkyboxShader);

	 if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
    {
         removePipeline(m_renderer, pSkyboxPipeline);
    }

    if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
    {
        removeRootSignature(m_renderer, pSkyboxRootSignature);
        removeDescriptorSet(m_renderer, pDescriptorSetSkybox[0]);
        removeDescriptorSet(m_renderer, pDescriptorSetSkybox[1]);
    }
}

void SkyboxPass::Exit()
{
	removeResource(pSkyboxVertexBuffer);

	for (uint32_t i = 0; i < gImageCount; ++i)
	{
		removeResource(pBufferUniformCameraSky[i]);
	}

	removeResource(pSkyboxIrradianceMap);
    removeResource(pPanoSkybox);
}

void SkyboxPass::Draw(Cmd* cmd)
{
	BufferUpdateDesc skyboxViewProjCbv = { pBufferUniformCameraSky[m_app->GetFrameIndex()] };
	beginUpdateResource(&skyboxViewProjCbv);
	*(UniformCamData*)skyboxViewProjCbv.pMappedData = gUniformDataSky;
	endUpdateResource(&skyboxViewProjCbv);

	// Draw the skybox.
	cmdBeginGpuTimestampQuery(cmd, m_app->GetGpuProfileToken(), "Render SkyBox");

	const uint32_t skyboxStride = sizeof(float) * 4;
	cmdBindPipeline(cmd, pSkyboxPipeline);
	cmdBindDescriptorSet(cmd, 0, pDescriptorSetSkybox[0]);
	cmdBindDescriptorSet(cmd, m_app->GetFrameIndex(), pDescriptorSetSkybox[1]);
	cmdBindVertexBuffer(cmd, 1, &pSkyboxVertexBuffer, &skyboxStride, NULL);
	cmdDraw(cmd, 36, 0);
    cmdEndGpuTimestampQuery(cmd, m_app->GetGpuProfileToken());
}

void SkyboxPass::Update(float deltaTime)
{
    if (deltaTime > 0.1) // TODO remove this, this is just to get rid of the warning
	{

	}

	// Update camera
	mat4        viewMat = m_app->GetCameraController()->getViewMatrix();
	const float aspectInverse = (float)m_app->GetWindowSize().getY() / (float)m_app->GetWindowSize().getX();
	const float horizontal_fov = PI / 3.0f; //why 3 and not 2?
    CameraMatrix projMat = CameraMatrix::perspectiveReverseZ(horizontal_fov, aspectInverse, 0.1f, 1000.0f);

	CameraMatrix ViewProjMat = projMat * viewMat;
    gUniformDataSky.mProjectView = ViewProjMat;
    gUniformDataSky.mInvProjectView = CameraMatrix::inverse(ViewProjMat);
    gUniformDataSky.mCamPos = m_app->GetCameraController()->getViewPosition();

	viewMat.setTranslation(vec3(0));
    gUniformDataSky.mProjectView = projMat * viewMat;
}
