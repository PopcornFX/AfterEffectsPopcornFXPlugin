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

#include "ProfilerRenderer.h"

#if PKSAMPLE_HAS_PROFILER_RENDERER

#include "ShaderDefinitions/SampleLibShaderDefinitions.h"

#include <pk_rhi/include/AllInterfaces.h>

#include <pk_rhi/include/interfaces/SApiContext.h>

#define	VERTEX_SHADER_PATH			"./Shaders/Profiler.vert"
#define	FRAGMENT_SHADER_PATH		"./Shaders/Profiler.frag"

#define	VERTEX_RECT_SHADER_PATH		"./Shaders/ProfilerDrawNode.vert"
#define	FRAGMENT_RECT_SHADER_PATH	"./Shaders/ProfilerDrawNode.frag"

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

static float	scalar_to_target_l(float scalar, float target, float timestep, float lambda)
{
	const float	t = 1.0f - expf(-lambda * timestep);
	return scalar + t * (target - scalar);
}

//----------------------------------------------------------------------------

CProfilerRenderer::CProfilerRenderer()
:	m_Enabled(false)
,	m_Pause(false)
#if	defined(PK_DEBUG) || defined(PK_LINUX) || defined(PK_MACOSX)
,	m_CallstacksEnabled(true)
#else
,	m_CallstacksEnabled(false)
#endif
//,	m_TransferWait(null)
//,	m_TransferSignal(null)
,	m_TargetProfilerScale(40000.0f)
,	m_LastProfileFocusPos(0.0f)
,	m_ProfilerVerticalOffset(0.0f)
,	m_DtHistoryPos(0)
,	m_ProfilerReplay(false)
{
	_SetupDefaultDescriptor(m_RenderData.m_DrawDescriptor);
	for (u32 i = 0; i < kDtHistorySize; i++)
		m_DtHistory[i] = 1.0f / 60.0f;
}

//----------------------------------------------------------------------------

CProfilerRenderer::~CProfilerRenderer()
{
	ReleaseRenderInfo();

	if (!m_ProfilerReplay)
	{
		Profiler::CProfiler	*profiler = Profiler::MainEngineProfiler();
		if (profiler != null)
		{
			profiler->GrabCallstacks(false);
			profiler->Activate(false);
			profiler->Reset();
		}
	}
}

//----------------------------------------------------------------------------

void	CProfilerRenderer::ReleaseRenderInfo()
{
	m_RenderInfo.Clear();
}

//----------------------------------------------------------------------------

