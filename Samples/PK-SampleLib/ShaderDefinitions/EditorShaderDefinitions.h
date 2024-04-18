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

#include <PK-SampleLib/PKSample.h>
#include <pk_rhi/include/interfaces/SShaderBindings.h>
#include <pk_rhi/include/interfaces/SRenderState.h>
#include <pk_rhi/include/interfaces/IApiManager.h>

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

struct	SShaderCombination;

//----------------------------------------------------------------------------

enum	ParticleDebugShader
{
	ParticleDebugShader_White,
	ParticleDebugShader_Color,
	ParticleDebugShader_VertexColor, // Per vertex color: for meshes
	ParticleDebugShader_Selection,
	ParticleDebugShader_Error,
};

// All the possible geometry billboarding:
enum	EParticleDebugBillboarderType
{
	ParticleDebugBT_Billboard,									// Billboards/Ribbons (CPU streams)
	ParticleDebugBT_Mesh,										// Meshes
	ParticleDebugBT_Light,										// Lights (just like meshes but with just a position instead of a transform mat4)
	ParticleDebugBT_VertexBillboarding_C0,						// No axis constraint, 4 vertex generated
	ParticleDebugBT_VertexBillboarding_C1,						// 1 axis constraint, 4 vertex generated
	ParticleDebugBT_VertexBillboarding_C1_Capsule,				// 1 axis constraint, 6 vertex generated
	ParticleDebugBT_VertexBillboarding_C2,						// 2 axis constraint, 4 vertex generated
	ParticleDebugBT_VertexBillboarding_SizeFloat2_C0,			// No axis constraint, 4 vertex generated, size float2
	ParticleDebugBT_VertexBillboarding_SizeFloat2_C1,			// 1 axis constraint, 4 vertex generated, size float2
	ParticleDebugBT_VertexBillboarding_SizeFloat2_C1_Capsule,	// 1 axis constraint, 6 vertex generated, size float2
	ParticleDebugBT_VertexBillboarding_SizeFloat2_C2,			// 2 axis constraint, 4 vertex generated, size float2
	ParticleDebugBT_TriangleVertexBillboarding,					// Triangles
	ParticleDebugBT_GeomBillboarding_C0,						// No axis constraint, 4 vertex generated
	ParticleDebugBT_GeomBillboarding_C1,						// 1 axis constraint, 4 vertex generated
	ParticleDebugBT_GeomBillboarding_C1_Capsule,				// 1 axis constraint, 6 vertex generated
	ParticleDebugBT_GeomBillboarding_C2,						// 2 axis constraint, 4 vertex generated
	ParticleDebugBT_GeomBillboarding_SizeFloat2_C0,				// No axis constraint, 4 vertex generated, size float2
	ParticleDebugBT_GeomBillboarding_SizeFloat2_C1,				// 1 axis constraint, 4 vertex generated, size float2
	ParticleDebugBT_GeomBillboarding_SizeFloat2_C1_Capsule,		// 1 axis constraint, 6 vertex generated, size float2
	ParticleDebugBT_GeomBillboarding_SizeFloat2_C2,				// 2 axis constraint, 4 vertex generated, size float2
	_ParticleDebugBT_Count
};

void	FillEditorDebugParticleInputVertexBuffers(	TArray<RHI::SVertexInputBufferDesc>	&inputVertexBuffers,
													ParticleDebugShader shaderMode,
													EParticleDebugBillboarderType bbType,
													bool gpuStorage);
void	FillEditorDebugParticleShaderBindings(	RHI::SShaderBindings &bindings,
												ParticleDebugShader shaderMode,
												EParticleDebugBillboarderType bbType,
												bool gpuStorage,
												RHI::SShaderDescription *shaderDescription = null);
void	AddEditorDebugParticleDefinition(TArray<SShaderCombination> &shaders);
void	AddEditorDebugVertexBBParticleDefinition(TArray<SShaderCombination> &shaders);
void	AddEditorDebugGeomBBParticleDefinition(TArray<SShaderCombination> &shaders);

void	FillEditorDebugDrawShaderBindings(RHI::SShaderBindings &bindings, bool hasVertexColor, bool isClipSpace);
void	AddEditorDebugDrawDefinition(TArray<SShaderCombination> &shaders);

void	FillDebugDrawValueConstantSetLayout(RHI::SConstantSetLayout &layout);
void	FillEditorDebugDrawValueShaderBindings(RHI::SShaderBindings &bindings);
void	AddEditorDebugDrawValueDefinition(TArray<SShaderCombination> &shaders);

void	FillEditorDebugDrawLineShaderBindings(RHI::SShaderBindings &bindings);
void	AddEditorDebugDrawLineDefinition(TArray<SShaderCombination> &shaders);

void	AddEditorHeatmapOverdraw(TArray<SShaderCombination> &shaders);

void	CreateBrushBackdropInfoConstantSetLayout(RHI::SConstantSetLayout &layout);
void	FillBrushBackdropShaderBindings(RHI::SShaderBindings &bindings, const RHI::SConstantSetLayout &brushInfo, const RHI::SConstantSetLayout &environmentMapLayout);
void	AddBrushBackdropDefinition(TArray<SShaderCombination> &shaders);

void	FillParticleLightShaderBindings(RHI::SShaderBindings &bindings);
void	AddParticleLightDefinition(TArray<SShaderCombination> &shaders);

void	CreateEditorSelectorConstantSetLayout(RHI::SConstantSetLayout &layout);
void	AddEditorSelectorDefinition(TArray<SShaderCombination> &shaders);

//----------------------------------------------------------------------------

__PK_SAMPLE_API_END
