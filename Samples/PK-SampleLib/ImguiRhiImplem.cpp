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

#include "ImguiRhiImplem.h"
#include "SampleUtils.h"
#include "ShaderLoader.h"

#include "ShaderDefinitions/UnitTestsShaderDefinitions.h"
#include "ShaderDefinitions/SampleLibShaderDefinitions.h"

#include <pk_rhi/include/AllInterfaces.h>
#include <pk_rhi/include/PixelFormatFallbacks.h>
#include <pk_kernel/include/kr_threads_basics.h>

#define		VERTEX_SHADER_PATH		"./Shaders/ImGui.vert"
#define		FRAGMENT_SHADER_PATH	"./Shaders/ImGui.frag"

#define		IMGUI_QUEUED_FRAMES		3

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

ImGuiPkRHI	*ImGuiPkRHI::m_Instance = null;

//----------------------------------------------------------------------------

ImGuiPkRHI::ImGuiPkRHI()
{
	m_Time = 0.0f;
	m_MouseWheel = 0.0f;
	m_DefaultContext = ImGui::CreateContext();
}

//----------------------------------------------------------------------------

ImGuiPkRHI::~ImGuiPkRHI()
{
	m_RenderInfo.Clear();
}

//----------------------------------------------------------------------------

bool	ImGuiPkRHI::Init(const SImguiInit &initData)
{
	PK_SCOPEDPROFILE();

	ImGui::SetAllocatorFunctions(&ImGuiPkRHI::Malloc, &ImGuiPkRHI::Free, null);

	if (PK_VERIFY(m_Instance == null))
		m_Instance = PK_NEW(ImGuiPkRHI);
	if (m_Instance == null)
		return false;

	ImGuiIO	&io = ImGui::GetIO();

	Mem::Copy(io.KeyMap, initData.m_KeyMap, ImGuiKey_COUNT * sizeof(int));
	io.RenderDrawListsFn = null; // Alternatively you can set this to NULL and call ImGui::GetDrawData() after ImGui::Render() to get the same ImDrawData pointer.

	if (PK_VERIFY(io.Fonts != null) && !initData.m_FontDescs.Empty())
	{
		for (const auto &font : initData.m_FontDescs)
		{
			if (font.m_FontPath == null)
				io.Fonts->AddFontDefault();
			else
				io.Fonts->AddFontFromFileTTF(font.m_FontPath, font.m_FontSize);
		}
	}

	m_Instance->m_IsMultiThreaded = initData.m_IsMultiThreaded;
	{
		// Initializes imgui font
		unsigned char	*pixels = null;
		int				width = 0;
		int				height = 0;

		io.Fonts->GetTexDataAsAlpha8(&pixels, &width, &height);
	}
	m_Instance->m_InitData = initData;

	return true;
}

//----------------------------------------------------------------------------

void	ImGuiPkRHI::Quit()
{
	PK_SCOPEDPROFILE();
	if (m_Instance != null)
	{
		ImGui::SetCurrentContext(m_Instance->m_DefaultContext);
		ImGui::DestroyContext();
		PK_ASSERT(m_Instance != null);
		PK_DELETE(m_Instance);
		m_Instance = null;
	}
}

//----------------------------------------------------------------------------