bool	CProfilerRenderer::_TransferBatchToGpu(const SProfilerRenderData &renderData)
{
	PK_SCOPEDPROFILE();
	if (!PK_VERIFY(m_RenderInfo.m_ApiManager != null))	// Not properly initialized
		return false;

	if (!renderData.m_IsValid) // We can re-render the old profiler geometry in this case
		return true;

	if (m_ProfilerReplay)
	{
		PK_NAMEDSCOPEDPROFILE("CProfilerDrawer::UpdateReplay");

		Profiler::PProfilerReport	frameReport;
		{
			PK_SCOPEDLOCK(m_NewProfilerFramesLock);
			PK_ASSERT(m_NewProfilerFrames.Empty() || m_NewProfilerFrames.Count() == 1);
			if (!m_NewProfilerFrames.Empty())
				frameReport = m_NewProfilerFrames.Last();
		}

		m_ProfilerDrawer.m_ReportToTrash = null;
		m_ProfilerDrawer.m_ProfilerReportHistoryPos = 0;
		m_ProfilerDrawer.m_ProfilerThreadUsageHistoryPos = 0;

		m_ProfilerDrawer.Update(renderData.m_DrawDescriptor, frameReport);
	}
	else
	{
		PK_NAMEDSCOPEDPROFILE("CProfilerDrawer::Update");
		DynamicProfileReportList	framesToPush;
		StaticProfileReportList		framesToTrash;
		{
			PK_SCOPEDLOCK(m_NewProfilerFramesLock);
			framesToPush = m_NewProfilerFrames;
			m_NewProfilerFrames.Clear();
		}
		for (u32 i = 0; i < framesToPush.Count(); ++i)
		{
			m_ProfilerDrawer.Update(renderData.m_DrawDescriptor, framesToPush[i]);
			if (m_ProfilerDrawer.m_ReportToTrash != null && !framesToTrash.Full())
				framesToTrash.PushBack(m_ProfilerDrawer.m_ReportToTrash);
		}
		if (!framesToTrash.Empty())
		{
			PK_SCOPEDLOCK(m_OldProfilerFramesLock);
			for (u32 i = 0; i < framesToTrash.Count() && !m_OldProfilerFrames.Full(); ++i)
				m_OldProfilerFrames.PushBack(framesToTrash[i]);
		}
	}

	{
		PK_NAMEDSCOPEDPROFILE("CProfilerDrawer::DrawGPU");
		m_DrawOutputs.Clear();
		m_ToolTipMsgs.Clear();
		m_ProfilerDrawer.DrawGPU(m_DrawOutputs, renderData.m_DrawDescriptor);
	}

	// Allocate the GpuBuffers IFN:
	m_RenderInfo.m_ElemCountLinesAlphaBlend = m_DrawOutputs.m_LineBatchSolid.m_LineVtxPos.Count();
	m_RenderInfo.m_ElemCountLinesAdditive = m_DrawOutputs.m_LineBatch.m_LineVtxPos.Count();
	m_RenderInfo.m_ElemCountRectsAlphaBlendDashed = m_DrawOutputs.m_RectBatchSolidDashed.m_RectDescs.Count();
	m_RenderInfo.m_ElemCountRectsAlphaBlend = m_DrawOutputs.m_RectBatchSolid.m_RectDescs.Count();
	m_RenderInfo.m_ElemCountRectsAdditive = m_DrawOutputs.m_RectBatch.m_RectDescs.Count();

	u32		linesVertexCount = m_RenderInfo.m_ElemCountLinesAlphaBlend + m_RenderInfo.m_ElemCountLinesAdditive;
	u32		rectsVertexCount = m_RenderInfo.m_ElemCountRectsAlphaBlendDashed + m_RenderInfo.m_ElemCountRectsAlphaBlend + m_RenderInfo.m_ElemCountRectsAdditive;
	u32		dashedVertexCount = m_RenderInfo.m_ElemCountRectsAlphaBlendDashed;

	if (m_RenderInfo.m_RectsIndexBuffer == null)
	{
		PK_NAMEDSCOPEDPROFILE("Alloc rectangles index buffer");
		m_RenderInfo.m_RectsIndexBuffer = m_RenderInfo.m_ApiManager->CreateGpuBuffer(RHI::SRHIResourceInfos("ProfilerRenderer Index Buffer"), RHI::IndexBuffer, 6 * sizeof(u16));
		// Basic index buffer for quads:
		u16		*mappedIdx = (u16*)m_RenderInfo.m_ApiManager->MapCpuView(m_RenderInfo.m_RectsIndexBuffer);

		mappedIdx[0] = 0;
		mappedIdx[1] = 1;
		mappedIdx[2] = 2;
		mappedIdx[3] = 2;
		mappedIdx[4] = 3;
		mappedIdx[5] = 0;

		m_RenderInfo.m_ApiManager->UnmapCpuView(m_RenderInfo.m_RectsIndexBuffer);
	}
	if (m_RenderInfo.m_LinesVertexCapacity < linesVertexCount)
	{
		PK_NAMEDSCOPEDPROFILE("Alloc lines buffer");
		m_RenderInfo.m_LinesVertexCapacity = (linesVertexCount * 2) + 128;
		m_RenderInfo.m_LinesVertexBuffer = m_RenderInfo.m_ApiManager->CreateGpuBuffer(RHI::SRHIResourceInfos("ProfilerRenderer Lines Vertex Buffer"), RHI::VertexBuffer, m_RenderInfo.m_LinesVertexCapacity * sizeof(CFloat2));
		m_RenderInfo.m_LinesColorBuffer = m_RenderInfo.m_ApiManager->CreateGpuBuffer(RHI::SRHIResourceInfos("ProfilerRenderer Lines Color Vertex Buffer"), RHI::VertexBuffer, m_RenderInfo.m_LinesVertexCapacity * sizeof(CFloat4));
		if (m_RenderInfo.m_LinesVertexBuffer == null || m_RenderInfo.m_LinesColorBuffer == null)
			return false;
	}
	if (m_RenderInfo.m_RectsVertexCapacity < rectsVertexCount)
	{
		PK_NAMEDSCOPEDPROFILE("Alloc rectangles buffer");
		m_RenderInfo.m_RectsVertexCapacity = (rectsVertexCount * 2) + 128;
		m_RenderInfo.m_RectsVertexBuffer = m_RenderInfo.m_ApiManager->CreateGpuBuffer(RHI::SRHIResourceInfos("ProfilerRenderer Rects Vertex Buffer"), RHI::VertexBuffer, m_RenderInfo.m_RectsVertexCapacity * sizeof(CFloat4));
		m_RenderInfo.m_RectsColorBuffer0 = m_RenderInfo.m_ApiManager->CreateGpuBuffer(RHI::SRHIResourceInfos("ProfilerRenderer Rects Color0 Vertex Buffer"), RHI::VertexBuffer, m_RenderInfo.m_RectsVertexCapacity * sizeof(CFloat4));
		m_RenderInfo.m_RectsColorBuffer1 = m_RenderInfo.m_ApiManager->CreateGpuBuffer(RHI::SRHIResourceInfos("ProfilerRenderer Rects Color1 Vertex Buffer"), RHI::VertexBuffer, m_RenderInfo.m_RectsVertexCapacity * sizeof(CFloat4));
		if (m_RenderInfo.m_RectsVertexBuffer == null || m_RenderInfo.m_RectsColorBuffer0 == null || m_RenderInfo.m_RectsColorBuffer1 == null)
			return false;
	}
	if (m_RenderInfo.m_DashedRectsVertexCapacity < dashedVertexCount)
	{
		PK_NAMEDSCOPEDPROFILE("Alloc dashed rectangles border color");
		m_RenderInfo.m_DashedRectsVertexCapacity = (dashedVertexCount * 2) + 128;
		m_RenderInfo.m_DashedRectsBorderColor = m_RenderInfo.m_ApiManager->CreateGpuBuffer(RHI::SRHIResourceInfos("ProfilerRenderer Dashed Rects Color Vertex Buffer"), RHI::VertexBuffer, m_RenderInfo.m_DashedRectsVertexCapacity * sizeof(CFloat4));
		if (m_RenderInfo.m_DashedRectsBorderColor == null)
			return false;
	}

	if (linesVertexCount != 0)
	{
		// Transfer the vertex data for lines:
		CFloat2	*linesVertexData = static_cast<CFloat2*>(m_RenderInfo.m_ApiManager->MapCpuView(m_RenderInfo.m_LinesVertexBuffer, 0, linesVertexCount * sizeof(CFloat2)));
		CFloat4	*linesColorData = static_cast<CFloat4*>(m_RenderInfo.m_ApiManager->MapCpuView(m_RenderInfo.m_LinesColorBuffer, 0, linesVertexCount * sizeof(CFloat4)));
		if (linesVertexData == null || linesColorData == null)
			return false;

		Mem::Copy(linesVertexData, m_DrawOutputs.m_LineBatchSolid.m_LineVtxPos.RawDataPointer(), m_RenderInfo.m_ElemCountLinesAlphaBlend * sizeof(CFloat2));
		linesVertexData += m_RenderInfo.m_ElemCountLinesAlphaBlend;
		Mem::Copy(linesVertexData, m_DrawOutputs.m_LineBatch.m_LineVtxPos.RawDataPointer(), m_RenderInfo.m_ElemCountLinesAdditive * sizeof(CFloat2));

		Mem::Copy(linesColorData, m_DrawOutputs.m_LineBatchSolid.m_LineVtxColor.RawDataPointer(), m_RenderInfo.m_ElemCountLinesAlphaBlend * sizeof(CFloat4));
		linesColorData += m_RenderInfo.m_ElemCountLinesAlphaBlend;
		Mem::Copy(linesColorData, m_DrawOutputs.m_LineBatch.m_LineVtxColor.RawDataPointer(), m_RenderInfo.m_ElemCountLinesAdditive * sizeof(CFloat4));

		m_RenderInfo.m_ApiManager->UnmapCpuView(m_RenderInfo.m_LinesVertexBuffer);
		m_RenderInfo.m_ApiManager->UnmapCpuView(m_RenderInfo.m_LinesColorBuffer);
	}
	if (rectsVertexCount != 0)
	{
		// Transfer the vertex data for rects:
		CFloat4	*rectsVertexData = static_cast<CFloat4*>(m_RenderInfo.m_ApiManager->MapCpuView(m_RenderInfo.m_RectsVertexBuffer, 0, rectsVertexCount * sizeof(CFloat4)));
		CFloat4	*rectsColor0Data = static_cast<CFloat4*>(m_RenderInfo.m_ApiManager->MapCpuView(m_RenderInfo.m_RectsColorBuffer0, 0, rectsVertexCount * sizeof(CFloat4)));
		CFloat4	*rectsColor1Data = static_cast<CFloat4*>(m_RenderInfo.m_ApiManager->MapCpuView(m_RenderInfo.m_RectsColorBuffer1, 0, rectsVertexCount * sizeof(CFloat4)));

		if (rectsVertexData == null || rectsColor0Data == null || rectsColor1Data == null)
			return false;

		Mem::Copy(rectsVertexData, m_DrawOutputs.m_RectBatchSolid.m_RectDescs.RawDataPointer(), m_RenderInfo.m_ElemCountRectsAlphaBlend * sizeof(CFloat4));
		rectsVertexData += m_RenderInfo.m_ElemCountRectsAlphaBlend;
		Mem::Copy(rectsVertexData, m_DrawOutputs.m_RectBatchSolidDashed.m_RectDescs.RawDataPointer(), m_RenderInfo.m_ElemCountRectsAlphaBlendDashed * sizeof(CFloat4));
		rectsVertexData += m_RenderInfo.m_ElemCountRectsAlphaBlendDashed;
		Mem::Copy(rectsVertexData, m_DrawOutputs.m_RectBatch.m_RectDescs.RawDataPointer(), m_RenderInfo.m_ElemCountRectsAdditive * sizeof(CFloat4));

		Mem::Copy(rectsColor0Data, m_DrawOutputs.m_RectBatchSolid.m_RectColor0.RawDataPointer(), m_RenderInfo.m_ElemCountRectsAlphaBlend * sizeof(CFloat4));
		rectsColor0Data += m_RenderInfo.m_ElemCountRectsAlphaBlend;
		Mem::Copy(rectsColor0Data, m_DrawOutputs.m_RectBatchSolidDashed.m_DashColor0.RawDataPointer(), m_RenderInfo.m_ElemCountRectsAlphaBlendDashed * sizeof(CFloat4));
		rectsColor0Data += m_RenderInfo.m_ElemCountRectsAlphaBlendDashed;
		Mem::Copy(rectsColor0Data, m_DrawOutputs.m_RectBatch.m_RectColor0.RawDataPointer(), m_RenderInfo.m_ElemCountRectsAdditive * sizeof(CFloat4));

		Mem::Copy(rectsColor1Data, m_DrawOutputs.m_RectBatchSolid.m_RectColor1.RawDataPointer(), m_RenderInfo.m_ElemCountRectsAlphaBlend * sizeof(CFloat4));
		rectsColor1Data += m_RenderInfo.m_ElemCountRectsAlphaBlend;
		Mem::Copy(rectsColor1Data, m_DrawOutputs.m_RectBatchSolidDashed.m_DashColor1.RawDataPointer(), m_RenderInfo.m_ElemCountRectsAlphaBlendDashed * sizeof(CFloat4));
		rectsColor1Data += m_RenderInfo.m_ElemCountRectsAlphaBlendDashed;
		Mem::Copy(rectsColor1Data, m_DrawOutputs.m_RectBatch.m_RectColor1.RawDataPointer(), m_RenderInfo.m_ElemCountRectsAdditive * sizeof(CFloat4));

		m_RenderInfo.m_ApiManager->UnmapCpuView(m_RenderInfo.m_RectsVertexBuffer);
		m_RenderInfo.m_ApiManager->UnmapCpuView(m_RenderInfo.m_RectsColorBuffer0);
		m_RenderInfo.m_ApiManager->UnmapCpuView(m_RenderInfo.m_RectsColorBuffer1);

		if (dashedVertexCount != 0)
		{
			CFloat4	*dashedRectsBorderColor = static_cast<CFloat4*>(m_RenderInfo.m_ApiManager->MapCpuView(m_RenderInfo.m_DashedRectsBorderColor, 0, dashedVertexCount * sizeof(CFloat4)));

			if (dashedRectsBorderColor == null)
				return false;

			Mem::Copy(dashedRectsBorderColor, m_DrawOutputs.m_RectBatchSolidDashed.m_BorderColor.RawDataPointer(), m_RenderInfo.m_ElemCountRectsAlphaBlendDashed * sizeof(CFloat4));

			m_RenderInfo.m_ApiManager->UnmapCpuView(m_RenderInfo.m_DashedRectsBorderColor);
		}
	}
	return true;
}

