#include "TiledShadingSample.h"

#include "../../../../Common_3/Application/Interfaces/IFont.h"
#include "../../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../../Common_3/Application/Interfaces/IUI.h"
#include "../../../../Common_3/Application/Interfaces/IInput.h"

#include "../../../Visibility_Buffer/src/SanMiguel.h"

#include "SkyBoxPass.h"
#include "GbufferPass.h"

TiledShadingSample::TiledShadingSample()
{

}

ICameraController* TiledShadingSample::GetCameraController()
{
	//return m_camController;
	return pCameraController;
}

uvec2 TiledShadingSample::GetWindowSize()
{
	return uvec2(mSettings.mWidth, mSettings.mHeight);
}

Scene* TiledShadingSample::GetScene()
{ 
	return SanMiguelScene;
}

const char* TiledShadingSample::GetName() { return "Tiled Shading Sample"; }

bool TiledShadingSample::Init()
{
	// FILE PATHS
    fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_ANIMATIONS, "Animations");
    fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_BINARIES, "CompiledShaders");
    fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_PIPELINE_CACHE, "PipelineCaches");
    fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_TEXTURES, "Textures");
    fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_FONTS, "Fonts");
    fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_MESHES, "Meshes");
    fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SCRIPTS, "Scripts");
    fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_OTHER_FILES, "");
    fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_SCREENSHOTS, "Screenshots");
    fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_DEBUG, "Debug");

	//initThreadSystem(&pThreadSystem); //TODO do I need that?

    CameraMotionParameters camParameters{ 100.0f, 150.0f, 300.0f };
    vec3                   camPos{ -0.21f, 12.2564745f, 59.3652649f };
    vec3                   lookAt{ 0, 0, 0 };

	m_camController = initFpsCameraController(camPos, lookAt);
	m_camController->setMotionParameters(camParameters);
    pCameraController = m_camController; //TODO remove this when we figure out how to do input handling without this being static

	RendererDesc settings;
	memset(&settings, 0, sizeof(settings));
	initRenderer(GetName(), &settings, &pRenderer);
	//check for init success
	if (!pRenderer)
		return false;

	QueueDesc queueDesc = {};
	queueDesc.mType = QUEUE_TYPE_GRAPHICS;
	queueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;
	addQueue(pRenderer, &queueDesc, &pGraphicsQueue);

	// Create command pool and create a cmd buffer for each swapchain image
	for (uint32_t i = 0; i < gImageCount; ++i)
	{
		CmdPoolDesc cmdPoolDesc = {};
		cmdPoolDesc.pQueue = pGraphicsQueue;
		addCmdPool(pRenderer, &cmdPoolDesc, &pCmdPools[i]);
		CmdDesc cmdDesc = {};
		cmdDesc.pPool = pCmdPools[i];
		addCmd(pRenderer, &cmdDesc, &pCmds[i]);
	}

	GpuCmdRingDesc cmdRingDesc = {};
    cmdRingDesc.pQueue = pGraphicsQueue;
    cmdRingDesc.mPoolCount = gDataBufferCount;
    cmdRingDesc.mCmdPerPoolCount = 1;
    cmdRingDesc.mAddSyncPrimitives = true;
    addGpuCmdRing(pRenderer, &cmdRingDesc, &gGraphicsCmdRing);

	//for (uint32_t i = 0; i < gImageCount; ++i)
	//{
	//	addFence(pRenderer, &pRenderCompleteFences[i]);
	//	addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
	//}

	addSemaphore(pRenderer, &pImageAcquiredSemaphore);

	initResourceLoaderInterface(pRenderer);

	// Load fonts
	FontDesc font = {};
	font.pFontPath = "TitilliumText/TitilliumText-Bold.otf";
	fntDefineFonts(&font, 1, &gFontID);

	FontSystemDesc fontRenderDesc = {};
	fontRenderDesc.pRenderer = pRenderer;
	if (!initFontSystem(&fontRenderDesc))
		return false; // report?

	// Initialize Forge User Interface Rendering
	UserInterfaceDesc uiRenderDesc = {};
	uiRenderDesc.pRenderer = pRenderer;
	initUserInterface(&uiRenderDesc);

	// Initialize micro profiler and its UI.
	ProfilerDesc profiler = {};
	profiler.pRenderer = pRenderer;
	profiler.mWidthUI = mSettings.mWidth;
	profiler.mHeightUI = mSettings.mHeight;
	initProfiler(&profiler);

	m_graphicsProfileToken = addGpuProfiler(pRenderer, pGraphicsQueue, "Graphics");

	InputSystemDesc inputDesc = {};
	inputDesc.pRenderer = pRenderer;
	inputDesc.pWindow = pWindow;
	if (!initInputSystem(&inputDesc))
		return false;

	/************************************************************************/
	// Setup sampler states
	/************************************************************************/
	// Create sampler for VB render target
	SamplerDesc trilinearDesc = {
		FILTER_LINEAR, FILTER_LINEAR, MIPMAP_MODE_LINEAR, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, 0.0f, false, 0.0f, 0.0f, 8.0f
	};
	SamplerDesc bilinearDesc = { FILTER_LINEAR,       FILTER_LINEAR,       MIPMAP_MODE_LINEAR,
		ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT };

	SamplerDesc pointDesc = { FILTER_NEAREST,
		FILTER_NEAREST,
		MIPMAP_MODE_NEAREST,
		ADDRESS_MODE_CLAMP_TO_EDGE,
		ADDRESS_MODE_CLAMP_TO_EDGE,
		ADDRESS_MODE_CLAMP_TO_EDGE };

	SamplerDesc bilinearClampDesc = { FILTER_LINEAR,
		FILTER_LINEAR,
		MIPMAP_MODE_LINEAR,
		ADDRESS_MODE_CLAMP_TO_EDGE,
		ADDRESS_MODE_CLAMP_TO_EDGE,
		ADDRESS_MODE_CLAMP_TO_EDGE };

	addSampler(pRenderer, &trilinearDesc, &pSamplerTrilinearAniso);
	addSampler(pRenderer, &bilinearDesc, &pSamplerBilinear);
	addSampler(pRenderer, &pointDesc, &pSamplerPointClamp);
	addSampler(pRenderer, &bilinearClampDesc, &pSamplerBilinearClamp);


	//geo loading
    SyncToken        sceneToken = {};
    GeometryLoadDesc sceneLoadDesc = {};
    sceneLoadDesc.mFlags = GEOMETRY_LOAD_FLAG_SHADOWED; // To compute CPU clusters
    SanMiguelScene = nullptr;    //loadSanMiguel(&sceneLoadDesc, sceneToken, false);
    waitForToken(&sceneToken);
	//TODO remove when creating proper scene structure
	//for (size_t i = 0; i < TOTAL_IMGS; i += 1)
	//{
	//	loadTexture(i);
	//}
	//InitVertexLayout();
	//for (size_t i = 0; i < 2; i += 1)
	//{
	//	loadMesh(i);
	//}

	//Init passes
	m_skybox = tf_placement_new<SkyboxPass>(tf_calloc(1, sizeof(SkyboxPass)),"LA_Helipad3D.tex", pRenderer, this);
	m_skybox->Init();

	//add camera controls
    AddCameraControls();

	return true;
}

