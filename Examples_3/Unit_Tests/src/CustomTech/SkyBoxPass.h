#pragma once

#include "RenderPass.h"
#include "TiledShadingSample.h"

#include "../../../../Common_3/Application/Interfaces/ICameraController.h"

// Have a uniform for camera data
struct UniformCamData
{
	CameraMatrix mProjectView;
    CameraMatrix mInvProjectView;
	vec3 mCamPos;
};

class SkyboxPass : public RenderPass
{
public:
	SkyboxPass(const char* skyboxPath, Renderer* renderer, TiledShadingSample* app);
	virtual ~SkyboxPass() {};

	virtual bool Init();
	virtual bool Load(ReloadDesc* pReloadDesc);
	virtual void Unload(ReloadDesc* pReloadDesc);
	virtual void Exit();
	virtual void Draw(Cmd* cmd);
	virtual void Update(float deltaTime);

private:

	TiledShadingSample* m_app;
	Renderer* m_renderer = nullptr;
    Texture* pPanoSkybox = NULL;
	Buffer* pSkyboxVertexBuffer = NULL;
	Shader* pSkyboxShader = NULL;
	Pipeline* pSkyboxPipeline = NULL;
	Pipeline* pSkyboxWithClearTexturesPipeline = NULL;
	RootSignature* pSkyboxRootSignature = NULL;
	DescriptorSet* pDescriptorSetSkybox[2] = { NULL };
	const char* m_skyboxPath;
	Texture* pSkyboxIrradianceMap = NULL;
	Buffer* pBufferUniformCameraSky[gImageCount] = { NULL };
	UniformCamData gUniformDataSky;

};