//----------------------------------------------------------------------------

void	CProfilerRenderer::SRenderInfo::Clear()
{
	m_ApiManager = null;
	m_RectAlphaBlendDashed = null;
	m_RectAlphaBlend = null;
	m_RectAdditive = null;
	m_LineAlphaBlend = null;
	m_LineAdditive = null;
	m_LinesVertexBuffer = null;
	m_LinesColorBuffer = null;
	m_RectsIndexBuffer = null;
	m_RectsVertexBuffer = null;
	m_RectsColorBuffer0 = null;
	m_RectsColorBuffer1 = null;
	m_DashedRectsBorderColor = null;

	m_LinesVertexCapacity = 0;
	m_RectsVertexCapacity = 0;
	m_DashedRectsVertexCapacity = 0;

	m_ElemCountRectsAlphaBlendDashed = 0;
	m_ElemCountRectsAlphaBlend = 0;
	m_ElemCountRectsAdditive = 0;
	m_ElemCountLinesAlphaBlend = 0;
	m_ElemCountLinesAdditive = 0;
}

//----------------------------------------------------------------------------

void	CProfilerRenderer::Setup(const ParticleToolbox::SProfilerDrawerDescriptorNew &descriptor)
{
	const CRect		oldDrawRect		= m_RenderData.m_DrawDescriptor.m_DrawRect;
	const CInt2		oldCursorPos	= m_RenderData.m_DrawDescriptor.m_CursorPos;
	const CInt2		oldCursorExtent	= m_RenderData.m_DrawDescriptor.m_CursorExtent;
	const float		oldPixelRatio	= m_RenderData.m_DrawDescriptor.m_PixelRatio;
	const double	oldScale		= m_RenderData.m_DrawDescriptor.m_ProfilerScale;
	const double	oldOffset		= m_RenderData.m_DrawDescriptor.m_ProfilerOffset;
	const float		oldVOffset		= m_RenderData.m_DrawDescriptor.m_ProfilerVerticalOffset;
	const u32		oldHFrameId		= m_RenderData.m_DrawDescriptor.m_ProfilerHistoryFrameToDisplay;
	const u32		oldHFrameCount	= m_RenderData.m_DrawDescriptor.m_ProfilerHistoryFrameToDisplayCount;

	m_RenderData.m_DrawDescriptor = descriptor;

	m_RenderData.m_DrawDescriptor.m_DrawRect = oldDrawRect;
	m_RenderData.m_DrawDescriptor.m_CursorPos = oldCursorPos;
	m_RenderData.m_DrawDescriptor.m_CursorExtent = oldCursorExtent;
	m_RenderData.m_DrawDescriptor.m_PixelRatio = oldPixelRatio;
	m_RenderData.m_DrawDescriptor.m_ProfilerScale = oldScale;
	m_RenderData.m_DrawDescriptor.m_ProfilerOffset = oldOffset;
	m_RenderData.m_DrawDescriptor.m_ProfilerVerticalOffset = oldVOffset;
	m_RenderData.m_DrawDescriptor.m_ProfilerHistoryFrameToDisplay = oldHFrameId;
	m_RenderData.m_DrawDescriptor.m_ProfilerHistoryFrameToDisplayCount = oldHFrameCount;

	if (!m_RenderData.m_DrawDescriptor.m_FontDraw)
		m_RenderData.m_DrawDescriptor.m_FontDraw = FastDelegate<void(ParticleToolbox::EFontType, const CFloat3 &, const TMemoryView<const char> &, const CFloat4 &)>(this, &CProfilerRenderer::_FontDraw);
	if (!m_RenderData.m_DrawDescriptor.m_FontLength)
		m_RenderData.m_DrawDescriptor.m_FontLength = FastDelegate<float(ParticleToolbox::EFontType, const TMemoryView<const char> &)>(this, &CProfilerRenderer::_FontLength);
	if (!m_RenderData.m_DrawDescriptor.m_FontInfo)
		m_RenderData.m_DrawDescriptor.m_FontInfo = FastDelegate<void(ParticleToolbox::EFontType, ParticleToolbox::SFontDesc &)>(this, &CProfilerRenderer::_FontInfo);
	if (!m_RenderData.m_DrawDescriptor.m_ToolTip_AddString)
		m_RenderData.m_DrawDescriptor.m_ToolTip_AddString = FastDelegate<void(const CString &string)>(this, &CProfilerRenderer::_ToolTip_AddString);
	if (!m_RenderData.m_DrawDescriptor.m_ToolTip_Rect)
		m_RenderData.m_DrawDescriptor.m_ToolTip_Rect = FastDelegate<void(const CInt2 &where, CIRect &outRect)>(this, &CProfilerRenderer::_ToolTip_Rect);
	if (!m_RenderData.m_DrawDescriptor.m_ToolTip_Draw)
		m_RenderData.m_DrawDescriptor.m_ToolTip_Draw = FastDelegate<void(const CInt2 &where, float z)>(this, &CProfilerRenderer::_ToolTip_Draw);
}