bool TiledShadingSample::addSwapChain()
{
	SwapChainDesc swapChainDesc = {};
	swapChainDesc.mWindowHandle = pWindow->handle;
	swapChainDesc.mPresentQueueCount = 1;
	swapChainDesc.ppPresentQueues = &pGraphicsQueue;
	swapChainDesc.mWidth = mSettings.mWidth;
	swapChainDesc.mHeight = mSettings.mHeight;
	swapChainDesc.mImageCount = gImageCount;
    swapChainDesc.mColorFormat = getSupportedSwapchainFormat(pRenderer, &swapChainDesc, COLOR_SPACE_SDR_SRGB);
	swapChainDesc.mEnableVsync = mSettings.mVSyncEnabled;
	::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

	return pSwapChain != NULL;
}

bool TiledShadingSample::addSceneBuffer()
{
	RenderTargetDesc sceneRT = {};
	sceneRT.mArraySize = 1;
	sceneRT.mClearValue = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	sceneRT.mDepth = 1;
	sceneRT.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
	sceneRT.mFormat = TinyImageFormat_R16G16B16A16_SFLOAT; 
	sceneRT.mStartState = RESOURCE_STATE_SHADER_RESOURCE;

	sceneRT.mHeight = mSettings.mHeight;
	sceneRT.mWidth = mSettings.mWidth;

	sceneRT.mSampleCount = SAMPLE_COUNT_1;
	sceneRT.mSampleQuality = 0;
	sceneRT.mFlags = TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
	sceneRT.pName = "Scene Buffer";

	addRenderTarget(pRenderer, &sceneRT, &pSceneBuffer);

	return pSceneBuffer != NULL;
}

