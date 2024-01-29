#pragma once
namespace ew {
	struct Framebuffer {
		unsigned int fbo;
		unsigned int colorBuffers[8];
		unsigned int depthBuffer;
		unsigned int width;
		unsigned int height;
	};
	Framebuffer createFramebuffer(unsigned int width, unsigned int height, int colorFormat);
	Framebuffer createDepthOnlyFramebuffer(unsigned int width, unsigned int height);
	Framebuffer createGBuffers(unsigned int width, unsigned int height);
}