//----------------------------------------------------------------------------

void	CProfilerRenderer::_SetupDefaultDescriptor(ParticleToolbox::SProfilerDrawerDescriptorNew &descriptor)
{
	descriptor.m_PauseProfilerCapture = false;
	descriptor.m_ShowTooltipOnlyWhenCapturePaused = true;
	descriptor.m_MemoryView = false;
	descriptor.m_MemoryViewIntensity = 1.0f;
	descriptor.m_MemoryViewMaxMem = 1.2f;	// Max mem intensity happens @ 1.2 Mb
	descriptor.m_VisibleFraction = 1.5f;
	descriptor.m_HightlightFilter = null;
	descriptor.m_Profiler_TopDown = false;
	descriptor.m_Profiler_HMarginsInPixels = CInt2(4, 4);
	descriptor.m_Profiler_StartX = 0.0f;
	descriptor.m_Profiler_StartY = 12.0f;
	descriptor.m_Profiler_LineHeight = 8.0f;
	descriptor.m_Profiler_LineSpacing = 0.0f;
	descriptor.m_Profiler_SatteliteThreadLineHeight = 12.0f;
	descriptor.m_Profiler_SatteliteThreadLineSpacing = 0.0f;
	descriptor.m_Profiler_LineThreadSpacing = 3.0f;
	descriptor.m_Profiler_LineBottomColorCoeff = 1.0f;//0.8f;
	descriptor.m_Profiler_HitNodeBrightness = 2.0f;
	descriptor.m_Profiler_SimilarHitNodesBrightness = 1.5f;
	descriptor.m_Profiler_OddLinesBrightness = 0.8f;
	descriptor.m_Profiler_HScale = 25000.0f;
	descriptor.m_Profiler_CondenseAllSatteliteThreads = false;
	descriptor.m_Profiler_HideUnusedCallLevels = true;
	descriptor.m_Profiler_HLine_Thread = CFloat4(0.7f, 1.0f, 0.7f, 1.0f);
	descriptor.m_Profiler_VLine_Time = CFloat4(0.05f, 0.05f, 0.05f, 1.0f);
	descriptor.m_Profiler_VLine_Frame = CFloat4(1, 0, 0, 1.0f);
	descriptor.m_Profiler_VLine_FrameFramerate = 60.0f;	// 60 fps (~16 ms)
	descriptor.m_Profiler_NoMemColor = CFloat4(0.05f, 0.05f, 0.05f, 0.5f);
	descriptor.m_Profiler_RejectedBarsColor = CFloat4(0.1f, 0.1f, 0.1f, 1.0f);
	descriptor.m_Profiler_FilteredBarsColor = CFloat4(1.0f, 0.1f, 0.1f, 1.0f);
	descriptor.m_Profiler_IdleNode_Color0 = CFloat4(0.8f, 0, 0, 0.3f);
	descriptor.m_Profiler_IdleNode_Color1 = CFloat4(0.01f, 0.01f, 0.01f, 0.8f);
	descriptor.m_Profiler_ScaleFont = ParticleToolbox::Font_Medium;
	descriptor.m_Profiler_ThreadInfoFont = ParticleToolbox::Font_Small;
//	descriptor.m_Profiler_ShowNodeNames = true;
	descriptor.m_Profiler_ShowNodeNames = false;
	descriptor.m_Profiler_NodeNameFont = ParticleToolbox::Font_Medium;
	descriptor.m_FrameCacheSize = 5;
	descriptor.m_FrameCachePersistence = 6.0f;
	descriptor.m_HistorySizeInFrames = 64;
	descriptor.m_HistoryGraph_Start = CFloat2(180, 5);
//	descriptor.m_HistoryGraph_Start = CFloat2(0, 0);
	descriptor.m_HistoryGraph_Size = CFloat2(600, 50);
	descriptor.m_HistoryGraph_ShowCPU = true;
	descriptor.m_HistoryGraph_ShowMem = true;
//	descriptor.m_CPUGraph_Show = true;
	descriptor.m_CPUGraph_Show = false;
	descriptor.m_CPUGraph_Height = 16.0f;
	descriptor.m_TaskGroups_Show = false;
	descriptor.m_TaskGroups_IntensityBright = 0.4f;
	descriptor.m_TaskGroups_IntensityDim = 0.2f;
	descriptor.m_PixelRatio = 1.0;
	descriptor.m_ProfilerScale = 40000.0;
	descriptor.m_ProfilerOffset = 0.0;
	descriptor.m_ProfilerVerticalOffset = 0.0f;
	descriptor.m_ProfilerHistoryFrameToDisplay = 0;

	descriptor.m_FontDraw = FastDelegate<void(ParticleToolbox::EFontType, const CFloat3 &, const TMemoryView<const char> &, const CFloat4 &)>(this, &CProfilerRenderer::_FontDraw);
	descriptor.m_FontLength = FastDelegate<float(ParticleToolbox::EFontType, const TMemoryView<const char> &)>(this, &CProfilerRenderer::_FontLength);
	descriptor.m_FontInfo = FastDelegate<void(ParticleToolbox::EFontType, ParticleToolbox::SFontDesc &)>(this, &CProfilerRenderer::_FontInfo);
	descriptor.m_ToolTip_AddString = FastDelegate<void(const CString &string)>(this, &CProfilerRenderer::_ToolTip_AddString);
	descriptor.m_ToolTip_Rect = FastDelegate<void(const CInt2 &where, CIRect &outRect)>(this, &CProfilerRenderer::_ToolTip_Rect);
	descriptor.m_ToolTip_Draw = FastDelegate<void(const CInt2 &where, float z)>(this, &CProfilerRenderer::_ToolTip_Draw);
}

//----------------------------------------------------------------------------

bool	CProfilerRenderer::EnableWithProfileReport(const Profiler::PProfilerReport &profileReport)
{
	PK_SCOPEDPROFILE();
	m_Enabled = (profileReport != null);
	m_Pause = true;
	m_ProfilerReplay = true;

	{
		PK_SCOPEDLOCK(m_NewProfilerFramesLock);
		m_NewProfilerFrames.Clear();

		if (profileReport != null)
		{
			if (!PK_VERIFY(m_NewProfilerFrames.PushBack(profileReport).Valid()))
				return false;
		}
	}

	{
		PK_SCOPEDLOCK(m_OldProfilerFramesLock);
		m_OldProfilerFrames.Clear();
	}

	SetCurrentHistoryFrame(0);
	return true;
}

//----------------------------------------------------------------------------

