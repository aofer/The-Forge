#pragma once

#include "RenderPass.h"
#include "TiledShadingSample.h"

class GbufferPass : public RenderPass
{
public:
	GbufferPass(Renderer* renderer, TiledShadingSample* app);
	virtual ~GbufferPass() {};

	virtual bool Init();
	virtual bool Load(ReloadDesc* pReloadDesc);
	virtual void Unload(ReloadDesc* pReloadDesc);
	virtual void Exit();
	virtual void Draw(Cmd* cmd);
	virtual void Update(float deltaTime);

private:

	TiledShadingSample* m_app;
	Renderer* m_renderer = nullptr;

	Shader* m_gbuffersShader = NULL;
	Pipeline* m_gbufferPipeline = NULL;
	RootSignature* m_gbufferRootSignature = NULL;
	DescriptorSet* m_gbufferDescriptorSet[2] = { NULL };
    uint32_t       m_mapIDRootConstantIndex;

};