bool	ImGuiPkRHI::CreateRenderInfo(	const RHI::PApiManager &apiManager,
										CShaderLoader &loader,
										const TMemoryView<const RHI::SRenderTargetDesc> &frameBufferLayout,
										const RHI::PRenderPass &renderPass,
										u32 subPassIdx)
{
	PK_SCOPEDPROFILE();
	PK_ASSERT(m_Instance != null);
	m_Instance->m_RenderInfo.m_ApiManager = apiManager;
	m_Instance->m_RenderInfo.m_AlphaBlend = m_Instance->m_RenderInfo.m_ApiManager->CreateRenderState(RHI::SRHIResourceInfos("ImGui AlphaBlend Render State"));
	if (m_Instance->m_RenderInfo.m_AlphaBlend == null)
		return false;

	RHI::SRenderState	&renderState = m_Instance->m_RenderInfo.m_AlphaBlend->m_RenderState;

	renderState.m_PipelineState.m_DynamicViewport = true;
	renderState.m_PipelineState.m_DynamicScissor = true;

	renderState.m_PipelineState.m_Blending = true;
	renderState.m_PipelineState.m_ColorBlendingSrc = RHI::BlendSrcAlpha;
	renderState.m_PipelineState.m_ColorBlendingDst = RHI::BlendOneMinusSrcAlpha;
	renderState.m_PipelineState.m_ColorBlendingEquation = RHI::BlendAdd;

	if (!renderState.m_InputVertexBuffers.PushBack().Valid())
		return false;
	renderState.m_InputVertexBuffers.Last().m_Stride = sizeof(ImDrawVert);

	if (m_Instance->m_RenderInfo.m_ShaderConstSetLayout.m_Constants.Empty())
	{
		CreateSimpleSamplerConstSetLayouts(m_Instance->m_RenderInfo.m_ShaderConstSetLayout, false);
	}

	FillImGuiShaderBindings(renderState.m_ShaderBindings, m_Instance->m_RenderInfo.m_ShaderConstSetLayout);

	renderState.m_ShaderBindings.m_InputAttributes[0].m_BufferIdx = 0;
	renderState.m_ShaderBindings.m_InputAttributes[0].m_StartOffset = PK_MEMBER_OFFSET(ImDrawVert, pos);

	renderState.m_ShaderBindings.m_InputAttributes[1].m_BufferIdx = 0;
	renderState.m_ShaderBindings.m_InputAttributes[1].m_StartOffset = PK_MEMBER_OFFSET(ImDrawVert, uv);

	renderState.m_ShaderBindings.m_InputAttributes[2].m_ShaderLocationBinding = 2;
	renderState.m_ShaderBindings.m_InputAttributes[2].m_StartOffset = PK_MEMBER_OFFSET(ImDrawVert, col);

	CShaderLoader::SShadersPaths shadersPaths;
	shadersPaths.m_Fragment	= FRAGMENT_SHADER_PATH;
	shadersPaths.m_Vertex	= VERTEX_SHADER_PATH;
	if (loader.LoadShader(renderState, shadersPaths, m_Instance->m_RenderInfo.m_ApiManager) == false)
		return false;

	if (m_Instance->m_RenderInfo.m_ApiManager->BakeRenderState(m_Instance->m_RenderInfo.m_AlphaBlend, frameBufferLayout, renderPass, subPassIdx) == false)
		return false;

	if (m_Instance->m_RenderInfo.m_FontTexture == null)
		return CreateFontTexture();
	return true;
}

//----------------------------------------------------------------------------

void	ImGuiPkRHI::ReleaseRenderInfo()
{
	PK_SCOPEDPROFILE();
	PK_ASSERT(m_Instance != null);
	m_Instance->m_RenderInfo.Clear();
}

//----------------------------------------------------------------------------

bool	ImGuiPkRHI::CreateViewport()
{
	PK_SCOPEDPROFILE();
	PK_ASSERT(m_Instance != null);
	return m_Instance->m_ViewportFrameInfo.PushBack().Valid();
}

//----------------------------------------------------------------------------

void	ImGuiPkRHI::ReleaseViewport()
{
	PK_SCOPEDPROFILE();
	if (PK_VERIFY(m_Instance != null))
	{
		PK_ASSERT(m_Instance->m_ViewportFrameInfo.Count() > 0);
		m_Instance->m_ViewportFrameInfo.PopBackAndDiscard();
	}
}

//----------------------------------------------------------------------------

void	ImGuiPkRHI::NewFrame(CUint2 contextSize, float dt, float devicePixelRatio /*= 1.0f*/, float fontScale /*= 1.0f*/)
{
	PK_SCOPEDPROFILE();
	PK_ASSERT(m_Instance != null);
	m_Instance->NextViewport();
	// Set the render size
	ImGuiIO	&io = ImGui::GetIO();

	io.DisplaySize = ImVec2((float)contextSize.x(), (float)contextSize.y());
	io.DisplayFramebufferScale = ImVec2(devicePixelRatio, devicePixelRatio);
	io.FontGlobalScale = fontScale;

	// Setup time step
	io.DeltaTime = dt > 0.0 ? dt : (float)(1.0f / 60.0f);

	// Start the frame
	ImGui::NewFrame();
}

//----------------------------------------------------------------------------

void	ImGuiPkRHI::EndFrame()
{
	PK_SCOPEDPROFILE();
	PK_ASSERT(m_Instance != null);
	m_Instance->m_ViewportIndex.Clear();
}

//----------------------------------------------------------------------------

ImDrawData	*ImGuiPkRHI::GenerateRenderData()
{
	PK_SCOPEDPROFILE();
	PK_ASSERT(m_Instance != null);
	ImGui::Render();
	return ImGui::GetDrawData();
}