bool TiledShadingSample::addGBuffers()
{
	ClearValue optimizedColorClearBlack = { { 0.0f, 0.0f, 0.0f, 0.0f } };

	/************************************************************************/
	// Deferred pass render targets
	/************************************************************************/
	RenderTargetDesc deferredRTDesc = {};
	deferredRTDesc.mArraySize = 1;
	deferredRTDesc.mClearValue = optimizedColorClearBlack;
	deferredRTDesc.mDepth = 1;
	deferredRTDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
	deferredRTDesc.mWidth = mSettings.mWidth;
	deferredRTDesc.mHeight = mSettings.mHeight;
	deferredRTDesc.mSampleCount = SAMPLE_COUNT_1;
	deferredRTDesc.mSampleQuality = 0;
	deferredRTDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
	deferredRTDesc.mFlags = TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
	deferredRTDesc.pName = "G-Buffer RTs";

	for (uint32_t i = 0; i < DEFERRED_RT_COUNT; ++i)
	{
		if (i == 1 || i == 2)
			deferredRTDesc.mFormat = TinyImageFormat_R16G16B16A16_SFLOAT;
		else if (i == 3)
			deferredRTDesc.mFormat = TinyImageFormat_R16G16_SFLOAT;
		else
			deferredRTDesc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;

		if (i == 2)
			deferredRTDesc.mClearValue = { { 1.0f, 0.0f, 0.0f, 0.0f } };
		else if (i == 3)
			deferredRTDesc.mClearValue = optimizedColorClearBlack;

		addRenderTarget(pRenderer, &deferredRTDesc, &pRenderTargetDeferredPass[i]);
	}

	//deferredRTDesc.mFormat = TinyImageFormat_R16G16B16A16_SFLOAT;
	//addRenderTarget(pRenderer, &deferredRTDesc, &pRenderTargetDeferredPass[1][1]);

	//deferredRTDesc.mClearValue = { { 1.0f, 0.0f, 0.0f, 0.0f } };
	//addRenderTarget(pRenderer, &deferredRTDesc, &pRenderTargetDeferredPass[1][2]);

	for (int i = 0; i < DEFERRED_RT_COUNT; i++)
	{
		if (pRenderTargetDeferredPass[i] == nullptr)
			return false;
	}

	return true;
}

void TiledShadingSample::addRenderTargets()
{
	addSceneBuffer();

	//addReflectionBuffer();

	addGBuffers();

	addDepthBuffer();

}


bool TiledShadingSample::addDepthBuffer()
{
	// Add depth buffer
	RenderTargetDesc depthRT = {};
	depthRT.mArraySize = 1;
	depthRT.mClearValue = { { 1.0f, 0 } };
	depthRT.mDepth = 1;
	depthRT.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
	depthRT.mFormat = TinyImageFormat_D32_SFLOAT;
	depthRT.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
	depthRT.mHeight = mSettings.mHeight;
	depthRT.mSampleCount = SAMPLE_COUNT_1;
	depthRT.mSampleQuality = 0;
	depthRT.mWidth = mSettings.mWidth;
	depthRT.mFlags = TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
	depthRT.pName = "Depth Buffer";
	addRenderTarget(pRenderer, &depthRT, &pDepthBuffer);

	return pDepthBuffer != NULL;
}

