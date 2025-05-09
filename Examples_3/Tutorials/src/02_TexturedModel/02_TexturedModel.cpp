// Interfaces
#include "../../../../Common_3/Application/Interfaces/IApp.h"

// Renderer
#include "../../../../Common_3/Utilities/RingBuffer.h"
#include "../../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../../Common_3/Application/Interfaces/ICameraController.h"
#include "../../../../Common_3/Application/Interfaces/IUI.h"


// But we only need Two sets of resources (one in flight and one being used on CPU)
const uint32_t gDataBufferCount = 2;

struct UniformCamData
{
    CameraMatrix mProjectView;
    CameraMatrix mInvProjectView;

    vec3 mCamPos;
};

struct UniformObjectData
{
    mat4 mWorldMat;
};

ICameraController* pCameraController = NULL;

VertexLayout gVertexLayoutDefault = {};

SwapChain* pSwapChain = nullptr;
Renderer*  pRenderer = nullptr;
Queue*     pGraphicsQueue = nullptr;
GpuCmdRing gGraphicsCmdRing = {};

Semaphore* pImageAcquiredSemaphore = nullptr;

uint32_t gFrameIndex = 0;

Geometry*      pModelGeo;
Pipeline* pMeshPipeline = nullptr;
Shader* pMeshShader = nullptr;
RootSignature* pRootSignature = nullptr;

UniformCamData gUniformDataCamera;
UniformObjectData gUniformDataObject;

DescriptorSet* pCameraDescriptor;
DescriptorSet* pObjectDescriptor;
DescriptorSet* pTextureDescriptor;

Buffer* pUniformBufferCamera[gDataBufferCount] = { NULL };
Buffer* pUniformBufferObject[gDataBufferCount] = { NULL };

Texture* pModelTexture = nullptr;
Sampler* pSamplerPointRepeat = nullptr;