//----------------------------------------------------------------------------

void	ImGuiPkRHI::DrawRenderData(ImDrawData *draw_data, const RHI::PCommandBuffer &cmdBuff)
{
	PK_NAMEDSCOPEDPROFILE("ImGui pass");
	ImGuiPkRHI	*instance = ImGuiPkRHI::GetInstance();

	// Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
	ImGuiIO		&io = ImGui::GetIO();

	const int	fb_width = (int)(io.DisplaySize.x * io.DisplayFramebufferScale.x);
	const int	fb_height = (int)(io.DisplaySize.y * io.DisplayFramebufferScale.y);
	if (fb_width == 0 || fb_height == 0)
		return;
	draw_data->ScaleClipRects(io.DisplayFramebufferScale);

	ImGuiPkRHI::SFrameRenderInfo	&frameRenderInfo = instance->GetFrameRenderInfo();

	// Create the vertex and index buffer IFN
	if (frameRenderInfo.m_VertexCount < static_cast<u32>(draw_data->TotalVtxCount))
	{
		frameRenderInfo.m_VertexCount = draw_data->TotalVtxCount;
		frameRenderInfo.m_VertexBuffer = instance->GetApiManager()->CreateGpuBuffer(RHI::SRHIResourceInfos("ImGui Vertex Buffer"), RHI::VertexBuffer, draw_data->TotalVtxCount * sizeof(ImDrawVert));
	}
	if (frameRenderInfo.m_IndexCount < static_cast<u32>(draw_data->TotalIdxCount))
	{
		frameRenderInfo.m_IndexCount = draw_data->TotalIdxCount;
		frameRenderInfo.m_IndexBuffer = instance->GetApiManager()->CreateGpuBuffer(RHI::SRHIResourceInfos("ImGui Index Buffer"), RHI::IndexBuffer, draw_data->TotalIdxCount * sizeof(ImDrawIdx));
	}

	if (frameRenderInfo.m_VertexCount == 0 || frameRenderInfo.m_IndexCount == 0 ||
		!PK_VERIFY(frameRenderInfo.m_VertexBuffer != null && frameRenderInfo.m_IndexBuffer != null))
		return ;

	// Fill the buffers
	ImDrawVert	*vertexData = static_cast<ImDrawVert*>(instance->GetApiManager()->MapCpuView(frameRenderInfo.m_VertexBuffer));
	ImDrawIdx	*indexData = static_cast<ImDrawIdx*>(instance->GetApiManager()->MapCpuView(frameRenderInfo.m_IndexBuffer));

	if (vertexData == null || indexData == null)
		return;

	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = draw_data->CmdLists[n];

		Mem::Copy(vertexData, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
		Mem::Copy(indexData, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
		vertexData += cmd_list->VtxBuffer.Size;
		indexData += cmd_list->IdxBuffer.Size;
	}

	instance->GetApiManager()->UnmapCpuView(frameRenderInfo.m_VertexBuffer);
	instance->GetApiManager()->UnmapCpuView(frameRenderInfo.m_IndexBuffer);

	// We push the constants used for rendering
	float	scaleTranslate[4];
	scaleTranslate[0] = 2.0f / io.DisplaySize.x;
	scaleTranslate[1] = 2.0f / io.DisplaySize.y;
	scaleTranslate[2] = -1.0f;
	scaleTranslate[3] = -1.0f;

	cmdBuff->BindRenderState(instance->GetRenderState());

	cmdBuff->PushConstant(scaleTranslate, 0);

	// Setup the correct viewport
	cmdBuff->SetViewport(CInt2(0, 0), CUint2(fb_width, fb_height), CFloat2(0, 1));

	cmdBuff->BindVertexBuffers(TMemoryView<const RHI::PGpuBuffer>(frameRenderInfo.m_VertexBuffer));
	cmdBuff->BindIndexBuffer(frameRenderInfo.m_IndexBuffer, 0, RHI::IndexBuffer16Bit);
	cmdBuff->BindConstantSets(TMemoryView<const RHI::PConstantSet>(instance->GetConstantSet()));

	// Start to render the draw list
	u32	vtx_offset = 0;
	u32	idx_offset = 0;
	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		const ImDrawList	*cmd_list = draw_data->CmdLists[n];

		for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
		{
			const ImDrawCmd	*pcmd = &cmd_list->CmdBuffer[cmd_i];
			if (pcmd->UserCallback)
			{
				pcmd->UserCallback(cmd_list, pcmd);
			}
			else
			{
				CFloat2		clipRectOrigin = CFloat2(pcmd->ClipRect.x, pcmd->ClipRect.y);
				CFloat2 	clipRectSize = CFloat2(pcmd->ClipRect.z, pcmd->ClipRect.w) - clipRectOrigin;

				cmdBuff->SetScissor(CInt2((s32)(clipRectOrigin.x()), (s32)(clipRectOrigin.y())),
									CUint2((u32)(clipRectSize.x()), (u32)(clipRectSize.y())));
				cmdBuff->DrawIndexed(idx_offset, vtx_offset, pcmd->ElemCount);
			}
			idx_offset += pcmd->ElemCount;
		}
		vtx_offset += cmd_list->VtxBuffer.Size;
	}
}