bool	CProfilerRenderer::Enable(bool enable)
{
	if (m_ProfilerReplay)
	{
		m_Enabled = enable;
		return m_Enabled;
	}

	m_Enabled = enable;
	m_Pause = false;

	Profiler::CProfiler	*profiler = Profiler::MainEngineProfiler();
	if (profiler != null)
	{
		profiler->Reset();
	}

	SetCurrentHistoryFrame(0);

	return m_Enabled;
}

//----------------------------------------------------------------------------

Profiler::PProfilerReport	CProfilerRenderer::CurrentRecord() const
{
	PK_SCOPEDLOCK(m_NewProfilerFramesLock);
	if (!m_NewProfilerFrames.Empty())
	{
//		const u32	frameId = PKMin(m_NewProfilerFrames.Count() - 1, m_RenderData.m_DrawDescriptor.m_ProfilerHistoryFrameToDisplay);
		return m_NewProfilerFrames.Last();
	}
	return null;
}

//----------------------------------------------------------------------------

void	CProfilerRenderer::SetCurrentHistoryFrame(s32 frameIndex)
{
	frameIndex = frameIndex < 0 ? frameIndex + m_RenderData.m_DrawDescriptor.m_HistorySizeInFrames : frameIndex;
	m_RenderData.m_DrawDescriptor.m_ProfilerHistoryFrameToDisplay = frameIndex % m_RenderData.m_DrawDescriptor.m_HistorySizeInFrames;
}

//----------------------------------------------------------------------------

void	CProfilerRenderer::ViewVerticalScroll(float scrollXInPixel)
{
	m_RenderData.m_DrawDescriptor.m_ProfilerVerticalOffset += scrollXInPixel;
}

//----------------------------------------------------------------------------

void	CProfilerRenderer::ViewHScroll(float scrollXInPixel)
{
	m_RenderData.m_DrawDescriptor.m_ProfilerOffset += scrollXInPixel / m_RenderData.m_DrawDescriptor.m_ProfilerScale;
}

//----------------------------------------------------------------------------

void	CProfilerRenderer::ViewHZoom(float zoomCenterXInPixel, float zoomFactor)
{
	const double	oldscale = m_RenderData.m_DrawDescriptor.m_ProfilerScale;
	const double	newscale = oldscale * powf(2.0f, zoomFactor);	// *1.41, /1.41<->*0.707
	const double	xoff = (zoomCenterXInPixel / oldscale) - (zoomCenterXInPixel / newscale);
	m_RenderData.m_DrawDescriptor.m_ProfilerScale = newscale;
	m_RenderData.m_DrawDescriptor.m_ProfilerOffset += xoff;
}

//----------------------------------------------------------------------------

void	CProfilerRenderer::ProfilerNextFrameTick()
{
	if (m_ProfilerReplay)
		return;

	Profiler::CProfiler	*profiler = Profiler::MainEngineProfiler();
	if (profiler == null)
		return;

	if (!m_Enabled)
	{
		profiler->GrabCallstacks(false);
		profiler->Activate(false);
		profiler->Reset();
		return;
	}

	{
		PK_NAMEDSCOPEDPROFILE("ProfilerNextFrameTick");

#if	(KR_PROFILER_ENABLED != 0)
		// Optim: lock here once, instead of locking multiple times in function calls below
		profiler->m_RecordsLock.Lock();
#endif

		if (!m_Pause)
		{
			Profiler::PProfilerReport	profilerReport = null;

			{
				PK_NAMEDSCOPEDPROFILE("get profiler report to fill");
				{
					PK_SCOPEDLOCK(m_OldProfilerFramesLock);
					if (!m_OldProfilerFrames.Empty())
						profilerReport = m_OldProfilerFrames.PopBack();
				}
				if (profilerReport == null)
				{
					profilerReport = PK_NEW(Profiler::CProfilerReport);
					if (profilerReport == null)
					{
						CLog::Log(PK_INFO, "Could not allocate profiler report");
						return;
					}
				}
			}
			{
				PK_NAMEDSCOPEDPROFILE("profiler build report");
				profiler->BuildReport(profilerReport.Get());
			}
			{
				PK_NAMEDSCOPEDPROFILE("push back profiler report");
				PK_SCOPEDLOCK(m_NewProfilerFramesLock);
				m_NewProfilerFrames.PushBack(profilerReport);
			}
		}
		profiler->GrabCallstacks(m_CallstacksEnabled);
		profiler->Activate(true);
	}

	profiler->Reset();

#if	(KR_PROFILER_ENABLED != 0)
	// Optim: lock here once, instead of locking multiple times in function calls below
	profiler->m_RecordsLock.Unlock();
#endif
}

//----------------------------------------------------------------------------

void	CProfilerRenderer::UpdateDrawGeometry(const CRect &drawRect, const CInt2 &cursorPos, float pixelRatio)
{
	m_RenderData.m_DrawDescriptor.m_PixelRatio = pixelRatio;
	m_RenderData.m_DrawDescriptor.m_PauseProfilerCapture = m_Pause;
	m_RenderData.m_DrawDescriptor.m_DrawRect = drawRect;
	m_RenderData.m_DrawDescriptor.m_CursorPos = cursorPos;
	m_RenderData.m_DrawDescriptor.m_CursorExtent = CInt2(10, 20);
}

//----------------------------------------------------------------------------

void	CProfilerRenderer::GenerateProfilerRenderData(double dt, const CRect &drawRect, const CInt2 &cursorPos, float pixelRatio)
{
	UpdateDrawGeometry(drawRect, cursorPos, pixelRatio);

	double				scaleSpeed = 2.0;
	//const double		dt = m_Dt;

	m_DtHistory[m_DtHistoryPos] = dt;
	m_DtHistoryPos = (m_DtHistoryPos + 1) % kDtHistorySize;

	float	avgDt = 0.0f;
	for (u32 i = 0; i < kDtHistorySize; i++)
		avgDt += m_DtHistory[i];
	avgDt /= kDtHistorySize;

	bool			needsAutozoom = false;
	if (needsAutozoom)
	{
		const CFloat2	viewportSize(m_RenderData.m_DrawDescriptor.m_DrawRect.Extent().x(), m_RenderData.m_DrawDescriptor.m_DrawRect.Extent().y());	// viewport dimensions in pixels
		const float		filteredDt = avgDt;
		const float		hScale = viewportSize.x();
		const double	idealScale = hScale / PKMax(filteredDt * m_RenderData.m_DrawDescriptor.m_VisibleFraction, 1.0e-3f);	// 5.0e+4f;	// don't autozoom below the millisecond
		const double	idealOffset = 2.0 / idealScale;

		m_RenderData.m_DrawDescriptor.m_ProfilerOffset = scalar_to_target_l(m_RenderData.m_DrawDescriptor.m_ProfilerOffset, double(idealOffset), double(dt), 2.0);

		m_TargetProfilerScale = idealScale;
		m_ProfilerVerticalOffset = 0.0f;
		m_LastProfileFocusPos = m_RenderData.m_DrawDescriptor.m_ProfilerOffset;

		scaleSpeed = 0.8;

		const double	newProfilerScale = scalar_to_target_l(m_RenderData.m_DrawDescriptor.m_ProfilerScale, m_TargetProfilerScale, dt, scaleSpeed);
		const float		newProfilerVOffset = scalar_to_target_l(m_RenderData.m_DrawDescriptor.m_ProfilerVerticalOffset, m_ProfilerVerticalOffset, dt, float(scaleSpeed));
		// set the profiler start draw position so that time 'm_LastProfileFocusPos' is invariant during the zoom:
		const double	focusPosInPixels = (m_LastProfileFocusPos - m_RenderData.m_DrawDescriptor.m_ProfilerOffset) * m_RenderData.m_DrawDescriptor.m_ProfilerScale;
		const double	newFocusPosInPixels = PKMax(-0.001, m_LastProfileFocusPos - (focusPosInPixels / newProfilerScale));	// allow -1 ms offset

		m_RenderData.m_DrawDescriptor.m_ProfilerScale = newProfilerScale;
		m_RenderData.m_DrawDescriptor.m_ProfilerOffset = newFocusPosInPixels;
		m_RenderData.m_DrawDescriptor.m_ProfilerVerticalOffset = newProfilerVOffset;
	}
}