class TexturedModel: public IApp
{ 
public:
    bool Init()
    {
        //Initialize the settings and renderer
        RendererDesc settings;
        memset(&settings, 0, sizeof(settings));

        initGPUConfiguration(settings.pExtendedSettings);
        initRenderer(GetName(), &settings, &pRenderer);

        // check for init success
        if (!pRenderer)
        {
            ShowUnsupportedMessage("Failed To Initialize renderer!");
            return false;
        }
        setupGPUConfigurationPlatformParameters(pRenderer, settings.pExtendedSettings);

        QueueDesc queueDesc = {};
        queueDesc.mType = QUEUE_TYPE_GRAPHICS;
        queueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;
        initQueue(pRenderer, &queueDesc, &pGraphicsQueue);

        GpuCmdRingDesc cmdRingDesc = {};
        cmdRingDesc.pQueue = pGraphicsQueue;
        cmdRingDesc.mPoolCount = gDataBufferCount;
        cmdRingDesc.mCmdPerPoolCount = 1;
        cmdRingDesc.mAddSyncPrimitives = true;
        initGpuCmdRing(pRenderer, &cmdRingDesc, &gGraphicsCmdRing);

        initSemaphore(pRenderer, &pImageAcquiredSemaphore);

                // INITIALIZE CAMERA & INPUT
        //
        CameraMotionParameters camParameters{ 100.0f, 150.0f, 300.0f };
        vec3                   camPos{ -0.21f, 12.2564745f, 59.3652649f };
        vec3                   lookAt{ 0, 0, 0 };

        pCameraController = initFpsCameraController(camPos, lookAt);
        pCameraController->setMotionParameters(camParameters);

        initResourceLoaderInterface(pRenderer); //needed for buffers etc.

        gVertexLayoutDefault.mBindingCount = 1;
        gVertexLayoutDefault.mAttribCount = 3;
        gVertexLayoutDefault.mAttribs[0].mSemantic = SEMANTIC_POSITION;
        gVertexLayoutDefault.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
        gVertexLayoutDefault.mAttribs[0].mBinding = 0;
        gVertexLayoutDefault.mAttribs[0].mLocation = 0;
        gVertexLayoutDefault.mAttribs[0].mOffset = 0;
        gVertexLayoutDefault.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
        gVertexLayoutDefault.mAttribs[1].mFormat = TinyImageFormat_R32_UINT;
        gVertexLayoutDefault.mAttribs[1].mLocation = 1;
        gVertexLayoutDefault.mAttribs[1].mBinding = 0;
        gVertexLayoutDefault.mAttribs[1].mOffset = 3 * sizeof(float);
        gVertexLayoutDefault.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD0;
        gVertexLayoutDefault.mAttribs[2].mFormat = TinyImageFormat_R32_UINT;
        gVertexLayoutDefault.mAttribs[2].mLocation = 2;
        gVertexLayoutDefault.mAttribs[2].mBinding = 0;
        gVertexLayoutDefault.mAttribs[2].mOffset = 3 * sizeof(float) + sizeof(uint32_t);

        // Load the model
        GeometryLoadDesc loadDesc = {};
        loadDesc.pFileName = "matBall.bin";
        loadDesc.ppGeometry = &pModelGeo;
        loadDesc.pVertexLayout = &gVertexLayoutDefault;
        addResource(&loadDesc, NULL);

        //Load the texture
        TextureLoadDesc texDesc = {};
        //texDesc.pFileName = "Bricks.tex"; //what are we loading?
        texDesc.pFileName = "PBR/brushed_iron_01/2K/Albedo.tex"; //what are we loading?
        //texDesc.pFileName = "PBR/Bricks.tex"; //what are we loading?
        texDesc.ppTexture = &pModelTexture;
        texDesc.mCreationFlag |= TEXTURE_CREATION_FLAG_SRGB;
        addResource(&texDesc, NULL);

        SamplerDesc samplerDesc = {};
        samplerDesc.mMinFilter = FILTER_NEAREST;
        samplerDesc.mMagFilter = FILTER_NEAREST;
        samplerDesc.mMipMapMode = MIPMAP_MODE_NEAREST;
        addSampler(pRenderer, &samplerDesc, &pSamplerPointRepeat);

        // Uniform buffer for camera data
        BufferLoadDesc cameraUBDesc = {};
        cameraUBDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        cameraUBDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        cameraUBDesc.mDesc.mSize = sizeof(UniformCamData);
        cameraUBDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        cameraUBDesc.pData = NULL;
        for (uint i = 0; i < gDataBufferCount; ++i)
        {
            cameraUBDesc.ppBuffer = &pUniformBufferCamera[i];
            addResource(&cameraUBDesc, NULL);
        }

        //Uniform buffer for object
        BufferLoadDesc objectUBDesc = {};
        objectUBDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        objectUBDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        objectUBDesc.mDesc.mSize = sizeof(UniformObjectData);
        objectUBDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        objectUBDesc.pData = NULL;
        for (uint i = 0; i < gDataBufferCount; ++i)
        {
            objectUBDesc.ppBuffer = &pUniformBufferObject[i];
            addResource(&objectUBDesc, NULL);
        }
     
        waitForAllResourceLoads(); //Add this after adding resourceLoader

        //Init model uniform

       // mat4 modelmat = mat4::translation(vec3(baseX - i - offsetX * i, baseY, baseZ)) * mat4::scale(vec3(scaleVal)) * mat4::rotationY(PI);
        mat4 modelmat = mat4::identity();
        gUniformDataObject;
        gUniformDataObject.mWorldMat = modelmat;

        AddCustomInputBindings();
        gFrameIndex = 0;

        return true;
    }
    void Exit()
    {

        exitQueue(pRenderer, pGraphicsQueue);
        exitGpuCmdRing(pRenderer, &gGraphicsCmdRing);
        exitSemaphore(pRenderer, pImageAcquiredSemaphore);

        exitCameraController(pCameraController);

        removeResource(pModelGeo);
        removeResource(pModelTexture);
        removeSampler(pRenderer, pSamplerPointRepeat);

        //delete the uniform buffers
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            removeResource(pUniformBufferCamera[i]);
            removeResource(pUniformBufferObject[i]);
        }

        exitResourceLoaderInterface(pRenderer);