//----------------------------------------------------------------------------

void	ImGuiPkRHI::MouseMoved(const CInt2 &mousePosition)
{
	PK_ASSERT(m_Instance != null);
	ImGuiIO& io = ImGui::GetIO();
	io.MousePos = ImVec2(mousePosition.x(), mousePosition.y());
}

//----------------------------------------------------------------------------

void	ImGuiPkRHI::MouseWheel(float mouseWheel)
{
	PK_ASSERT(m_Instance != null);
	if (mouseWheel > 0)
		m_Instance->m_MouseWheel = 1;
	if (mouseWheel < 0)
		m_Instance->m_MouseWheel = -1;
}

//----------------------------------------------------------------------------

void	ImGuiPkRHI::MouseButtonEvents(bool pressed, int keyMouseButtons)
{
	PK_ASSERT(m_Instance != null);
	ImGuiIO	&io = ImGui::GetIO();

	io.MouseDown[0] = (keyMouseButtons & MouseButton_Left) ? pressed : io.MouseDown[0];
	io.MouseDown[1] = (keyMouseButtons & MouseButton_Middle) ? pressed : io.MouseDown[1];
	io.MouseDown[2] = (keyMouseButtons & MouseButton_Right) ? pressed : io.MouseDown[2];
}

//----------------------------------------------------------------------------

void	ImGuiPkRHI::KeyEvents(bool pressed, int key, int keyModifiers)
{
	PK_ASSERT(m_Instance != null);
	ImGuiIO	&io = ImGui::GetIO();
	io.KeysDown[key] = pressed;
	io.KeyShift = (keyModifiers & Modifier_Shift) ? pressed : io.KeyShift;
	io.KeyCtrl = (keyModifiers & Modifier_Ctrl) ? pressed : io.KeyCtrl;
	io.KeyAlt = (keyModifiers & Modifier_Alt) ? pressed : io.KeyAlt;
	io.KeySuper = (keyModifiers & Modifier_Super) ? pressed : io.KeySuper;
}

//----------------------------------------------------------------------------

void	ImGuiPkRHI::TextInput(const char *c)
{
	PK_ASSERT(m_Instance != null);
	ImGuiIO	&io = ImGui::GetIO();
	io.AddInputCharactersUTF8(c);
}

//----------------------------------------------------------------------------

void	ImGuiPkRHI::WindowLostFocus()
{
	PK_ASSERT(m_Instance != null);
	ImGuiIO	&io = ImGui::GetIO();
	io.MouseDown[0] = false;
	io.MouseDown[1] = false;
	io.MouseDown[2] = false;
}

//----------------------------------------------------------------------------

bool	ImGuiPkRHI::Hovered()
{
	return ImGui::IsMouseHoveringAnyWindow() || ImGui::IsAnyItemHovered();
}

//----------------------------------------------------------------------------

#ifdef PK_WINDOWS
void	ImGuiPkRHI::ChangeWindowHandle(HWND window)
{
	PK_ASSERT(m_Instance != null);
	ImGuiIO	&io = ImGui::GetIO();
	io.ImeWindowHandle = window;
}
#endif

//----------------------------------------------------------------------------

void	ImGuiPkRHI::UpdateInputs()
{
	PK_ASSERT(m_Instance != null);
	ImGuiIO	&io = ImGui::GetIO();
	io.MouseWheel = m_Instance->m_MouseWheel;
	m_Instance->m_MouseWheel = 0.0f;
}

//----------------------------------------------------------------------------