void TiledShadingSample::AddCameraControls() 
{
    typedef bool              (*CameraInputHandler)(InputActionContext* ctx, DefaultInputActions::DefaultInputAction action);
    static CameraInputHandler onCameraInput = [](InputActionContext* ctx, DefaultInputActions::DefaultInputAction action)
    {
        if (*(ctx->pCaptured))
        {
            float2 delta = uiIsFocused() ? float2(0.f, 0.f) : ctx->mFloat2;
            switch (action)
            {
            case DefaultInputActions::ROTATE_CAMERA:
                pCameraController->onRotate(delta);
                break;
            case DefaultInputActions::TRANSLATE_CAMERA:
                pCameraController->onMove(delta);
                break;
            case DefaultInputActions::TRANSLATE_CAMERA_VERTICAL:
                pCameraController->onMoveY(delta[0]);
                break;
            default:
                break;
            }
		}
		return true;
    };
    InputActionDesc actionDesc = { DefaultInputActions::CAPTURE_INPUT,
                                              [](InputActionContext* ctx)
                                              {
                                                  setEnableCaptureInput(!uiIsFocused() && INPUT_ACTION_PHASE_CANCELED != ctx->mPhase);
                                                  return true;
                                              },
                                              NULL };
    addInputAction(&actionDesc);
    actionDesc = { DefaultInputActions::ROTATE_CAMERA,
                   [](InputActionContext* ctx) { return onCameraInput(ctx, DefaultInputActions::ROTATE_CAMERA); },
                   NULL };
    addInputAction(&actionDesc);
    actionDesc = { DefaultInputActions::TRANSLATE_CAMERA,
                   [](InputActionContext* ctx)
                   { return onCameraInput(ctx, DefaultInputActions::TRANSLATE_CAMERA); },
                   NULL };
    addInputAction(&actionDesc);
    actionDesc = { DefaultInputActions::TRANSLATE_CAMERA_VERTICAL,
                   [](InputActionContext* ctx)
                   { return onCameraInput(ctx, DefaultInputActions::TRANSLATE_CAMERA_VERTICAL); },
                   NULL };
    addInputAction(&actionDesc);
    actionDesc = { DefaultInputActions::RESET_CAMERA,
					[](InputActionContext* ctx)
                   {
					   if (ctx) //todo remove - this is just for fixing a warning
                       {

					   }
                       if (!uiWantTextInput())
                           pCameraController->resetView();
                       return true;
                   } };
    addInputAction(&actionDesc);

}

