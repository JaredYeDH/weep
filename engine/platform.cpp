#include "platform.hpp"
#include "renderer.hpp"
#include "camera.hpp"

#include <SDL2/SDL.h>

namespace
{
	static SDL_Window* s_window = nullptr;
	static SDL_GLContext s_glcontext = nullptr;
}

void Platform::init()
{
	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		panic(SDL_GetError());
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	std::string err;
	Json settings = Json::parse(readFile("settings.json"), err);
	if (!err.empty())
		panic(err.c_str());

	int w = settings["screen"]["width"].int_value();
	int h = settings["screen"]["height"].int_value();
	int fullscreen = settings["screen"]["fullscreen"].bool_value() ? SDL_WINDOW_FULLSCREEN : 0;

	s_window = SDL_CreateWindow("App",
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h,
		SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | fullscreen);
	if (!s_window) {
		panic(SDL_GetError());
	}

	s_glcontext = SDL_GL_CreateContext(s_window);
	if (!s_glcontext) {
		panic(SDL_GetError());
	}

	Renderer::create();
	int major, minor;
	SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &major);
	SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &minor);
	logInfo("OpenGL Context:  %d.%d", major, minor);

	//atexit(Platform::deinit);
}

void Platform::deinit()
{
	Renderer::destroy();
	SDL_GL_DeleteContext(s_glcontext);
	SDL_DestroyWindow(s_window);
	SDL_Quit();
}

void Platform::run()
{
	Camera camera;
	camera.makePerspective(0.5, 1280.0f / 720.0f, 0.1, 1000);
	SDL_Event e;
	while (true) {
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT)
				return;
		}
		GetRenderer().render(camera);
		SDL_GL_SwapWindow(s_window);
	}
}