ImGuiPkRHI	*ImGuiPkRHI::GetInstance()
{
	PK_ASSERT(m_Instance != null);
	return m_Instance;
}

//----------------------------------------------------------------------------

const RHI::PApiManager	&ImGuiPkRHI::GetApiManager() const
{
	return m_RenderInfo.m_ApiManager;
}

//----------------------------------------------------------------------------

ImGuiPkRHI::SRenderInfo	&ImGuiPkRHI::GetRenderInfo()
{
	return m_RenderInfo;
}

//----------------------------------------------------------------------------

ImGuiPkRHI::SFrameRenderInfo	&ImGuiPkRHI::GetFrameRenderInfo()
{
	PK_ASSERT(m_ViewportIndex < m_ViewportFrameInfo.Count());
	return m_ViewportFrameInfo[m_ViewportIndex];
}

//----------------------------------------------------------------------------

const RHI::PConstantSet	&ImGuiPkRHI::GetConstantSet() const
{
	return m_RenderInfo.m_ShaderConstSet;
}

//----------------------------------------------------------------------------

const RHI::PRenderState	&ImGuiPkRHI::GetRenderState() const
{
	return m_RenderInfo.m_AlphaBlend;
}

//----------------------------------------------------------------------------
//
//	Copy ImGui data (copy constructor and copy operator are not implemented in ImGui)
//
//----------------------------------------------------------------------------

static bool	copyDrawCommand(ImDrawCmd &dst, const ImDrawCmd &src)
{
	dst.ElemCount = src.ElemCount;
	dst.ClipRect = src.ClipRect;
	dst.TextureId = src.TextureId;
	dst.UserCallback = src.UserCallback;
	dst.UserCallbackData = src.UserCallbackData;
	return true;
}

//----------------------------------------------------------------------------

static bool	copyDrawIdx(ImDrawIdx &dst, const ImDrawIdx &src)
{
	dst = src;
	return true;
}

//----------------------------------------------------------------------------

static bool	copyDrawVert(ImDrawVert &dst, const ImDrawVert &src)
{
	dst = src;
	return true;
}

//----------------------------------------------------------------------------

ImDrawData	*ImGuiPkRHI::CopyDrawData(const ImDrawData *data)
{
	PK_SCOPEDPROFILE();
	if (data->CmdListsCount == 0)
		return null;

	ImDrawData	*copiedData = PK_NEW(ImDrawData);
	if (copiedData == null)
		return null;

	copiedData->Valid = data->Valid;
	copiedData->TotalVtxCount = data->TotalVtxCount;
	copiedData->TotalIdxCount = data->TotalIdxCount;
	copiedData->CmdListsCount = data->CmdListsCount;

	copiedData->CmdLists = PK_NEW(ImDrawList*[data->CmdListsCount]);
	if (copiedData->CmdLists == null)
	{
		DeleteDrawData(copiedData);
		return null;
	}

	for (s32 i = 0; i < data->CmdListsCount; ++i)
	{
		copiedData->CmdLists[i] = PK_NEW(ImDrawList(ImGui::GetDrawListSharedData()));
		if (copiedData->CmdLists[i] == null)
		{
			DeleteDrawData(copiedData);
			return null;
		}

		if (!CopyImguiVector<ImDrawCmd, copyDrawCommand>(copiedData->CmdLists[i]->CmdBuffer, data->CmdLists[i]->CmdBuffer))
		{
			DeleteDrawData(copiedData);
			return null;
		}
		if (!CopyImguiVector<ImDrawIdx, copyDrawIdx>(copiedData->CmdLists[i]->IdxBuffer, data->CmdLists[i]->IdxBuffer))
		{
			DeleteDrawData(copiedData);
			return null;
		}
		if (!CopyImguiVector<ImDrawVert, copyDrawVert>(copiedData->CmdLists[i]->VtxBuffer, data->CmdLists[i]->VtxBuffer))
		{
			DeleteDrawData(copiedData);
			return null;
		}
	}
	return copiedData;
}

//----------------------------------------------------------------------------

void	ImGuiPkRHI::DeleteDrawData(ImDrawData *data)
{
	PK_SCOPEDPROFILE();
	for (s32 i = 0; i < data->CmdListsCount; ++i)
	{
		PK_DELETE(data->CmdLists[i]);
	}
	PK_DELETE(data->CmdLists);
	PK_DELETE(data);
}

//----------------------------------------------------------------------------

