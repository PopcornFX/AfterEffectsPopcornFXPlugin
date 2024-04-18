//----------------------------------------------------------------------------
// This program is the property of Persistant Studios SARL.
//
// You may not redistribute it and/or modify it under any conditions
// without written permission from Persistant Studios SARL, unless
// otherwise stated in the latest Persistant Studios Code License.
//
// See the Persistant Studios Code License for further details.
//----------------------------------------------------------------------------

#include "precompiled.h"

#include "DebugHelper.h"
#include "ShaderDefinitions/EditorShaderDefinitions.h"
#include "ShaderLoader.h"

// Debug draw shaders:
#define		DEBUG_DRAW_VERTEX_SHADER_PATH			"./Shaders/DebugDraw.vert"
#define		DEBUG_DRAW_FRAGMENT_SHADER_PATH			"./Shaders/DebugDrawColor.frag"

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

bool	SDebugDraw::CreateRenderStates(	const RHI::PApiManager &apiManager,
										CShaderLoader &shaderLoader,
										TMemoryView<const RHI::SRenderTargetDesc>	frameBufferLayout,
										RHI::PRenderPass renderPass,
										CGuid mergeSubPassIdx)
{
	// m_LinesRenderState
	{
		m_LinesRenderState = apiManager->CreateRenderState(RHI::SRHIResourceInfos("Lines Render State"));
		if (m_LinesRenderState == null)
			return false;
		RHI::SRenderState	&renderState = m_LinesRenderState->m_RenderState;

		renderState.m_PipelineState.m_DynamicViewport = true;
		renderState.m_PipelineState.m_DynamicScissor = true;
		renderState.m_PipelineState.m_DepthTest = RHI::LessOrEqual;
		renderState.m_PipelineState.m_DepthWrite = false;
		renderState.m_PipelineState.m_Blending = false;
		renderState.m_PipelineState.m_DrawMode = RHI::DrawModeLine;

		if (!renderState.m_InputVertexBuffers.PushBack().Valid())
			return false;
		renderState.m_InputVertexBuffers.Last().m_Stride = sizeof(CFloat3);

		if (!renderState.m_InputVertexBuffers.PushBack().Valid())
			return false;
		renderState.m_InputVertexBuffers.Last().m_Stride = sizeof(CFloat4);

		PKSample::FillEditorDebugDrawShaderBindings(renderState.m_ShaderBindings, true, false);
		PKSample::CShaderLoader::SShadersPaths	paths;
		paths.m_Vertex = DEBUG_DRAW_VERTEX_SHADER_PATH;
		paths.m_Fragment = DEBUG_DRAW_FRAGMENT_SHADER_PATH;
		if (!shaderLoader.LoadShader(m_LinesRenderState->m_RenderState, paths, apiManager))
			return false;
		if (!apiManager->BakeRenderState(m_LinesRenderState, frameBufferLayout, renderPass, mergeSubPassIdx))
			return false;
	}
	return true;
}

//----------------------------------------------------------------------------

void	SDebugDraw::Draw(	const RHI::PApiManager		&apiManager,
							const RHI::PCommandBuffer	&cmdBuff,
							const RHI::PConstantSet		&sceneInfoConstantSet,
							const SDebugLines			&lines)
{
	if (!lines.Empty())
	{
		const u32	bufferBytePositionsCount = lines.m_Points.CoveredBytes();
		const u32	bufferByteColorCount = lines.m_Colors.CoveredBytes();

		if (m_LinesPointsColorBuffer.m_LinesPointsBuffer == null || m_LinesPointsColorBuffer.m_LinesPointsBuffer->GetByteSize() < bufferBytePositionsCount)
			m_LinesPointsColorBuffer.m_LinesPointsBuffer = apiManager->CreateGpuBuffer(RHI::SRHIResourceInfos("Lines Points Vertex Buffer"), RHI::VertexBuffer, bufferBytePositionsCount);

		if (m_LinesPointsColorBuffer.m_LinesColorBuffer == null || m_LinesPointsColorBuffer.m_LinesColorBuffer->GetByteSize() < bufferByteColorCount)
			m_LinesPointsColorBuffer.m_LinesColorBuffer = apiManager->CreateGpuBuffer(RHI::SRHIResourceInfos("Lines Colors Vertex Buffer"), RHI::VertexBuffer, bufferByteColorCount);

		if (m_LinesPointsColorBuffer.m_LinesPointsBuffer != null && m_LinesPointsColorBuffer.m_LinesColorBuffer != null)
		{
			void	*ptr = apiManager->MapCpuView(m_LinesPointsColorBuffer.m_LinesPointsBuffer);
			if (ptr != null)
			{
				Mem::Copy(ptr, lines.m_Points.RawDataPointer(), bufferBytePositionsCount);
				apiManager->UnmapCpuView(m_LinesPointsColorBuffer.m_LinesPointsBuffer);
			}
			ptr = apiManager->MapCpuView(m_LinesPointsColorBuffer.m_LinesColorBuffer);
			if (ptr != null)
			{
				Mem::Copy(ptr, lines.m_Colors.RawDataPointer(), bufferByteColorCount);
				apiManager->UnmapCpuView(m_LinesPointsColorBuffer.m_LinesColorBuffer);
			}

			cmdBuff->BindRenderState(m_LinesRenderState);

			cmdBuff->BindConstantSets(TMemoryView<const RHI::PConstantSet>(sceneInfoConstantSet));

			cmdBuff->BindVertexBuffers(TMemoryView<const RHI::PGpuBuffer>(&m_LinesPointsColorBuffer.m_LinesPointsBuffer, 2));

			PK_ASSERT(lines.m_Points.Count() == lines.m_Colors.Count());

			cmdBuff->Draw(0, lines.m_Points.Count());
		}
	}
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
