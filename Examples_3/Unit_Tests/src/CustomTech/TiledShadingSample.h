#pragma once

#include "../../../../Common_3/Application/Interfaces/IApp.h"
//Math
#include "../../../../Common_3/Utilities/Math/MathTypes.h"
#include "../../../../Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"
#include "../../../../Common_3/Application/Interfaces/IProfiler.h"
#include "../../../../Common_3/Utilities/RingBuffer.h"

//temp geometry stuff
#include "../../../../Common_3/Renderer/Interfaces/IVisibilityBuffer.h"

struct Cmd;
class ICameraController;
class SkyboxPass;
class GbufferPass;
struct Scene;

const uint32_t gImageCount = 2;
#define DEFERRED_RT_COUNT 4
#define TOTAL_IMGS 84

#pragma warning(disable : 4200) //TODO remove after removing the temp scene stuff

static ICameraController* pCameraController = NULL; //TODO figure out how we can get the camera input working without this being outside the class

class TiledShadingSample : public IApp
{

private:

	ICameraController* m_camController = nullptr;

	/************************************************************************/
	// Profiling
	/************************************************************************/
	ProfileToken m_graphicsProfileToken;
	/************************************************************************/
	// Rendering data
	/************************************************************************/
	Renderer* pRenderer = nullptr;
    GpuCmdRing    gGraphicsCmdRing = {};
	/************************************************************************/
	// Render targets
	/************************************************************************/
	RenderTarget* pRenderTargetDeferredPass[DEFERRED_RT_COUNT] = {  nullptr };
	RenderTarget* pDepthBuffer = nullptr;
	RenderTarget* pSceneBuffer = nullptr;
	/************************************************************************/
	// Samplers
	/************************************************************************/
	Sampler* pSamplerTrilinearAniso = nullptr;
	Sampler* pSamplerBilinear = nullptr;
	Sampler* pSamplerPointClamp = nullptr;
	Sampler* pSamplerBilinearClamp = nullptr;
	/************************************************************************/
	// Queues and Command buffers
	/************************************************************************/
	Queue* pGraphicsQueue = nullptr;
	CmdPool* pCmdPools[gImageCount];
	Cmd* pCmds[gImageCount];
	/************************************************************************/
	// Swapchain
	/************************************************************************/
	SwapChain* pSwapChain = nullptr;
	/************************************************************************/
	// Synchronization primitives
	/************************************************************************/
	//Fence* pRenderCompleteFences[gImageCount] = { nullptr };
	Semaphore* pImageAcquiredSemaphore = nullptr;
	//Semaphore* pRenderCompleteSemaphores[gImageCount] = { nullptr };

	SyncToken gResourceSyncStartToken = {};
	SyncToken gResourceSyncToken = {};

	//***********************************************************************/
	// Display to screen
	//***********************************************************************/
    Shader* pDisplayTextureShader = NULL;
    Pipeline* pDisplayTexturePipeline = NULL;
    RootSignature* pDisplayTextureSignature = NULL;
    DescriptorSet* pDescriptorSetTexture = NULL;

	//***********************************************************************/
	// Buffers
	//***********************************************************************/


	//***********************************************************************/
	// Passes
	//***********************************************************************/
	SkyboxPass* m_skybox;


	//TODO remove later after we fix gbuffer
	TinyImageFormat m_deferredFormats[DEFERRED_RT_COUNT];

	//***********************************************************************/
	// Temp scene stuff
	//***********************************************************************/

	//struct PropData
 //   {
 //       mat4             mWorldMatrix;
 //       Geometry*        pGeom = NULL;
 //       GeometryData*    pGeomData = NULL;
 //       uint32_t         mMeshCount = 0;
 //       uint32_t         mMaterialCount = 0;
 //       Buffer*          pConstantBuffer = NULL;
 //       Texture**        pTextureStorage = NULL;
 //       FilterContainer* pFilterContainers = NULL;
 //       MaterialFlags*   mMaterialFlags = NULL;
 //       uint32_t         numAlpha = 0;
 //       uint32_t         numNoAlpha = 0;
 //   };

	Scene*       SanMiguelScene;