bool	ImGuiPkRHI::CreateFontTexture()
{
	// Build texture atlas
	unsigned char	*pixels = null;
	int				width = 0;
	int				height = 0;
	int				bpp = 0;

	ImGuiIO	&io = ImGui::GetIO();
	io.Fonts->GetTexDataAsAlpha8(&pixels, &width, &height, &bpp);

	CImage	fontImage;

	fontImage.m_Format = CImage::Format_Lum8;
	if (!fontImage.m_Frames.PushBack().Valid() ||
		!fontImage.m_Frames.Last().m_Mipmaps.PushBack(CImageMap(CUint3(width, height, 1), pixels, width * height * bpp)).Valid())
		return false;

	m_Instance->m_RenderInfo.m_FontTexture = RHI::PixelFormatFallbacks::CreateTextureAndFallbackIFN(m_Instance->m_RenderInfo.m_ApiManager, fontImage, true, "IMGUI STATIC FONT IMAGE");
	m_Instance->m_RenderInfo.m_FontSampler = m_Instance->m_RenderInfo.m_ApiManager->CreateConstantSampler(RHI::SRHIResourceInfos("ImGui Font Sampler"), RHI::SampleLinear, RHI::SampleLinear, RHI::SampleClampToEdge, RHI::SampleClampToEdge, RHI::SampleClampToEdge, m_Instance->m_RenderInfo.m_FontTexture->GetMipmapCount());

	m_Instance->m_RenderInfo.m_ShaderConstSet = m_Instance->m_RenderInfo.m_ApiManager->CreateConstantSet(RHI::SRHIResourceInfos("ImGui Constant Set"), m_Instance->m_RenderInfo.m_ShaderConstSetLayout);
	m_Instance->m_RenderInfo.m_ShaderConstSet->SetConstants(m_Instance->m_RenderInfo.m_FontSampler, m_Instance->m_RenderInfo.m_FontTexture, 0);
	m_Instance->m_RenderInfo.m_ShaderConstSet->UpdateConstantValues();

	// Store our identifier
	io.Fonts->TexID = (void *)m_Instance->m_RenderInfo.m_FontTexture.Get();
	return true;
}

//----------------------------------------------------------------------------

void	*ImGuiPkRHI::Malloc(size_t size, void *)
{
	return PK_MALLOC(checked_numcast<u32>(size));
}

//----------------------------------------------------------------------------

void	ImGuiPkRHI::Free(void *ptr, void *)
{
	PK_FREE(ptr);
}

void	ImGuiPkRHI::NextViewport()
{
	if (m_ViewportIndex.Valid())
		m_ViewportIndex++;
	else
		m_ViewportIndex = 0;
	PK_ASSERT_MESSAGE(m_ViewportIndex < m_ViewportFrameInfo.Count(), "ImGuiPkRHI::ViewportIndex invalid. Missing call to ImGuiPkRHI::EndFrame or ImGuiPkRHI::CreateViewport");
	ImGui::SetCurrentContext(GetFrameRenderInfo().m_ImGuiContext);
}

//----------------------------------------------------------------------------

ImGuiPkRHI::SFrameRenderInfo::SFrameRenderInfo()
	: m_ImGuiContext(null)
{
	Clear();
	ImFontAtlas	*atlas = ImGui::GetIO().Fonts;
	m_ImGuiContext = ImGui::CreateContext(atlas);
	ImGuiIO	&io = ImGui::GetIO();
	Mem::Copy(io.KeyMap, m_Instance->m_InitData.m_KeyMap, ImGuiKey_COUNT * sizeof(int));
	io.RenderDrawListsFn = null; // Alternatively you can set this to NULL and call ImGui::GetDrawData() after ImGui::Render() to get the same ImDrawData pointer.
}

//----------------------------------------------------------------------------

ImGuiPkRHI::SFrameRenderInfo::~SFrameRenderInfo()
{
	Clear();
}

//----------------------------------------------------------------------------

void	ImGuiPkRHI::SFrameRenderInfo::Clear()
{
	m_VertexBuffer = null;
	m_VertexCount = 0;
	m_IndexBuffer = null;
	m_IndexCount = 0;
	if (m_ImGuiContext != null)
	{
		ImGui::SetCurrentContext(m_ImGuiContext);
		ImGui::GetIO().Fonts = null; // avoid to destroy shared Font
		ImGui::SetCurrentContext(m_Instance->m_DefaultContext);
		ImGui::DestroyContext(m_ImGuiContext);
	}
	m_ImGuiContext = null;
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