//----------------------------------------------------------------------------

void	CProfilerRenderer::LockAndCopyProfilerData(SProfilerRenderData &renderData)
{
	PK_SCOPEDPROFILE();
	if (!m_Enabled)
		return;
	renderData.m_DrawDescriptor = m_RenderData.m_DrawDescriptor;
	renderData.m_IsValid = true;
}

//----------------------------------------------------------------------------

bool	CProfilerRenderer::CreateRenderInfoIFN(const RHI::PApiManager &apiManager, CShaderLoader &loader, TMemoryView<const RHI::SRenderTargetDesc> famebufferLayout, RHI::PRenderPass renderPass, u32 subPass)
{
	if (m_RenderInfo.m_ApiManager != null)
		return true;
	return CreateRenderInfo(apiManager, loader, famebufferLayout, renderPass, subPass);
}

//----------------------------------------------------------------------------

bool	CProfilerRenderer::CreateRenderInfo(const RHI::PApiManager &apiManager, CShaderLoader &loader, TMemoryView<const RHI::SRenderTargetDesc> famebufferLayout, RHI::PRenderPass renderPass, u32 subPass)
{
	RHI::SRenderState		linesRenderStateData;
	RHI::SRenderState		rectsRenderStateData;
	RHI::SRenderState		rectsDashedRenderStateData;

	// Common between rects and lines:
	m_RenderInfo.m_ApiManager = apiManager;
	m_RenderInfo.m_RectAlphaBlendDashed = apiManager->CreateRenderState(RHI::SRHIResourceInfos("ProfilerRenderer Rects AlphaBlendDashed Render State"));
	m_RenderInfo.m_RectAlphaBlend = apiManager->CreateRenderState(RHI::SRHIResourceInfos("ProfilerRenderer Rects AlphaBlend Render State"));
	m_RenderInfo.m_RectAdditive = apiManager->CreateRenderState(RHI::SRHIResourceInfos("ProfilerRenderer Rects Additive Render State"));
	m_RenderInfo.m_LineAlphaBlend = apiManager->CreateRenderState(RHI::SRHIResourceInfos("ProfilerRenderer Lines AlphaBlend Render State"));
	m_RenderInfo.m_LineAdditive = apiManager->CreateRenderState(RHI::SRHIResourceInfos("ProfilerRenderer Lines Additive Render State"));
	if (m_RenderInfo.m_RectAlphaBlendDashed == null ||
		m_RenderInfo.m_RectAlphaBlend == null || m_RenderInfo.m_RectAdditive == null ||
		m_RenderInfo.m_LineAlphaBlend == null || m_RenderInfo.m_LineAdditive == null)
		return false;

	linesRenderStateData.m_PipelineState.m_DynamicScissor = true;
	linesRenderStateData.m_PipelineState.m_DynamicViewport = true;
	linesRenderStateData.m_PipelineState.m_Blending = true;

	// Copy render state data:
	rectsRenderStateData = linesRenderStateData;
	rectsDashedRenderStateData = linesRenderStateData;

	// ------------------------
	// Add line input buffers:
	if (!linesRenderStateData.m_InputVertexBuffers.Resize(2))
		return false;
	linesRenderStateData.m_InputVertexBuffers[0].m_Stride = sizeof(CFloat2);
	linesRenderStateData.m_InputVertexBuffers[1].m_Stride = sizeof(CFloat4);

	// Create shader bindings:
	FillProfilerShaderBindings(linesRenderStateData.m_ShaderBindings);
	linesRenderStateData.m_ShaderBindings.m_InputAttributes[0].m_BufferIdx = 0;
	linesRenderStateData.m_ShaderBindings.m_InputAttributes[1].m_BufferIdx = 1;

	// ------------------------
	// Add dashed rect input buffers:
	if (!rectsDashedRenderStateData.m_InputVertexBuffers.Resize(4))
		return false;
	rectsDashedRenderStateData.m_InputVertexBuffers[0].m_Stride = sizeof(CFloat4);
	rectsDashedRenderStateData.m_InputVertexBuffers[0].m_InputRate = RHI::PerInstanceInput;
	rectsDashedRenderStateData.m_InputVertexBuffers[1].m_Stride = sizeof(CFloat4);
	rectsDashedRenderStateData.m_InputVertexBuffers[1].m_InputRate = RHI::PerInstanceInput;
	rectsDashedRenderStateData.m_InputVertexBuffers[2].m_Stride = sizeof(CFloat4);
	rectsDashedRenderStateData.m_InputVertexBuffers[2].m_InputRate = RHI::PerInstanceInput;
	rectsDashedRenderStateData.m_InputVertexBuffers[3].m_Stride = sizeof(CFloat4);
	rectsDashedRenderStateData.m_InputVertexBuffers[3].m_InputRate = RHI::PerInstanceInput;

	// Create shader bindings:
	FillProfilerDrawNodeShaderBindings(rectsDashedRenderStateData.m_ShaderBindings, true);
	rectsDashedRenderStateData.m_ShaderBindings.m_InputAttributes[0].m_BufferIdx = 0;
	rectsDashedRenderStateData.m_ShaderBindings.m_InputAttributes[1].m_BufferIdx = 1;
	rectsDashedRenderStateData.m_ShaderBindings.m_InputAttributes[2].m_BufferIdx = 2;
	rectsDashedRenderStateData.m_ShaderBindings.m_InputAttributes[3].m_BufferIdx = 3;

	// ------------------------
	// Add rect input buffers:
	if (!rectsRenderStateData.m_InputVertexBuffers.Resize(3))
		return false;
	rectsRenderStateData.m_InputVertexBuffers[0].m_Stride = sizeof(CFloat4);
	rectsRenderStateData.m_InputVertexBuffers[0].m_InputRate = RHI::PerInstanceInput;
	rectsRenderStateData.m_InputVertexBuffers[1].m_Stride = sizeof(CFloat4);
	rectsRenderStateData.m_InputVertexBuffers[1].m_InputRate = RHI::PerInstanceInput;
	rectsRenderStateData.m_InputVertexBuffers[2].m_Stride = sizeof(CFloat4);
	rectsRenderStateData.m_InputVertexBuffers[2].m_InputRate = RHI::PerInstanceInput;

	// Create shader bindings:
	FillProfilerDrawNodeShaderBindings(rectsRenderStateData.m_ShaderBindings, false);
	rectsRenderStateData.m_ShaderBindings.m_InputAttributes[0].m_BufferIdx = 0;
	rectsRenderStateData.m_ShaderBindings.m_InputAttributes[1].m_BufferIdx = 1;
	rectsRenderStateData.m_ShaderBindings.m_InputAttributes[2].m_BufferIdx = 2;

	// load basic profiler shader:
	CShaderLoader::SShadersPaths linesShadersPaths;
	linesShadersPaths.m_Fragment = FRAGMENT_SHADER_PATH;
	linesShadersPaths.m_Vertex = VERTEX_SHADER_PATH;
	if (!loader.LoadShader(linesRenderStateData, linesShadersPaths, apiManager))
		return false;

	// load profiler shader for rectangles:
	CShaderLoader::SShadersPaths rectsShadersPaths;
	rectsShadersPaths.m_Fragment = FRAGMENT_RECT_SHADER_PATH;
	rectsShadersPaths.m_Vertex = VERTEX_RECT_SHADER_PATH;
	if (!loader.LoadShader(rectsRenderStateData, rectsShadersPaths, apiManager))
		return false;

	// load profiler shader for dashed rectangles:
	CShaderLoader::SShadersPaths rectsDashedShadersPaths;
	rectsDashedShadersPaths.m_Fragment = FRAGMENT_RECT_SHADER_PATH;
	rectsDashedShadersPaths.m_Vertex = VERTEX_RECT_SHADER_PATH;
	if (!loader.LoadShader(rectsDashedRenderStateData, rectsDashedShadersPaths, apiManager))
		return false;

	m_RenderInfo.m_RectAlphaBlendDashed->m_RenderState = rectsDashedRenderStateData;
	m_RenderInfo.m_RectAlphaBlend->m_RenderState = rectsRenderStateData;
	m_RenderInfo.m_RectAdditive->m_RenderState = rectsRenderStateData;

	m_RenderInfo.m_LineAlphaBlend->m_RenderState = linesRenderStateData;
	m_RenderInfo.m_LineAdditive->m_RenderState = linesRenderStateData;

	m_RenderInfo.m_RectAlphaBlendDashed->m_RenderState.m_PipelineState.m_ColorBlendingEquation = RHI::BlendAdd;
	m_RenderInfo.m_RectAlphaBlendDashed->m_RenderState.m_PipelineState.m_ColorBlendingSrc = RHI::BlendSrcAlpha;
	m_RenderInfo.m_RectAlphaBlendDashed->m_RenderState.m_PipelineState.m_ColorBlendingDst = RHI::BlendOneMinusSrcAlpha;

	m_RenderInfo.m_RectAlphaBlend->m_RenderState.m_PipelineState.m_ColorBlendingEquation = RHI::BlendAdd;
	m_RenderInfo.m_RectAlphaBlend->m_RenderState.m_PipelineState.m_ColorBlendingSrc = RHI::BlendSrcAlpha;
	m_RenderInfo.m_RectAlphaBlend->m_RenderState.m_PipelineState.m_ColorBlendingDst = RHI::BlendOneMinusSrcAlpha;

	m_RenderInfo.m_RectAdditive->m_RenderState.m_PipelineState.m_ColorBlendingEquation = RHI::BlendAdd;
	m_RenderInfo.m_RectAdditive->m_RenderState.m_PipelineState.m_ColorBlendingSrc = RHI::BlendOne;
	m_RenderInfo.m_RectAdditive->m_RenderState.m_PipelineState.m_ColorBlendingDst = RHI::BlendOne;

	m_RenderInfo.m_LineAlphaBlend->m_RenderState.m_PipelineState.m_ColorBlendingEquation = RHI::BlendAdd;
	m_RenderInfo.m_LineAlphaBlend->m_RenderState.m_PipelineState.m_ColorBlendingSrc = RHI::BlendSrcAlpha;
	m_RenderInfo.m_LineAlphaBlend->m_RenderState.m_PipelineState.m_ColorBlendingDst = RHI::BlendOneMinusSrcAlpha;
	m_RenderInfo.m_LineAlphaBlend->m_RenderState.m_PipelineState.m_DrawMode = RHI::DrawModeLine;

	m_RenderInfo.m_LineAdditive->m_RenderState.m_PipelineState.m_ColorBlendingEquation = RHI::BlendAdd;
	m_RenderInfo.m_LineAdditive->m_RenderState.m_PipelineState.m_ColorBlendingSrc = RHI::BlendOne;
	m_RenderInfo.m_LineAdditive->m_RenderState.m_PipelineState.m_ColorBlendingDst = RHI::BlendOne;
	m_RenderInfo.m_LineAdditive->m_RenderState.m_PipelineState.m_DrawMode = RHI::DrawModeLine;

	if (!apiManager->BakeRenderState(m_RenderInfo.m_RectAlphaBlendDashed, famebufferLayout, renderPass, subPass) ||
		!apiManager->BakeRenderState(m_RenderInfo.m_RectAlphaBlend, famebufferLayout, renderPass, subPass) ||
		!apiManager->BakeRenderState(m_RenderInfo.m_RectAdditive, famebufferLayout, renderPass, subPass) ||
		!apiManager->BakeRenderState(m_RenderInfo.m_LineAlphaBlend, famebufferLayout, renderPass, subPass) ||
		!apiManager->BakeRenderState(m_RenderInfo.m_LineAdditive, famebufferLayout, renderPass, subPass))
		return false;
	return true;
}