	Texture* pMaterialTextures[TOTAL_IMGS];
	VertexLayout m_vertexLayoutModel = {};
    const uint32_t gDataBufferCount = 2;
	const char* pTextureName[5] = { "albedoMap", "normalMap", "metallicMap", "roughnessMap", "aoMap" };

	const char* gModelNames[2] = { "Sponza.gltf", "lion.gltf" };
	Geometry* gModels[2];
	uint32_t    gMaterialIds[193] = {
		0,  3,  1,  4,  5,  6,  7,  8,  6,  9,  7,  6, 10, 5, 7,  5, 6, 7,  6, 7,  6,  7,  6,  7,  6,  7,  6,  7,  6,  7,  6,  7,  6,  7,  6,
		5,  6,  5,  11, 5,  11, 5,  11, 5,  10, 5,  9, 8,  6, 12, 2, 5, 13, 0, 14, 15, 16, 14, 15, 14, 16, 15, 13, 17, 18, 19, 18, 19, 18, 17,
		19, 18, 17, 20, 21, 20, 21, 20, 21, 20, 21, 3, 1,  3, 1,  3, 1, 3,  1, 3,  1,  3,  1,  3,  1,  22, 23, 4,  23, 4,  5,  24, 5,
	};



	const char* pMaterialImageFileNames[TOTAL_IMGS] = {
	"SponzaPBR_Textures/ao",
	"SponzaPBR_Textures/ao",
	"SponzaPBR_Textures/ao",
	"SponzaPBR_Textures/ao",
	"SponzaPBR_Textures/ao",

	//common
	"SponzaPBR_Textures/ao",
	"SponzaPBR_Textures/Dielectric_metallic",
	"SponzaPBR_Textures/Metallic_metallic",
	"SponzaPBR_Textures/gi_flag",

	//Background
	"SponzaPBR_Textures/Background/Background_Albedo",
	"SponzaPBR_Textures/Background/Background_Normal",
	"SponzaPBR_Textures/Background/Background_Roughness",

	//ChainTexture
	"SponzaPBR_Textures/ChainTexture/ChainTexture_Albedo",
	"SponzaPBR_Textures/ChainTexture/ChainTexture_Metallic",
	"SponzaPBR_Textures/ChainTexture/ChainTexture_Normal",
	"SponzaPBR_Textures/ChainTexture/ChainTexture_Roughness",

	//Lion
	"SponzaPBR_Textures/Lion/Lion_Albedo",
	"SponzaPBR_Textures/Lion/Lion_Normal",
	"SponzaPBR_Textures/Lion/Lion_Roughness",

	//Sponza_Arch
	"SponzaPBR_Textures/Sponza_Arch/Sponza_Arch_diffuse",
	"SponzaPBR_Textures/Sponza_Arch/Sponza_Arch_normal",
	"SponzaPBR_Textures/Sponza_Arch/Sponza_Arch_roughness",

	//Sponza_Bricks
	"SponzaPBR_Textures/Sponza_Bricks/Sponza_Bricks_a_Albedo",
	"SponzaPBR_Textures/Sponza_Bricks/Sponza_Bricks_a_Normal",
	"SponzaPBR_Textures/Sponza_Bricks/Sponza_Bricks_a_Roughness",

	//Sponza_Ceiling
	"SponzaPBR_Textures/Sponza_Ceiling/Sponza_Ceiling_diffuse",
	"SponzaPBR_Textures/Sponza_Ceiling/Sponza_Ceiling_normal",
	"SponzaPBR_Textures/Sponza_Ceiling/Sponza_Ceiling_roughness",

	//Sponza_Column
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_a_diffuse",
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_a_normal",
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_a_roughness",

	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_b_diffuse",
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_b_normal",
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_b_roughness",

	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_c_diffuse",
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_c_normal",
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_c_roughness",

	//Sponza_Curtain
	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_Blue_diffuse",
	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_Blue_normal",

	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_Green_diffuse",
	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_Green_normal",

	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_Red_diffuse",
	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_Red_normal",

	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_metallic",
	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_roughness",

	//Sponza_Details
	"SponzaPBR_Textures/Sponza_Details/Sponza_Details_diffuse",
	"SponzaPBR_Textures/Sponza_Details/Sponza_Details_metallic",
	"SponzaPBR_Textures/Sponza_Details/Sponza_Details_normal",
	"SponzaPBR_Textures/Sponza_Details/Sponza_Details_roughness",

	//Sponza_Fabric
	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_Blue_diffuse",
	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_Blue_normal",

	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_Green_diffuse",
	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_Green_normal",

	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_metallic",
	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_roughness",

	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_Red_diffuse",
	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_Red_normal",

	//Sponza_FlagPole
	"SponzaPBR_Textures/Sponza_FlagPole/Sponza_FlagPole_diffuse",
	"SponzaPBR_Textures/Sponza_FlagPole/Sponza_FlagPole_normal",
	"SponzaPBR_Textures/Sponza_FlagPole/Sponza_FlagPole_roughness",

	//Sponza_Floor
	"SponzaPBR_Textures/Sponza_Floor/Sponza_Floor_diffuse",
	"SponzaPBR_Textures/Sponza_Floor/Sponza_Floor_normal",
	"SponzaPBR_Textures/Sponza_Floor/Sponza_Floor_roughness",

	//Sponza_Roof
	"SponzaPBR_Textures/Sponza_Roof/Sponza_Roof_diffuse",
	"SponzaPBR_Textures/Sponza_Roof/Sponza_Roof_normal",
	"SponzaPBR_Textures/Sponza_Roof/Sponza_Roof_roughness",

	//Sponza_Thorn
	"SponzaPBR_Textures/Sponza_Thorn/Sponza_Thorn_diffuse",
	"SponzaPBR_Textures/Sponza_Thorn/Sponza_Thorn_normal",
	"SponzaPBR_Textures/Sponza_Thorn/Sponza_Thorn_roughness",

	//Vase
	"SponzaPBR_Textures/Vase/Vase_diffuse",
	"SponzaPBR_Textures/Vase/Vase_normal",
	"SponzaPBR_Textures/Vase/Vase_roughness",

	//VaseHanging
	"SponzaPBR_Textures/VaseHanging/VaseHanging_diffuse",
	"SponzaPBR_Textures/VaseHanging/VaseHanging_normal",
	"SponzaPBR_Textures/VaseHanging/VaseHanging_roughness",

	//VasePlant
	"SponzaPBR_Textures/VasePlant/VasePlant_diffuse",
	"SponzaPBR_Textures/VasePlant/VasePlant_normal",
	"SponzaPBR_Textures/VasePlant/VasePlant_roughness",

	//VaseRound
	"SponzaPBR_Textures/VaseRound/VaseRound_diffuse",
	"SponzaPBR_Textures/VaseRound/VaseRound_normal",
	"SponzaPBR_Textures/VaseRound/VaseRound_roughness",

	"lion/lion_albedo",
	"lion/lion_specular",
	"lion/lion_normal",

	};



