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

#if	defined(PK_DESKTOP) || defined(PK_ORBIS) || defined(PK_DURANGO) || defined(PK_UNKNOWN2)

#include <pk_particles_toolbox/include/pt_profiler_new.h>

#if	defined(PK_HAS_PARTICLESTOOLBOX_PROFILER)

#	define PKSAMPLE_HAS_PROFILER_RENDERER			1

#include <pk_geometrics/include/ge_mesh_vertex_declarations.h>

#include <pk_rhi/include/FwdInterfaces.h>
#include <pk_rhi/include/interfaces/IFrameBuffer.h>
#include <pk_rhi/include/interfaces/ICommandBuffer.h>

#include "ShaderLoader.h"

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

class	CProfilerRenderer : public CNonCopyable
{
public:
	enum	EProfilerRenderState
	{
		RenderState_Rect_Solid,
		RenderState_Rect_Blended,
		RenderState_Line_Solid,
		RenderState_Line_Blended,
		_RenderState_Count
	};

	struct	SProfilerRenderData
	{
		bool											m_IsValid;
		ParticleToolbox::SProfilerDrawerDescriptorNew	m_DrawDescriptor;

		SProfilerRenderData() : m_IsValid(false) { }
	};

	bool								EnableWithProfileReport(const Profiler::PProfilerReport &profileReport);
	bool								Enable(bool enable);
	bool								Enabled() const { return m_Enabled; }

	void								EnableCallstacks(bool enable) { m_CallstacksEnabled = enable; }
	bool								CallstacksEnabled() const { return m_CallstacksEnabled; }

	void								EnableMemoryView(bool enable) { m_RenderData.m_DrawDescriptor.m_MemoryView = enable; }
	bool								MemoryViewEnabled() const { return m_RenderData.m_DrawDescriptor.m_MemoryView; }

	void								CollapseThreads(bool collapsed) { m_RenderData.m_DrawDescriptor.m_Profiler_CondenseAllSatteliteThreads = collapsed; }
	bool								ThreadsCollapsed() const { return m_RenderData.m_DrawDescriptor.m_Profiler_CondenseAllSatteliteThreads; }

	void								SetHiglightFilter(const CString &matchPattern) { m_RenderData.m_DrawDescriptor.m_HightlightFilter = matchPattern; }
	void								SetMemoryMaxLimit(float maxMemInMb) { m_RenderData.m_DrawDescriptor.m_MemoryViewMaxMem = maxMemInMb; }

	void								SetCurrentHistoryFrame(s32 frameIndex);
	s32									CurrentHistoryFrame() const { return m_RenderData.m_DrawDescriptor.m_ProfilerHistoryFrameToDisplay; }

	void								ViewVerticalScroll(float scrollXInPixel);
	void								ViewHScroll(float scrollXInPixel);
	void								ViewHZoom(float zoomCenterXInPixel, float zoomFactor);

	double								GetHorizontalScale() const { return m_RenderData.m_DrawDescriptor.m_ProfilerScale; }
	void								SetHorizontalScale(double scale) { m_RenderData.m_DrawDescriptor.m_ProfilerScale = scale; }

	float								GetVerticalOffset() const { return m_RenderData.m_DrawDescriptor.m_ProfilerVerticalOffset; }
	void								SetVerticalOffset(float offset) { m_RenderData.m_DrawDescriptor.m_ProfilerVerticalOffset = offset; }
	double								GetHorizontalOffset() const { return m_RenderData.m_DrawDescriptor.m_ProfilerOffset; }
	void								SetHorizontalOffset(double offset) { m_RenderData.m_DrawDescriptor.m_ProfilerOffset = offset; }

private:
	struct	SRenderInfo
	{
		// To reset
		RHI::PApiManager					m_ApiManager;

		RHI::PRenderState					m_RectAlphaBlendDashed;
		RHI::PRenderState					m_RectAlphaBlend;
		RHI::PRenderState					m_RectAdditive;
		RHI::PRenderState					m_LineAlphaBlend;
		RHI::PRenderState					m_LineAdditive;

		RHI::PGpuBuffer						m_LinesVertexBuffer;
		RHI::PGpuBuffer						m_LinesColorBuffer;
		u32									m_LinesVertexCapacity;

		RHI::PGpuBuffer						m_RectsIndexBuffer;
		RHI::PGpuBuffer						m_RectsVertexBuffer;
		RHI::PGpuBuffer						m_RectsColorBuffer0;
		RHI::PGpuBuffer						m_RectsColorBuffer1;
		RHI::PGpuBuffer						m_DashedRectsBorderColor;
		u32									m_RectsVertexCapacity;
		u32									m_DashedRectsVertexCapacity;

		u32									m_ElemCountRectsAlphaBlendDashed;
		u32									m_ElemCountRectsAlphaBlend;
		u32									m_ElemCountRectsAdditive;
		u32									m_ElemCountLinesAlphaBlend;
		u32									m_ElemCountLinesAdditive;

