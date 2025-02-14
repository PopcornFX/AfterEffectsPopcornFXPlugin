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

#include "ApiContextConfig.h"

#include <imgui.h>

#include <PK-SampleLib/PKSample.h>
#include <pk_rhi/include/AllInterfaces.h>

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

PK_FORWARD_DECLARE(ShaderLoader);

//----------------------------------------------------------------------------

class	ImGuiPkRHI
{
public:
	struct	SRenderInfo
	{
		RHI::PApiManager			m_ApiManager;
		RHI::PRenderState			m_AlphaBlend;
		RHI::PTexture				m_FontTexture;
		RHI::PConstantSampler		m_FontSampler;
		RHI::SConstantSetLayout		m_ShaderConstSetLayout;
		RHI::PConstantSet			m_ShaderConstSet;

		SRenderInfo()
		{
			Clear();
		}

		~SRenderInfo() { }

		void	Clear()
		{
			m_ApiManager = null;
			m_AlphaBlend = null;
			m_FontTexture = null;
			m_FontSampler = null;
			m_ShaderConstSetLayout.Reset();
			m_ShaderConstSet = null;
		}
	};

	struct	SFrameRenderInfo
	{
		RHI::PGpuBuffer			m_VertexBuffer;
		u32						m_VertexCount;
		RHI::PGpuBuffer			m_IndexBuffer;
		u32						m_IndexCount;
		ImGuiContext			*m_ImGuiContext;

		SFrameRenderInfo();
		~SFrameRenderInfo();

		void	Clear();
	};

	ImGuiPkRHI();
	~ImGuiPkRHI();

	// From render thread
	struct	SImguiInit
	{
		bool			m_IsMultiThreaded;			// Set this to true if DrawRenderData is called on a separate thread
		int				m_KeyMap[ImGuiKey_COUNT];

		struct	SFontDesc
		{
			const char	*m_FontPath;
			float		m_FontSize;	// only used if m_FontPath != null

			SFontDesc() : m_FontPath(null), m_FontSize(0) {}
			SFontDesc(const char *path, float size) : m_FontPath(path), m_FontSize(size) {}
		};
		TMemoryView<SFontDesc>	m_FontDescs;

		SImguiInit()
		:	m_IsMultiThreaded(false)
		{
			Mem::Clear(m_KeyMap);
		}
	};

	// This should be called on the same thread
	static bool			Init(const SImguiInit &initData);
	static void			Quit();
	static void			QuitIFN() { return Quit(); }	// Useless, will be deprecated in v2.22
	static bool			CreateRenderInfo(	const RHI::PApiManager &apiManager,
											CShaderLoader &loader,
											const TMemoryView<const RHI::SRenderTargetDesc> &frameBufferLayout,
											const RHI::PRenderPass &renderPass,
											u32 subPassIdx);
	static void			ReleaseRenderInfo();
	static bool			CreateViewport();
	static void			ReleaseViewport();
	static void			NewFrame(CUint2 contextSize, float dt, float devicePixelRatio = 1.0f, float fontScale = 1.0f);
	static ImDrawData	*GenerateRenderData();
	static void			DrawRenderData(ImDrawData *data, const RHI::PCommandBuffer &commandBuffer);
	static void			EndFrame();

	// From main thread
	enum	KeyModifiers
	{
		Modifier_Shift	= (1 << 0),
		Modifier_Ctrl	= (1 << 1),
		Modifier_Alt	= (1 << 2),
		Modifier_Super	= (1 << 3),
	};

	enum	KeyMouseButtons
	{
		MouseButton_Left	= (1 << 0),
		MouseButton_Middle	= (1 << 1),
		MouseButton_Right	= (1 << 2),
	};

	// This can be called on other threads
	static void			MouseMoved(const CInt2 &mousePosition);
	static void			MouseWheel(float mouseWheel);
	static void			MouseButtonEvents(bool pressed, int keyMouseButtons);
	static void			KeyEvents(bool pressed, int key, int keyModifiers);
	static void			TextInput(const char *c);
	static void			WindowLostFocus();
	static bool			Hovered();

#ifdef		PK_WINDOWS
	static void			ChangeWindowHandle(HWND window);
#endif

	static void			UpdateInputs();

	static ImGuiPkRHI			*GetInstance();

	const RHI::PApiManager		&GetApiManager() const;
	SRenderInfo					&GetRenderInfo();
	SFrameRenderInfo			&GetFrameRenderInfo();
	const RHI::PConstantSet		&GetConstantSet() const;
	const RHI::PRenderState		&GetRenderState() const;

	static ImDrawData			*CopyDrawData(const ImDrawData *data);
	static void					DeleteDrawData(ImDrawData *data);

	void						NextViewport();
private:
	static bool					CreateFontTexture();

	// Memory management Fn
	static void					*Malloc(size_t size, void *);
	static void					Free(void *ptr, void *);


	template<class _Type, bool (*_CopyTypeFunc)(_Type &dst, const _Type &src)>
	static bool					CopyImguiVector(ImVector<_Type> &vectorDst,
												const ImVector<_Type> &vectorSrc)
	{
		vectorDst.resize(vectorSrc.size());
		for (s32 i = 0; i < vectorSrc.size(); ++i)
		{
			if (!_CopyTypeFunc(vectorDst[i], vectorSrc[i]))
				return false;
		}
		return true;
	}

	static ImGuiPkRHI			*m_Instance;

	bool						m_IsMultiThreaded;

	double						m_Time;
	float						m_MouseWheel;

	SRenderInfo					m_RenderInfo;
	TArray<SFrameRenderInfo>	m_ViewportFrameInfo;
	CGuid						m_ViewportIndex;
	ImGuiContext				*m_DefaultContext;
	SImguiInit					m_InitData;
};

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
