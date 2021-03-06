#pragma once
#include "common.hpp"
#include "glutil.hpp"

#define GL_TEXTURE_MAX_ANISOTROPY_EXT     0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF

struct Image;

struct Texture
{
	Texture() {}
	~Texture();

	NONCOPYABLE(Texture);

	uint id = 0;
	uint type = 0;
	uint wrapS = GL_REPEAT;
	uint wrapT = GL_REPEAT;
	uint wrapR = GL_REPEAT;
	uint minFilter = GL_LINEAR_MIPMAP_LINEAR;
	uint magFilter = GL_LINEAR;
	float anisotropy = 1.0f;

	void create();
	void upload(Image& image);
	void uploadCube(Image* images[6]);
	void uploadArray(const std::vector<Image*>& images);
	void update();
	void destroy();
	bool valid() const { return id > 0; }
};
