// Interfaces
#include "../../../../Common_3/Application/Interfaces/IApp.h"

// Renderer
#include "../../../../Common_3/Utilities/RingBuffer.h"
#include "../../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../../Common_3/Application/Interfaces/ICameraController.h"
#include "../../../../Common_3/Application/Interfaces/IUI.h"

// But we only need Two sets of resources (one in flight and one being used on CPU)
const uint32_t gDataBufferCount = 2;

// Vertex buffer data
struct Vertex
{
    vec3 position;
    vec3 color;
};

const Vertex cubeVertices[] = {
    vec3(0.0f, 0.5f,  0.5f) // position
};
const float gCubeVertices[] = {
    0.0f,  0.5f,  0.5f, // position
    1.0f,  0.0f,  0.0f, // color
    0.5f,  -0.5f, 0.5f,
    0.0f, 1.0f, 0.0f,
    -0.5f, -0.5f, 0.5f,
    0.0f, 0.0f, 1.0f,

};

SwapChain* pSwapChain = nullptr;
Renderer*  pRenderer = nullptr;
Queue*     pGraphicsQueue = nullptr;
GpuCmdRing gGraphicsCmdRing = {};

Semaphore* pImageAcquiredSemaphore = nullptr;

uint32_t gFrameIndex = 0;


class Voxels: public IApp
{
public:
    bool Init()
    {
        // Initialize the settings and renderer
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

        initResourceLoaderInterface(pRenderer); // needed for buffers etc.
        waitForAllResourceLoads(); // Add this after adding resourceLoader

        AddCustomInputBindings();
        gFrameIndex = 0;

        return true;
    }
    void Exit()
    {
        exitQueue(pRenderer, pGraphicsQueue);
        exitGpuCmdRing(pRenderer, &gGraphicsCmdRing);
        exitSemaphore(pRenderer, pImageAcquiredSemaphore);

        
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
        }

        if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
        {
            removeSwapChain(pRenderer, pSwapChain);
        }
        if (pReloadDesc->mType && RELOAD_TYPE_SHADER)
        {
            removeShaders();
            //removeRootSignature(pRenderer, pRootSignature);
            removeDescriptorSets();
        }
    }

    void Update(float deltaTime)
    {

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

        RenderTarget*     pRenderTarget = pSwapChain->ppRenderTargets[swapchainImageIndex];
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

        // Transition RT into RT state
        RenderTargetBarrier barriers[] = {
            { pRenderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET },
        };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);

        // simply record the screen cleaning command
        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { pRenderTarget, LOAD_ACTION_CLEAR };
        bindRenderTargets.mRenderTargets[0].mClearValue = { 0.0f, 0.0f, 1.0f, 1.0f };
        bindRenderTargets.mRenderTargets[0].mOverrideClearValue = true;

        cmdBindRenderTargets(cmd, &bindRenderTargets);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);



        // Transition back into preset state
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

    const char* GetName() { return "Voxels"; }

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

    }

    void removeShaders() 
    {

    }

    void addRootSignatures()
    {

    }

    void addPipelines()
    {
 
    }

    void prepareDescriptorSets()
    {
    }

    void removeDescriptorSets()
    {

    }

    void addDescriptorSets()
    {

    }
};

DEFINE_APPLICATION_MAIN(Voxels)