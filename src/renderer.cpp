#include "renderer.hpp"
#include "shader.hpp"
#include "geometry.hpp"
#include "platform.hpp"

#include <GL/glcorearb.h>

namespace
{
	ShaderProgram shader;
}

Renderer::Renderer()
{
	shader.compile(VERTEX_SHADER, Platform::readFile("shaders/core.vert"));
	shader.compile(VERTEX_SHADER, Platform::readFile("shaders/core.frag"));
	shader.link();
	shader.use();
}

Renderer::~Renderer()
{
}

void Renderer::addGeometry(Geometry* geometry)
{
	geometry->upload();
	geometries.push_back(geometry);
}

void Renderer::render()
{
	for (auto geom : geometries) {
		glBindVertexArray(geom->vao);
		glDrawArrays(GL_TRIANGLES, 0, geom->vertices.size());
	}
	glBindVertexArray(0);
}