	uint32_t     gFontID = 0;

	const char* gSceneName = "SanMiguel.gltf";

	uint32_t m_frameIndex = 0;
	uint32_t m_frameFlipFlop = 0;

	bool addSwapChain();
	bool addSceneBuffer();
	bool addGBuffers();
	void addRenderTargets();
	void removeRenderTargets();
	bool addDepthBuffer();

void AddCameraControls();

public:

	TiledShadingSample();

	const char* GetName();

	//override
	void Exit() override;
	bool Init() override;
	void Draw() override;


	bool Load(ReloadDesc* pReloadDesc);
	void Unload(ReloadDesc* pReloadDesc);
	void Update(float deltaTime);

	//temp method to load the scene, remove when adding scene structure
	void loadMesh(size_t index);
	void loadTexture(size_t index);
	VertexLayout* GetVertexLayout();
	void InitVertexLayout();


	uint32_t GetFrameIndex();
	ProfileToken GetGpuProfileToken();
	RenderTarget* GetDepthBuffer();
	RenderTarget** GetGbuffers();
    RenderTarget*  GetMainRenderTarget();

	ICameraController* GetCameraController();


	Sampler* GetBilinearSampler() const { return pSamplerBilinear; }
	TinyImageFormat* GetDeferredColorFormats();
	TinyImageFormat GetMainRenderTargetFormat();
	uvec2 GetWindowSize();

	Scene* GetScene();
};