//----------------------------------------------------------------------------

const CProfilerRenderer::SProfilerRenderData	&CProfilerRenderer::UnsafeGetProfilerData() const
{
	return m_RenderData;
}

//----------------------------------------------------------------------------

bool	CProfilerRenderer::Render(const RHI::PCommandBuffer &commandBuff, const CUint2 &contextSize, const SProfilerRenderData &profilerData)
{
	if (!m_Enabled)
		return true;

	if (!_TransferBatchToGpu(profilerData))
	{
		return false;
	}

	const float		le = 0.0f;
	const float		ri = contextSize.x();
	const float		bo = 0.0f;
	const float		to = contextSize.y();
	const float		nearVal = -10.0f;
	const float		farVal = 10.0f;

	CFloat4x4		proj(	2.0f / (ri - le), 0.0f, 0.0f, -(ri + le) / (ri - le),
							0.0f, 2.0f / (to - bo), 0.0f, -(to + bo) / (to - bo),
							0.0f, 0.0f, -2.0f / (farVal - nearVal), -(farVal + nearVal) / (farVal - nearVal),
							0.0f, 0.0f, 0.0f, 1.0f);
	proj.Transpose();

	commandBuff->SetViewport(CInt2(0), contextSize, CFloat2(0, 1));
	commandBuff->SetScissor(CInt2(0), contextSize);

	// Draw the lines first:
	u32		curLineVtxOffset = 0;
	if (m_RenderInfo.m_ElemCountLinesAlphaBlend != 0)
	{
		commandBuff->BindRenderState(m_RenderInfo.m_LineAlphaBlend);
		commandBuff->PushConstant(&proj, 0);

		RHI::PGpuBuffer		buffers[2] = { m_RenderInfo.m_LinesVertexBuffer, m_RenderInfo.m_LinesColorBuffer };
		commandBuff->BindVertexBuffers(TMemoryView<RHI::PGpuBuffer>(buffers));

		commandBuff->Draw(curLineVtxOffset, m_RenderInfo.m_ElemCountLinesAlphaBlend);
		curLineVtxOffset += m_RenderInfo.m_ElemCountLinesAlphaBlend;
	}
	if (m_RenderInfo.m_ElemCountLinesAdditive != 0)
	{
		commandBuff->BindRenderState(m_RenderInfo.m_LineAdditive);
		commandBuff->PushConstant(&proj, 0);

		RHI::PGpuBuffer		buffers[2] = { m_RenderInfo.m_LinesVertexBuffer, m_RenderInfo.m_LinesColorBuffer };
		commandBuff->BindVertexBuffers(TMemoryView<RHI::PGpuBuffer>(buffers));

		commandBuff->Draw(curLineVtxOffset, m_RenderInfo.m_ElemCountLinesAdditive);
		curLineVtxOffset += m_RenderInfo.m_ElemCountLinesAdditive;
	}
	// Then draw the rects:
	u32		curRectVtxOffset = 0;
	if (m_RenderInfo.m_ElemCountRectsAlphaBlend != 0)
	{
		commandBuff->BindRenderState(m_RenderInfo.m_RectAlphaBlend);
		commandBuff->PushConstant(&proj, 0);

		RHI::PGpuBuffer		buffers[3] = { m_RenderInfo.m_RectsVertexBuffer, m_RenderInfo.m_RectsColorBuffer0, m_RenderInfo.m_RectsColorBuffer1 };
		u32					offsets[3] =
		{
			static_cast<u32>(curRectVtxOffset * sizeof(CFloat4)),
			static_cast<u32>(curRectVtxOffset * sizeof(CFloat4)),
			static_cast<u32>(curRectVtxOffset * sizeof(CFloat4))
		};
		commandBuff->BindIndexBuffer(m_RenderInfo.m_RectsIndexBuffer, 0, RHI::IndexBuffer16Bit);
		commandBuff->BindVertexBuffers(buffers, offsets);

		commandBuff->DrawIndexedInstanced(0, 0, 6, m_RenderInfo.m_ElemCountRectsAlphaBlend);
		curRectVtxOffset += m_RenderInfo.m_ElemCountRectsAlphaBlend;
	}
	if (m_RenderInfo.m_ElemCountRectsAlphaBlendDashed != 0)
	{
		commandBuff->BindRenderState(m_RenderInfo.m_RectAlphaBlendDashed);
		commandBuff->PushConstant(&proj, 0);

		RHI::PGpuBuffer		buffers[4] = { m_RenderInfo.m_RectsVertexBuffer, m_RenderInfo.m_RectsColorBuffer0, m_RenderInfo.m_RectsColorBuffer1, m_RenderInfo.m_DashedRectsBorderColor };
		u32					offsets[4] =
		{
			static_cast<u32>(curRectVtxOffset * sizeof(CFloat4)),
			static_cast<u32>(curRectVtxOffset * sizeof(CFloat4)),
			static_cast<u32>(curRectVtxOffset * sizeof(CFloat4)),
			0
		};
		commandBuff->BindIndexBuffer(m_RenderInfo.m_RectsIndexBuffer, 0, RHI::IndexBuffer16Bit);
		commandBuff->BindVertexBuffers(buffers, offsets);

		commandBuff->DrawIndexedInstanced(0, 0, 6, m_RenderInfo.m_ElemCountRectsAlphaBlendDashed);
		curRectVtxOffset += m_RenderInfo.m_ElemCountRectsAlphaBlendDashed;
	}
	if (m_RenderInfo.m_ElemCountRectsAdditive != 0)
	{
		commandBuff->BindRenderState(m_RenderInfo.m_RectAdditive);
		commandBuff->PushConstant(&proj, 0);

		RHI::PGpuBuffer		buffers[3] = { m_RenderInfo.m_RectsVertexBuffer, m_RenderInfo.m_RectsColorBuffer0, m_RenderInfo.m_RectsColorBuffer1 };
		u32					offsets[3] =
		{
			static_cast<u32>(curRectVtxOffset * sizeof(CFloat4)),
			static_cast<u32>(curRectVtxOffset * sizeof(CFloat4)),
			static_cast<u32>(curRectVtxOffset * sizeof(CFloat4))
		};
		commandBuff->BindIndexBuffer(m_RenderInfo.m_RectsIndexBuffer, 0, RHI::IndexBuffer16Bit);
		commandBuff->BindVertexBuffers(buffers, offsets);

		commandBuff->DrawIndexedInstanced(0, 0, 6, m_RenderInfo.m_ElemCountRectsAdditive);
		curRectVtxOffset += m_RenderInfo.m_ElemCountRectsAdditive;
	}
	return true;
}