void TiledShadingSample::Draw()
{
    if ((bool)pSwapChain->mEnableVsync != mSettings.mVSyncEnabled)
	{
		waitQueueIdle(pGraphicsQueue);
		::toggleVSync(pRenderer, &pSwapChain);
	}

	uint32_t swapchainImageIndex;
	acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &swapchainImageIndex);

	RenderTarget* pRenderTarget = pSwapChain->ppRenderTargets[swapchainImageIndex];
    GpuCmdRingElement elem = getNextGpuCmdRingElement(&gGraphicsCmdRing, true, 1);

    FenceStatus fenceStatus;
    getFenceStatus(pRenderer, elem.pFence, &fenceStatus);
    if (fenceStatus == FENCE_STATUS_INCOMPLETE)
        waitForFences(pRenderer, 1, &elem.pFence);

	//TODO update descriptor sets here

    resetCmdPool(pRenderer, elem.pCmdPool);

	Cmd* cmd = pCmds[m_frameIndex];
	beginCmd(cmd);

	if (pRenderer->pGpu->mSettings.mGpuMarkers)
    {
        // Check breadcrumb markers
        // checkMarkers();
    }

	cmdBeginGpuFrameProfile(cmd, m_graphicsProfileToken);

	RenderTargetBarrier barrier = { pRenderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET };
    cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, &barrier);


	// Draw Skybox
    cmdBeginGpuTimestampQuery(cmd, m_graphicsProfileToken, "Skybox Pass");

	barrier = { pSceneBuffer, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET };
    cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, &barrier);


	BindRenderTargetsDesc bindTargetsDesc = {};
    bindTargetsDesc.mRenderTargetCount = 1;
    bindTargetsDesc.mRenderTargets[0] = { pSceneBuffer, LOAD_ACTION_CLEAR };
    cmdBindRenderTargets(cmd, &bindTargetsDesc);
	cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 1.0f, 1.0f);
    cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

	m_skybox->Draw(cmd);

	cmdEndGpuTimestampQuery(cmd, m_graphicsProfileToken); // Skybox Pass

	//{
	//	//Prepare and draw Gbuffer
	//	BindRenderTargetsDesc bindRenderTargets = {};
 //       bindRenderTargets.mRenderTargetCount = 1;
 //       bindRenderTargets.mRenderTargets[0] = { pRenderTargetDeferredPass[0], LOAD_ACTION_CLEAR };
 //       bindRenderTargets.mDepthStencil = { pDepthBuffer, LOAD_ACTION_CLEAR };

	//	// Transfer G-buffers to render target state for each buffer
	//	RenderTargetBarrier rtBarriers[DEFERRED_RT_COUNT + 1] = {};
	//	uint32_t rtBarriersCount = DEFERRED_RT_COUNT + 1;
	//	rtBarriers[0] = { pDepthBuffer, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_DEPTH_WRITE };
	//	for (uint32_t i = 0; i < DEFERRED_RT_COUNT; ++i)
	//	{
	//		rtBarriers[1 + i] = { pRenderTargetDeferredPass[i], RESOURCE_STATE_SHADER_RESOURCE,
	//							  RESOURCE_STATE_RENDER_TARGET };
	//	}
	//	// Transfer DepthBuffer to a DephtWrite State
	//	cmdResourceBarrier(cmd, 0, NULL, 0, NULL, rtBarriersCount, rtBarriers);
 //       cmdBindRenderTargets(cmd, &bindRenderTargets);

	//	cmdSetViewport(
	//		cmd, 0.0f, 0.0f, (float)pRenderTargetDeferredPass[0]->mWidth, (float)pRenderTargetDeferredPass[0]->mHeight, 1.0f, 1.0f);
	//	cmdSetScissor(cmd, 0, 0, pRenderTargetDeferredPass[0]->mWidth, pRenderTargetDeferredPass[0]->mHeight);
	//	cmdSetViewport(
	//		cmd, 0.0f, 0.0f, (float)pRenderTargetDeferredPass[0]->mWidth, (float)pRenderTargetDeferredPass[0]->mHeight, 0.0f, 1.0f);


 //       BindRenderTargetsDesc bindRenderTargetsDesc = {};
 //       bindRenderTargetsDesc.mRenderTargetCount = DEFERRED_RT_COUNT;
 //       for (int i = 0; i < DEFERRED_RT_COUNT; ++i)
 //       {
 //           bindRenderTargetsDesc.mRenderTargets[i] = { pRenderTargetDeferredPass[i], LOAD_ACTION_CLEAR };
 //       }

 //       cmdBindRenderTargets(cmd, &bindRenderTargetsDesc);
	//	cmdSetViewport(
	//		cmd, 0.0f, 0.0f, (float)pRenderTargetDeferredPass[0]->mWidth, (float)pRenderTargetDeferredPass[0]->mHeight, 0.0f, 1.0f);
	//	cmdSetScissor(cmd, 0, 0, pRenderTargetDeferredPass[0]->mWidth, pRenderTargetDeferredPass[0]->mHeight);


	//	// Draw Sponza
	//	cmdBeginGpuTimestampQuery(cmd, m_graphicsProfileToken, "Fill GBuffers");
	//	// run gbuffer pass

	//	cmdEndGpuTimestampQuery(cmd, m_graphicsProfileToken); //end fill gbuffers
	//	cmdBindRenderTargets(cmd, NULL);
	//}

	//{
	//	// Transfer DepthBuffer to a Shader resource state
	//	RenderTargetBarrier rtBarriers2[DEFERRED_RT_COUNT + 2];
	//	rtBarriers2[0] = { pDepthBuffer, RESOURCE_STATE_DEPTH_WRITE, RESOURCE_STATE_SHADER_RESOURCE };
	//	// Transfer current render target to a render target state
	//	// Transfer G-buffers to a Shader resource state
	//	for (uint32_t i = 0; i < DEFERRED_RT_COUNT; ++i)
	//	{
	//		rtBarriers2[1 + i] = { pRenderTargetDeferredPass/*[m_frameFlipFlop]*/[i], RESOURCE_STATE_RENDER_TARGET,
	//							  RESOURCE_STATE_SHADER_RESOURCE };
	//	}

	//	cmdResourceBarrier(cmd, 0, NULL, 0, NULL, DEFERRED_RT_COUNT + 1, rtBarriers2);

	//	cmdBindRenderTargets(cmd, NULL);
	//}

	//TODO group this with other barriers
	RenderTargetBarrier Scenebarrier = { pSceneBuffer, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE };
	cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, &Scenebarrier);


    bindTargetsDesc = {};
    bindTargetsDesc.mRenderTargetCount = 1;
    BindRenderTargetDesc bindDesc = BindRenderTargetDesc();
    bindDesc.pRenderTarget = pRenderTarget;
    bindDesc.mClearValue = { 1.0f, 1.0f, 0.0f, 0.0f };
    bindDesc.mLoadAction = LOAD_ACTION_CLEAR;
    bindTargetsDesc.mRenderTargets[0] = bindDesc;
    cmdBindRenderTargets(cmd, &bindTargetsDesc);
	cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 1.0f, 1.0f);
	cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);


	/************************************************************************/
    // Blit Texture
    /************************************************************************/
    cmdBeginGpuTimestampQuery(cmd, m_graphicsProfileToken, "Blit Texture");
    // Draw computed results
    cmdBindPipeline(cmd, pDisplayTexturePipeline);
    cmdBindDescriptorSet(cmd, m_frameIndex, pDescriptorSetTexture);
    cmdDraw(cmd, 3, 0);
    cmdEndGpuTimestampQuery(cmd, m_graphicsProfileToken);


	barrier = { pRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
	cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, &barrier);

	//draw UI

	cmdEndGpuFrameProfile(cmd, m_graphicsProfileToken);
	//end command list
	endCmd(cmd);
	//allCmds.push_back(cmd);

    FlushResourceUpdateDesc flushUpdateDesc = {};
    flushUpdateDesc.mNodeIndex = 0;
    flushResourceUpdates(&flushUpdateDesc);
    Semaphore* waitSemaphores[2] = { flushUpdateDesc.pOutSubmittedSemaphore, pImageAcquiredSemaphore };

    QueueSubmitDesc submitDesc = {};
    submitDesc.mCmdCount = 1;
    submitDesc.mSignalSemaphoreCount = 1;
    submitDesc.mWaitSemaphoreCount = TF_ARRAY_COUNT(waitSemaphores);
    submitDesc.ppCmds = &cmd;
    submitDesc.ppSignalSemaphores = &elem.pSemaphore;
    submitDesc.ppWaitSemaphores = waitSemaphores;
    submitDesc.pSignalFence = elem.pFence;
    queueSubmit(pGraphicsQueue, &submitDesc);
    QueuePresentDesc presentDesc = {};
    presentDesc.mIndex = (uint8_t)swapchainImageIndex;
    presentDesc.mWaitSemaphoreCount = 1;
    presentDesc.pSwapChain = pSwapChain;
    presentDesc.ppWaitSemaphores = &elem.pSemaphore;
    presentDesc.mSubmitDone = true;

    queuePresent(pGraphicsQueue, &presentDesc);
    flipProfiler();

	m_frameIndex = (m_frameIndex + 1) % gImageCount;
	m_frameFlipFlop ^= 1;

}

