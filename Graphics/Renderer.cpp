#include "pch.h"
#include <glad/glad.h>
#include "Renderer.h"
#include "Texture.h"
#include "Shader.h"

namespace Graphics
{
	struct Framebuffer
	{
		GLuint RendererID;
		GLuint VerticalColorAttachment;
		GLuint HorizontalColorAttachment;
		uint32_t Width = 1280, Height = 720;
	};

	struct RendererData
	{
		Framebuffer RenderTarget;
		GLuint VertexArray;

		std::shared_ptr<Graphics::Shader> GaussianBlurVerticalPass;
		std::shared_ptr<Graphics::Shader> GaussianBlurHorizontalPass;

		GLuint ActiveTexture = 0;
	} static s_Data;

	struct DrawArraysIndirectCommand
	{
		GLuint Count;
		GLuint PrimCount;
		GLuint First;
		GLuint BaseInstance;

		DrawArraysIndirectCommand(GLuint count, GLuint primCount, GLuint first, GLuint baseInstance)
			: Count(count), PrimCount(primCount), First(first), BaseInstance(baseInstance)
		{

		}
	} static const indirect(6, 2, 0, 0);

	void Renderer::Init()
	{
		ShaderLibrary::Init();
		TextureLibrary::Init();

		s_Data.GaussianBlurVerticalPass = Graphics::ShaderLibrary::LoadShader("GaussianBlur13x13Vertical.glsl");
		s_Data.GaussianBlurHorizontalPass = Graphics::ShaderLibrary::LoadShader("GaussianBlur13x13Horizontal.glsl");

		glEnable(GL_MULTISAMPLE);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

		glCreateVertexArrays(1, &s_Data.VertexArray);
		glBindVertexArray(s_Data.VertexArray);
		
		ConstructFramebuffer();
	}

	void Renderer::Shutdown()
	{
		glDeleteVertexArrays(1, &s_Data.VertexArray);

		glDeleteFramebuffers(1, &s_Data.RenderTarget.RendererID);
		glDeleteTextures(1, &s_Data.RenderTarget.VerticalColorAttachment);
		glDeleteTextures(1, &s_Data.RenderTarget.HorizontalColorAttachment);
	}

	GLuint Renderer::DrawTextureFiltered(const Texture2D* texture)
	{
		ResizeFramebuffer(texture->Width, texture->Height);
		
		glBindFramebuffer(GL_FRAMEBUFFER, s_Data.RenderTarget.RendererID);
		glViewport(0, 0, s_Data.RenderTarget.Width, s_Data.RenderTarget.Height);
		
		RenderTexturePass(texture->TextureID, s_Data.RenderTarget.VerticalColorAttachment, s_Data.GaussianBlurVerticalPass.get());
		RenderTexturePass(s_Data.RenderTarget.VerticalColorAttachment, s_Data.RenderTarget.HorizontalColorAttachment, s_Data.GaussianBlurHorizontalPass.get());

		for (int i = 0; i < 9; i++)
		{
			RenderTexturePass(s_Data.RenderTarget.HorizontalColorAttachment, s_Data.RenderTarget.VerticalColorAttachment, s_Data.GaussianBlurVerticalPass.get());
			RenderTexturePass(s_Data.RenderTarget.VerticalColorAttachment, s_Data.RenderTarget.HorizontalColorAttachment, s_Data.GaussianBlurHorizontalPass.get());
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		return s_Data.RenderTarget.HorizontalColorAttachment;
	}

	void Renderer::RenderTexturePass(GLuint inputTexture, GLuint outputTexture, Shader* filterPass)
	{
		glNamedFramebufferTexture(s_Data.RenderTarget.RendererID, GL_COLOR_ATTACHMENT0, outputTexture, 0);
		glBindTextureUnit(0, inputTexture);
		filterPass->Bind();

		glDrawArraysIndirect(GL_TRIANGLES, &indirect);
	}

	void Renderer::ResizeFramebuffer(uint32_t width, uint32_t height)
	{
		if (s_Data.RenderTarget.Width != width && s_Data.RenderTarget.Height != height)
		{
			s_Data.RenderTarget.Width = width;
			s_Data.RenderTarget.Height = height;

			glDeleteFramebuffers(1, &s_Data.RenderTarget.RendererID);
			glDeleteTextures(1, &s_Data.RenderTarget.VerticalColorAttachment);
			glDeleteTextures(1, &s_Data.RenderTarget.HorizontalColorAttachment);

			ConstructFramebuffer();

			Core::Log::Trace("Framebuffer Resized to : {0}, {1}", width, height);
		}
	}

	void Renderer::ConstructFramebuffer()
	{
		glCreateTextures(GL_TEXTURE_2D, 1, &s_Data.RenderTarget.VerticalColorAttachment);
		glTextureParameteri(s_Data.RenderTarget.VerticalColorAttachment, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(s_Data.RenderTarget.VerticalColorAttachment, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureStorage2D(s_Data.RenderTarget.VerticalColorAttachment, 1, GL_RGBA8, s_Data.RenderTarget.Width, s_Data.RenderTarget.Height);

		glCreateTextures(GL_TEXTURE_2D, 1, &s_Data.RenderTarget.HorizontalColorAttachment);
		glTextureParameteri(s_Data.RenderTarget.HorizontalColorAttachment, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(s_Data.RenderTarget.HorizontalColorAttachment, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureStorage2D(s_Data.RenderTarget.HorizontalColorAttachment, 1, GL_RGBA8, s_Data.RenderTarget.Width, s_Data.RenderTarget.Height);

		glCreateFramebuffers(1, &s_Data.RenderTarget.RendererID);

		LOG_ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Framebuffer does not meet the requirements!");
	}
}