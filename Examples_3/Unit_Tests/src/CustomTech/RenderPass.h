#pragma once
struct Cmd;
struct Geometry;
struct ReloadDesc;

class RenderPass
{
public:
	virtual bool Init() = 0;
	virtual bool Load(ReloadDesc* pReloadDesc) = 0;
	virtual void Unload(ReloadDesc* pReloadDesc) = 0;
	virtual void Exit() = 0;
	virtual void Draw(Cmd* cmd) = 0;
	virtual void Update(float deltaTime) = 0;
};