void TiledShadingSample::Exit()
{
	exitInputSystem();
	//exitThreadSystem(pThreadSystem);
	exitCameraController(m_camController);

	m_frameIndex = 0;
	m_frameFlipFlop = 0;

	m_skybox->Exit();
    tf_delete(m_skybox); // TODO perhaps put on stack?

	exitProfiler();

	exitUserInterface();

	exitFontSystem();

	removeGpuCmdRing(pRenderer, &gGraphicsCmdRing);
	removeSemaphore(pRenderer, pImageAcquiredSemaphore);
	//for (uint32_t i = 0; i < gImageCount; ++i)
	//{
	//	removeFence(pRenderer, pRenderCompleteFences[i]);
	//	removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
	//}

	removeSampler(pRenderer,  pSamplerTrilinearAniso);
	removeSampler(pRenderer, GetBilinearSampler());
	removeSampler(pRenderer, pSamplerPointClamp);
	removeSampler(pRenderer, pSamplerBilinearClamp);

	// Remove commands and command pool&
	for (uint32_t i = 0; i < gImageCount; ++i)
	{
		removeCmd(pRenderer, pCmds[i]);
		removeCmdPool(pRenderer, pCmdPools[i]);
	}

	removeQueue(pRenderer, pGraphicsQueue);

	exitResourceLoaderInterface(pRenderer);
	exitRenderer(pRenderer);
	pRenderer = nullptr;
}

void TiledShadingSample::removeRenderTargets()
{
	removeRenderTarget(pRenderer, pDepthBuffer);
	removeRenderTarget(pRenderer, pSceneBuffer);
	for (uint32_t i = 0; i < DEFERRED_RT_COUNT; ++i)
		removeRenderTarget(pRenderer, pRenderTargetDeferredPass[i]);
}

