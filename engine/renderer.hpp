#pragma once
#include "common.hpp"
#include "environment.hpp"

class RenderDevice;
class Resources;
struct Camera;
struct Model;

class RenderSystem : public System
{
public:
	RenderSystem(Resources& resources);
	~RenderSystem();

	void render(Entities& entities, Camera& camera);
	void reset(Entities& entities);

	Environment& env() { return m_env; }

	void dumpStats() const;
	void toggleWireframe();

	RenderDevice& device() { return *m_device; }

private:
	std::unique_ptr<RenderDevice> m_device;
	std::vector<Model*> m_models;
	Environment m_env;
};
