#pragma once
namespace ew {
	struct Framebuffer {
		unsigned int fbo;
		unsigned int colorBuffer;
		unsigned int depthBuffer;
		unsigned int width;
		unsigned int height;
	};
	Framebuffer createFramebuffer(unsigned int width, unsigned int height);
}