bool TiledShadingSample::Load(ReloadDesc* pReloadDesc)
{
	gResourceSyncStartToken = getLastTokenCompleted();

	if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
	{
		if (!addSwapChain())
			return false;

		addRenderTargets();
	}

	/************************************************************************/
    // Blit texture
    /************************************************************************/
    if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
    {
        const char* displayTextureVertShader = "DisplayTexture.vert";

        const char* displayTextureFragShader = "DisplayTexture.frag";

        ShaderLoadDesc displayShader = {};
        displayShader.mStages[0].pFileName = displayTextureVertShader;
        displayShader.mStages[1].pFileName = displayTextureFragShader;
        addShader(pRenderer, &displayShader, &pDisplayTextureShader);

        const char*       pStaticSamplers[] = { "uSampler0" };
        RootSignatureDesc rootDesc = {};
        rootDesc.mStaticSamplerCount = 1;
        rootDesc.ppStaticSamplerNames = pStaticSamplers;
        rootDesc.ppStaticSamplers = &pSamplerPointClamp;
        rootDesc.mShaderCount = 1;
        rootDesc.ppShaders = &pDisplayTextureShader;
        addRootSignature(pRenderer, &rootDesc, &pDisplayTextureSignature);

		DescriptorSetDesc setDesc = { pDisplayTextureSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gDataBufferCount };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetTexture);
    }

	if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
    {
        RasterizerStateDesc rasterizerStateDesc = {};
        rasterizerStateDesc.mCullMode = CULL_MODE_NONE;

        PipelineDesc graphicsPipelineDesc = {};
        graphicsPipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
        GraphicsPipelineDesc& pipelineSettings = graphicsPipelineDesc.mGraphicsDesc;
        pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        pipelineSettings.pRasterizerState = &rasterizerStateDesc;
        pipelineSettings.mRenderTargetCount = 1;
        pipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
        pipelineSettings.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
        pipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
        pipelineSettings.pVertexLayout = NULL;
        pipelineSettings.pRootSignature = pDisplayTextureSignature;
        pipelineSettings.pShaderProgram = pDisplayTextureShader;
        addPipeline(pRenderer, &graphicsPipelineDesc, &pDisplayTexturePipeline);
    }

	DescriptorData params[1] = {};
	for (uint32_t i = 0; i < gDataBufferCount; ++i)
    {
        params[0].pName = "uTex0";
        params[0].ppTextures = &pSceneBuffer->pTexture;
        updateDescriptorSet(pRenderer, i, pDescriptorSetTexture, 1 , params);
    } 
	// end Blit texture

	//TODO Should it be here?
	TinyImageFormat deferredFormats[DEFERRED_RT_COUNT] = {};
	for (uint32_t i = 0; i < DEFERRED_RT_COUNT; ++i)
	{
		m_deferredFormats[i] = pRenderTargetDeferredPass[i]->mFormat;
	}

	waitForAllResourceLoads();

	UserInterfaceLoadDesc uiLoad = {};
	uiLoad.mColorFormat = pSwapChain->ppRenderTargets[0]->mFormat;
	uiLoad.mHeight = mSettings.mHeight;
	uiLoad.mWidth = mSettings.mWidth;
	uiLoad.mLoadType = pReloadDesc->mType;
	loadUserInterface(&uiLoad);

	FontSystemLoadDesc fontLoad = {};
	fontLoad.mColorFormat = pSwapChain->ppRenderTargets[0]->mFormat;
	fontLoad.mHeight = mSettings.mHeight;
	fontLoad.mWidth = mSettings.mWidth;
	fontLoad.mLoadType = pReloadDesc->mType;
	loadFontSystem(&fontLoad);

	m_skybox->Load(pReloadDesc);

	return true;
}

