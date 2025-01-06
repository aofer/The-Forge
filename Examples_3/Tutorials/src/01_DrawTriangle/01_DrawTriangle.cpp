// Interfaces
#include "../../../../Common_3/Application/Interfaces/IApp.h"

// Renderer
#include "../../../../Common_3/Utilities/RingBuffer.h"
#include "../../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../../Common_3/OS/Interfaces/IInput.h"


// But we only need Two sets of resources (one in flight and one being used on CPU)
const uint32_t gDataBufferCount = 2;

//Vertex buffer data
const float gTriangleVertices[] = {
    0.0f, 0.5f, 0.5f,   // position
    1.0f,  0.0f,   0.0f,   // color

    0.5f, -0.5f, 0.5f, 
    0.0f, 1.0f, 0.0f, 

    -0.5f, -0.5f, 0.5f, 
    0.0f, 0.0f, 1.0f, 

};

VertexLayout gTriangleVertexLayout = {};

SwapChain* pSwapChain = nullptr;
Renderer*  pRenderer = nullptr;
Queue*     pGraphicsQueue = nullptr;
GpuCmdRing gGraphicsCmdRing = {};

Semaphore* pImageAcquiredSemaphore = nullptr;


Buffer* pTriangleVerticesBuffer = nullptr;
Pipeline* pTrianglePipeline = nullptr;
Shader* pTriangleShader = nullptr;
RootSignature* pRootSignature = nullptr;

class DrawTriangle: public IApp
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

        initResourceLoaderInterface(pRenderer); //needed for buffers etc.

        //Init the vertex buffer for the triangle
        uint64_t       triangleDataSize = 3 * 3 * 2 * sizeof(float);
        BufferLoadDesc triangleVbDesc = {};
        triangleVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
        triangleVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        triangleVbDesc.mDesc.mSize = triangleDataSize;
        triangleVbDesc.pData = gTriangleVertices;
        triangleVbDesc.ppBuffer = &pTriangleVerticesBuffer;
        addResource(&triangleVbDesc, NULL);

        gTriangleVertexLayout.mBindingCount = 1;
        gTriangleVertexLayout.mAttribCount = 2;
        gTriangleVertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
        gTriangleVertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
        gTriangleVertexLayout.mAttribs[0].mBinding = 0;
        gTriangleVertexLayout.mAttribs[0].mLocation = 0;
        gTriangleVertexLayout.mAttribs[0].mOffset = 0;
        gTriangleVertexLayout.mAttribs[1].mSemantic = SEMANTIC_COLOR;
        gTriangleVertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
        gTriangleVertexLayout.mAttribs[1].mLocation = 1;
        gTriangleVertexLayout.mAttribs[1].mBinding = 0;
        gTriangleVertexLayout.mAttribs[1].mOffset = 3 * sizeof(float);

        waitForAllResourceLoads(); //Add this after adding resourceLoader

        return true;
    }
    void Exit()
    {

        exitQueue(pRenderer, pGraphicsQueue);
        exitGpuCmdRing(pRenderer, &gGraphicsCmdRing);
        exitSemaphore(pRenderer, pImageAcquiredSemaphore);

        removeResource(pTriangleVerticesBuffer);

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
        }

        if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
        {
            if (!addSwapChain())
                return false;
        }

        if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
        {
            addPipelines();
        }

        return true;
    }
    void Unload(ReloadDesc* pReloadDesc)
    {
        waitQueueIdle(pGraphicsQueue);

        if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
        {
            removePipeline(pRenderer, pTrianglePipeline);
        }

        if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
        {
            removeSwapChain(pRenderer, pSwapChain);
        }
        if (pReloadDesc->mType && RELOAD_TYPE_SHADER)
        {
            removeShaders();
            removeRootSignature(pRenderer, pRootSignature);
        }
    }

    void Update(float deltaTime) {}
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

        //Draw triangle

        const uint32_t stride = sizeof(float) * 3 * 2;
        cmdBindPipeline(cmd, pTrianglePipeline);
        cmdBindVertexBuffer(cmd, 1, &pTriangleVerticesBuffer, &stride, nullptr);
        cmdDraw(cmd, 3, 0);

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

    }

    const char* GetName() { return "01_DrawTriangle"; }

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
        basicShader.mVert.pFileName = "triangle.vert";
        basicShader.mFrag.pFileName = "triangle.frag";

        addShader(pRenderer, &basicShader, &pTriangleShader);
    }

    void removeShaders()
    { 
        removeShader(pRenderer, pTriangleShader);
    }

    void addRootSignatures() 
    {
        RootSignatureDesc rootDesc = {};
        rootDesc.mShaderCount = 1;
        rootDesc.ppShaders = &pTriangleShader;
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
        pipelineSettings.pShaderProgram = pTriangleShader;
        pipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
        pipelineSettings.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
        pipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
        pipelineSettings.pVertexLayout = &gTriangleVertexLayout;
        pipelineSettings.pRasterizerState = &rasterizerStateDesc;
        pipelineSettings.mDepthStencilFormat = TinyImageFormat_UNDEFINED;
        addPipeline(pRenderer, &graphicsPipelineDesc, &pTrianglePipeline);
    }
};

DEFINE_APPLICATION_MAIN(DrawTriangle)