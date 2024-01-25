#include "framebuffer.h"
#include "external/glad.h"
#include <stdio.h>

namespace ew {
	Framebuffer createFramebuffer(unsigned int width, unsigned int height)
	{
		Framebuffer framebuffer;
		framebuffer.width = width;
		framebuffer.height = height;

		glCreateFramebuffers(1, &framebuffer.fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.fbo);

		glGenTextures(1, &framebuffer.colorBuffer);
		glBindTexture(GL_TEXTURE_2D, framebuffer.colorBuffer);
		glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, framebuffer.colorBuffer, 0);

		glGenTextures(1, &framebuffer.depthBuffer);
		glBindTexture(GL_TEXTURE_2D, framebuffer.depthBuffer);
		glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT16, width, height);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, framebuffer.depthBuffer, 0);

		GLenum fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (fboStatus != GL_FRAMEBUFFER_COMPLETE) {
			printf("Framebuffer incomplete: %d", fboStatus);
		}
		glBindTexture(GL_TEXTURE_2D, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		return framebuffer;
	}
	Framebuffer createDepthOnlyFramebuffer(unsigned int width, unsigned int height)
	{
		Framebuffer framebuffer;
		framebuffer.width = width;
		framebuffer.height = height;

		glCreateFramebuffers(1, &framebuffer.fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.fbo);

		glGenTextures(1, &framebuffer.depthBuffer);
		glBindTexture(GL_TEXTURE_2D, framebuffer.depthBuffer);
		glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT16, width, height);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, framebuffer.depthBuffer, 0);

		glDrawBuffer(GL_NONE);

		GLenum fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (fboStatus != GL_FRAMEBUFFER_COMPLETE) {
			printf("Framebuffer incomplete: %d", fboStatus);
		}
		glBindTexture(GL_TEXTURE_2D, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		return framebuffer;
	}
}