void TiledShadingSample::Unload(ReloadDesc* pReloadDesc)
{
    waitQueueIdle(pGraphicsQueue);
    if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
    {
        //removeDescriptorSets();
        //removeRootSignatures();
        //removeShaders();
        removeDescriptorSet(pRenderer, pDescriptorSetTexture);
        removeRootSignature(pRenderer, pDisplayTextureSignature);
        removeShader(pRenderer, pDisplayTextureShader);
    }

	if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
    {
        //removePipelines();
        removePipeline(pRenderer, pDisplayTexturePipeline);
    }

	if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
	{
		m_skybox->Unload(pReloadDesc);

		unloadFontSystem(pReloadDesc->mType);
		unloadUserInterface(pReloadDesc->mType);
		waitForToken(&gResourceSyncToken);
		waitForAllResourceLoads();

		gResourceSyncToken = 0;

		removeSwapChain(pRenderer, pSwapChain);
		removeRenderTargets();


		for (uint i = 0; i < TOTAL_IMGS; ++i)
		{
			if (pMaterialTextures[i])
			{
				removeResource(pMaterialTextures[i]);
			}
		}
		removeResource(gModels[0]);
		removeResource(gModels[1]);

	}
}

void TiledShadingSample::Update(float deltaTime)
{
	updateInputSystem(deltaTime, mSettings.mWidth, mSettings.mHeight);
	m_camController->update(deltaTime);
    pCameraController->update(deltaTime);
	m_skybox->Update(deltaTime);
}

uint32_t TiledShadingSample::GetFrameIndex()
{
	return m_frameIndex;
}

ProfileToken TiledShadingSample::GetGpuProfileToken()
{
	return m_graphicsProfileToken;
}

RenderTarget* TiledShadingSample::GetDepthBuffer()
{
	return pDepthBuffer;
}

RenderTarget** TiledShadingSample::GetGbuffers() 
{
	return pRenderTargetDeferredPass;
}

RenderTarget* TiledShadingSample::GetMainRenderTarget()
{ 
	return pSceneBuffer;
}

TinyImageFormat* TiledShadingSample::GetDeferredColorFormats()
{
	return m_deferredFormats;
}

TinyImageFormat TiledShadingSample::GetMainRenderTargetFormat()
{
	return pSceneBuffer->mFormat;
}

VertexLayout* TiledShadingSample::GetVertexLayout()
{
	return &m_vertexLayoutModel;
}

void TiledShadingSample::InitVertexLayout()
{
	// Create vertex layout
	m_vertexLayoutModel.mAttribCount = 3;

	m_vertexLayoutModel.mAttribs[0].mSemantic = SEMANTIC_POSITION;
	m_vertexLayoutModel.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
	m_vertexLayoutModel.mAttribs[0].mBinding = 0;
	m_vertexLayoutModel.mAttribs[0].mLocation = 0;
	m_vertexLayoutModel.mAttribs[0].mOffset = 0;

	//normals
	m_vertexLayoutModel.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
	m_vertexLayoutModel.mAttribs[1].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
	m_vertexLayoutModel.mAttribs[1].mLocation = 1;
	m_vertexLayoutModel.mAttribs[1].mBinding = 0;
	m_vertexLayoutModel.mAttribs[1].mOffset = 3 * sizeof(float);

	//texture
	m_vertexLayoutModel.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD0;
	m_vertexLayoutModel.mAttribs[2].mFormat = TinyImageFormat_R32G32_SFLOAT;
	m_vertexLayoutModel.mAttribs[2].mLocation = 2;
	m_vertexLayoutModel.mAttribs[2].mBinding = 0;
	m_vertexLayoutModel.mAttribs[2].mOffset = 6 * sizeof(float);    // first attribute contains 3 floats
}

void TiledShadingSample::loadMesh(size_t index)
{
	//Load Sponza
	GeometryLoadDesc loadDesc = {};
	loadDesc.pFileName = gModelNames[index];
	loadDesc.ppGeometry = &gModels[index];
	loadDesc.pVertexLayout = &m_vertexLayoutModel;
	addResource(&loadDesc, &gResourceSyncToken);
}

void TiledShadingSample::loadTexture(size_t index)
{
	TextureLoadDesc textureDesc = {};
	textureDesc.pFileName = pMaterialImageFileNames[index];
	textureDesc.ppTexture = &pMaterialTextures[index];
	if (strstr(pMaterialImageFileNames[index], "Albedo") || strstr(pMaterialImageFileNames[index], "diffuse"))
	{
		// Textures representing color should be stored in SRGB or HDR format
		textureDesc.mCreationFlag = TEXTURE_CREATION_FLAG_SRGB;
	}
	addResource(&textureDesc, &gResourceSyncToken);
}


DEFINE_APPLICATION_MAIN(TiledShadingSample)