//----------------------------------------------------------------------------

void	CProfilerRenderer::SetPaused(bool pause)
{
	if (!m_ProfilerReplay)
		m_Pause = pause;
}

//----------------------------------------------------------------------------

void	CProfilerRenderer::Zoom(float zoom, const CUint2 &windowSize)
{
	const float	zoomStep = 0.5f;
	ViewHZoom((windowSize.x() * 0.5f), zoom  * zoomStep);
}

//----------------------------------------------------------------------------

void	CProfilerRenderer::Offset(float offset)
{
	const float	offsetStep = 100.0f;
	ViewHScroll(offset * offsetStep);
}

//----------------------------------------------------------------------------

void	CProfilerRenderer::VerticalOffset(float vOffset)
{
	const float	offsetStep = 1.0f;
	ViewVerticalScroll(vOffset * offsetStep);
}

//----------------------------------------------------------------------------

void	CProfilerRenderer::_FontDraw(ParticleToolbox::EFontType font, const CFloat3 &position, const TMemoryView<const char> &textView, const CFloat4 &color) const
{
	(void)font; (void)position; (void)textView; (void)color;
}

//----------------------------------------------------------------------------

float	CProfilerRenderer::_FontLength(ParticleToolbox::EFontType font, const TMemoryView<const char> &textView) const
{
	(void)font; (void)textView;
	return 1.0f;
}

//----------------------------------------------------------------------------

void	CProfilerRenderer::_FontInfo(ParticleToolbox::EFontType font, ParticleToolbox::SFontDesc &outDesc) const
{
	(void)font;
	outDesc.m_Ascender = 1.0f;
	outDesc.m_Descender = 1.0f;
	outDesc.m_Height = 1.0f;
}

//----------------------------------------------------------------------------

void	CProfilerRenderer::_ToolTip_AddString(const CString &string)
{
	if (string.Empty())
		m_ToolTipMsgs.PushBack("");
	else
		m_ToolTipMsgs.PushBack(string);
}

//----------------------------------------------------------------------------

void	CProfilerRenderer::_ToolTip_Rect(const CInt2 &where, CIRect &outRect) const
{
	(void)where;
	outRect = CIRect(CInt2(0, 0), CInt2(20, 40));
}

//----------------------------------------------------------------------------

void	CProfilerRenderer::_ToolTip_Draw(const CInt2 &where, float z)
{
	(void)where; (void)z;
}

//----------------------------------------------------------------------------

CRect	CProfilerRenderer::ComputeBounds() const
{
	return m_ProfilerDrawer.ComputeBounds(m_RenderData.m_DrawDescriptor, CurrentRecord().Get());
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END

#endif // PKSAMPLE_HAS_PROFILER_RENDERER