        exitRenderer(pRenderer);
        exitGPUConfiguration();
        pRenderer = nullptr;
    }
    bool Load(ReloadDesc* pReloadDesc)
    {
        if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
        {
            addShaders();
            addRootSignatures();
            addDescriptorSets();
        }

        if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
        {
            if (!addSwapChain())
                return false;
        }

        if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
        {
            prepareDescriptorSets();
            addPipelines();
        }

        return true;
    }
    void Unload(ReloadDesc* pReloadDesc)
    {
        waitQueueIdle(pGraphicsQueue);

        if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
        {
            removePipeline(pRenderer, pMeshPipeline);
        }

        if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
        {
            removeSwapChain(pRenderer, pSwapChain);
        }
        if (pReloadDesc->mType && RELOAD_TYPE_SHADER)
        {
            removeShaders();
            removeRootSignature(pRenderer, pRootSignature);
            removeDescriptorSets();
        }
    }

    void Update(float deltaTime) 
    {
        if (!uiIsFocused())
        {
            pCameraController->onMove({ inputGetValue(0, CUSTOM_MOVE_X), inputGetValue(0, CUSTOM_MOVE_Y) });
            pCameraController->onRotate({ inputGetValue(0, CUSTOM_LOOK_X), inputGetValue(0, CUSTOM_LOOK_Y) });
            pCameraController->onMoveY(inputGetValue(0, CUSTOM_MOVE_UP));
        }

        //GuiController::UpdateDynamicUI();
        pCameraController->update(deltaTime);

        // calculate matrices
        mat4         viewMat = pCameraController->getViewMatrix();
        const float  aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
        const float  horizontal_fov = PI / 3.0f;
        CameraMatrix projMat = CameraMatrix::perspectiveReverseZ(horizontal_fov, aspectInverse, 0.1f, 1000.0f);

        // cameras
        gUniformDataCamera.mProjectView = projMat * viewMat;
        gUniformDataCamera.mInvProjectView = CameraMatrix::inverse(gUniformDataCamera.mProjectView);
        gUniformDataCamera.mCamPos = pCameraController->getViewPosition();
   
        vec4 frustumPlanes[6];
        CameraMatrix::extractFrustumClipPlanes(gUniformDataCamera.mProjectView, frustumPlanes[0], frustumPlanes[1], frustumPlanes[2],
                                               frustumPlanes[3], frustumPlanes[4], frustumPlanes[5], true);
    }
    void Draw() 
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

        // Stall if CPU is running "gDataBufferCount" frames ahead of GPU
        FenceStatus fenceStatus;
        getFenceStatus(pRenderer, elem.pFence, &fenceStatus);
        if (fenceStatus == FENCE_STATUS_INCOMPLETE)
            waitForFences(pRenderer, 1, &elem.pFence);

        // Reset cmd pool for this frame
        resetCmdPool(pRenderer, elem.pCmdPool);

        //Update constant buffers

        BufferUpdateDesc camBuffUpdateDesc = { pUniformBufferCamera[gFrameIndex] };
        beginUpdateResource(&camBuffUpdateDesc);
        memcpy(camBuffUpdateDesc.pMappedData, &gUniformDataCamera, sizeof(gUniformDataCamera));
        endUpdateResource(&camBuffUpdateDesc);

        BufferUpdateDesc objBuffUpdateDesc = { pUniformBufferObject[gFrameIndex] };
        beginUpdateResource(&objBuffUpdateDesc);
        memcpy(objBuffUpdateDesc.pMappedData, &gUniformDataObject, sizeof(gUniformDataObject));
        endUpdateResource(&objBuffUpdateDesc);

        
        Cmd* cmd = elem.pCmds[0];
        beginCmd(cmd);

        //Transition RT into RT state
        RenderTargetBarrier barriers[] = {
            { pRenderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET },
        };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);

        // simply record the screen cleaning command
        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { pRenderTarget,
                                                LOAD_ACTION_CLEAR }; 
        bindRenderTargets.mRenderTargets[0].mClearValue = { 0.0f, 0.0f, 1.0f, 1.0f };
        bindRenderTargets.mRenderTargets[0].mOverrideClearValue = true;

        cmdBindRenderTargets(cmd, &bindRenderTargets);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f); 
        cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

        //Draw model

        const uint32_t stride = sizeof(float) * 3 * 2;
        cmdBindPipeline(cmd, pMeshPipeline);
        cmdBindDescriptorSet(cmd, 0, pCameraDescriptor);
        cmdBindDescriptorSet(cmd, 0, pObjectDescriptor);
        cmdBindDescriptorSet(cmd, 0, pTextureDescriptor);
        cmdBindVertexBuffer(cmd, 1, &pModelGeo->pVertexBuffers[0], &pModelGeo->mVertexStrides[0], NULL);
        cmdBindIndexBuffer(cmd, pModelGeo->pIndexBuffer, pModelGeo->mIndexType, 0);

       // cmdBindDescriptorSet(cmd, 0, pMeshDescriptor[1]);
        cmdDrawIndexed(cmd, pModelGeo->mIndexCount, 0, 0);

        //Transition back into preset state
        cmdBindRenderTargets(cmd, NULL);

        barriers[0] = { pRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);

        endCmd(cmd);

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

        gFrameIndex = (gFrameIndex + 1) % gDataBufferCount;

    }

    const char* GetName() { return "02_TexturedModel"; }

    bool addSwapChain()
    {
        SwapChainDesc swapChainDesc = {};
        swapChainDesc.mWindowHandle = pWindow->handle;
        swapChainDesc.mPresentQueueCount = 1;
        swapChainDesc.ppPresentQueues = &pGraphicsQueue;
        swapChainDesc.mWidth = mSettings.mWidth;
        swapChainDesc.mHeight = mSettings.mHeight;
        swapChainDesc.mImageCount = getRecommendedSwapchainImageCount(pRenderer, &pWindow->handle);
        swapChainDesc.mColorFormat = getSupportedSwapchainFormat(pRenderer, &swapChainDesc, COLOR_SPACE_SDR_SRGB);
        swapChainDesc.mColorSpace = COLOR_SPACE_SDR_SRGB;
        swapChainDesc.mEnableVsync = mSettings.mVSyncEnabled;
        ::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

        return pSwapChain != nullptr;
    }

    void addShaders()
    {
        ShaderLoadDesc basicShader = {};
        basicShader.mVert.pFileName = "texturedModel.vert";
        basicShader.mFrag.pFileName = "texturedModel.frag";

        addShader(pRenderer, &basicShader, &pMeshShader);
    }

    void removeShaders()
    { 
        removeShader(pRenderer, pMeshShader);
    }

    void addRootSignatures() 
    {
        const char* pStaticSamplerName = "materialSampler";
        RootSignatureDesc rootDesc = {};
        rootDesc.mShaderCount = 1;
        rootDesc.ppShaders = &pMeshShader;
        rootDesc.mStaticSamplerCount = 1;
        rootDesc.ppStaticSamplerNames = &pStaticSamplerName;
        rootDesc.ppStaticSamplers = &pSamplerPointRepeat;
        addRootSignature(pRenderer, &rootDesc, &pRootSignature);
    }

    void addPipelines() 
    { 
        RasterizerStateDesc rasterizerStateDesc = {};
        rasterizerStateDesc.mCullMode = CULL_MODE_NONE;

        PipelineDesc graphicsPipelineDesc = {};
        graphicsPipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
        GraphicsPipelineDesc& pipelineSettings = graphicsPipelineDesc.mGraphicsDesc;
        pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        pipelineSettings.pDepthState = nullptr;
        pipelineSettings.mRenderTargetCount = 1;
        pipelineSettings.pRootSignature = pRootSignature;
        pipelineSettings.pShaderProgram = pMeshShader;
        pipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
        pipelineSettings.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
        pipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
        pipelineSettings.pVertexLayout = &gVertexLayoutDefault;
        pipelineSettings.pRasterizerState = &rasterizerStateDesc;
        pipelineSettings.mDepthStencilFormat = TinyImageFormat_UNDEFINED;
        addPipeline(pRenderer, &graphicsPipelineDesc, &pMeshPipeline);
    }

    void prepareDescriptorSets()
    {
        // per frame descriptors
        DescriptorData params[2] = {};
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            params[0].pName = "cbCamera";
            params[0].ppBuffers = &pUniformBufferCamera[i];
            updateDescriptorSet(pRenderer, i, pCameraDescriptor, 1, &params[0]);

            params[1].pName = "cbObject";
            params[1].ppBuffers = &pUniformBufferObject[i];
            updateDescriptorSet(pRenderer, i, pObjectDescriptor, 1, &params[1]);
        }
        DescriptorData textureParam[1] = {};
        textureParam[0].pName = "albedoMap";
        textureParam[0].ppTextures = &pModelTexture;
        updateDescriptorSet(pRenderer, 0, pTextureDescriptor, 1, textureParam);
    }

    void removeDescriptorSets() 
    { 
        removeDescriptorSet(pRenderer, pCameraDescriptor);
        removeDescriptorSet(pRenderer, pObjectDescriptor);
        removeDescriptorSet(pRenderer, pTextureDescriptor);
    }

    void addDescriptorSets() 
    { 
        DescriptorSetDesc setDesc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gDataBufferCount };
        addDescriptorSet(pRenderer, &setDesc, &pCameraDescriptor);

        setDesc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_DRAW, gDataBufferCount };
        addDescriptorSet(pRenderer, &setDesc, &pObjectDescriptor);

        setDesc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
        addDescriptorSet(pRenderer, &setDesc, &pTextureDescriptor);
    }
};

DEFINE_APPLICATION_MAIN(TexturedModel)