		SRenderInfo() { Clear(); }
		~SRenderInfo() { }

		void		Clear();
	};

	// Game thread only:
	bool								m_Enabled;
	bool								m_Pause;
	bool								m_CallstacksEnabled;

	double								m_TargetProfilerScale;
	double								m_LastProfileFocusPos;
	float								m_ProfilerVerticalOffset;
	static const u32					kDtHistorySize = 32;
	float								m_DtHistory[kDtHistorySize];
	u32									m_DtHistoryPos;

	// Actual rendering data created by the update and used by the rendering
	SProfilerRenderData					m_RenderData;

	// Used for the profiler replay;
	bool									m_ProfilerReplay;

	// Render thread only:
	ParticleToolbox::CProfilerDrawerNew		m_ProfilerDrawer;
	ParticleToolbox::SProfilerDrawOutput	m_DrawOutputs;
	TArray<CString>							m_ToolTipMsgs;
	SRenderInfo								m_RenderInfo;

	// Accessed by both render and main thread:
	typedef TSemiDynamicArray<Profiler::PProfilerReport, 0x10>		DynamicProfileReportList;
	typedef TStaticCountedArray<Profiler::PProfilerReport, 0x10>	StaticProfileReportList;

	Threads::CCriticalSection	m_NewProfilerFramesLock;
	DynamicProfileReportList	m_NewProfilerFrames;	// Dynamic array: we do not want to miss an update
	Threads::CCriticalSection	m_OldProfilerFramesLock;
	StaticProfileReportList		m_OldProfilerFrames;	// Static array: pool of profile reports

	void								_SetupDefaultDescriptor(ParticleToolbox::SProfilerDrawerDescriptorNew &descriptor);

	bool								_TransferBatchToGpu(const SProfilerRenderData &renderData);

public:
	CProfilerRenderer();
	~CProfilerRenderer();

	// Setup
	void				Setup(const ParticleToolbox::SProfilerDrawerDescriptorNew &descriptor);

	// Defines the end of last frame the beginning of a new frame to profile
	void				ProfilerNextFrameTick();

	// Billboards profiling vertices of last frame
	void				GenerateProfilerRenderData(double dt, const CRect &drawRect, const CInt2 &cursorPos, float pixelRatio = 1.0f);
	void				LockAndCopyProfilerData(SProfilerRenderData &renderData);

	void				UpdateDrawGeometry(const CRect &drawRect, const CInt2 &cursorPos, float pixelRatio = 1.0f);	// Called by 'GenerateProfilerRenderData()'

	// Render thread:
	bool				CreateRenderInfo(	const RHI::PApiManager &apiManager,
											CShaderLoader &loader,
											TMemoryView<const RHI::SRenderTargetDesc> famebufferLayout,
											RHI::PRenderPass renderPass, u32 subPass);
	bool				CreateRenderInfoIFN(const RHI::PApiManager &apiManager,
											CShaderLoader &loader,
											TMemoryView<const RHI::SRenderTargetDesc> famebufferLayout,
											RHI::PRenderPass renderPass, u32 subPass);

	const SProfilerRenderData	&UnsafeGetProfilerData() const;

	bool				Render(	const RHI::PCommandBuffer &commandBuff,
								const CUint2 &contextSize,
								const SProfilerRenderData &profilerData);
	void				ReleaseRenderInfo();

	CRect				ComputeBounds() const;

	bool				IsPaused() const { return m_Pause; }
	void				SetPaused(bool pause);
	void				Zoom(float zoom, const CUint2 &windowSize);
	void				Offset(float offset);
	void				VerticalOffset(float vOffset);

	TMemoryView<const CString>	GetTooltips() const { return m_ToolTipMsgs.View(); }

	Profiler::PProfilerReport	CurrentRecord() const;

	void				_FontDraw(ParticleToolbox::EFontType font, const CFloat3 &position, const TMemoryView<const char> &textView, const CFloat4 &color) const;
	float				_FontLength(ParticleToolbox::EFontType font, const TMemoryView<const char> &textView) const;
	void				_FontInfo(ParticleToolbox::EFontType font, ParticleToolbox::SFontDesc &outDesc) const;
	void				_ToolTip_AddString(const CString &string);
	void				_ToolTip_Rect(const CInt2 &where, CIRect &outRect) const;
	void				_ToolTip_Draw(const CInt2 &where, float z);
};

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END

#endif	// defined(PK_HAS_PARTICLESTOOLBOX_PROFILER)
#endif	// defined(PK_DESKTOP) || defined(PK_ORBIS) || defined(PK_DURANGO)

#ifndef PKSAMPLE_HAS_PROFILER_RENDERER
#	define	PKSAMPLE_HAS_PROFILER_RENDERER		0
#endif
