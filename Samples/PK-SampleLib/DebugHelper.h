#pragma once

//----------------------------------------------------------------------------
// This program is the property of Persistant Studios SARL.
//
// You may not redistribute it and/or modify it under any conditions
// without written permission from Persistant Studios SARL, unless
// otherwise stated in the latest Persistant Studios Code License.
//
// See the Persistant Studios Code License for further details.
//----------------------------------------------------------------------------

#include "PKSample.h"

#include <pk_rhi/include/FwdInterfaces.h>
#include <pk_rhi/include/interfaces/IRenderTarget.h>

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

class	CShaderLoader;

//----------------------------------------------------------------------------

struct	SDebugLines
{
	TArray<CFloat3>		m_Points;
	TArray<CFloat4>		m_Colors;

	bool	Empty() const { return m_Points.Empty() && m_Colors.Empty(); }
	void	Clear() { m_Points.Clear(); m_Colors.Clear(); }
};

//----------------------------------------------------------------------------

struct	SDebugDraw
{
	bool	CreateRenderStates(	const RHI::PApiManager &apiManager,
								CShaderLoader &shaderLoader,
								TMemoryView<const RHI::SRenderTargetDesc>	frameBufferLayout,
								RHI::PRenderPass renderPass,
								CGuid subPassIdx);

	void	Draw(	const RHI::PApiManager		&apiManager,
					const RHI::PCommandBuffer	&cmdBuff,
					const RHI::PConstantSet		&sceneInfoConstantSet,
					const SDebugLines			&lines);

	struct SLinePointsColorBuffer
	{
		RHI::PGpuBuffer				m_LinesPointsBuffer;
		RHI::PGpuBuffer				m_LinesColorBuffer;
	};

	SLinePointsColorBuffer			m_LinesPointsColorBuffer;
	RHI::PRenderState				m_LinesRenderState;
};

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
