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

#include "SimInterfaces.h"

#include <pk_particles/include/Updaters/CPU/updater_cpu.h>
#include <pk_particles/include/ps_project_settings.h>
#include <pk_particles/include/ps_simulation_interface.h>
#include <pk_particles/include/ps_compiler_metadata.h>

#include <pk_compiler/include/cp_backend.h>
#include <pk_compiler/include/cp_binders.h>
#include <pk_compiler/include/cp_ir_details.h>
#include <pk_compiler/include/cp_ir_ranges.h>
#include <pk_particles/include/Updaters/GPU/updater_gpu.h>

// PK-RHI and PK-SampleLib are used for generating shader code & bindings, only PK-Particles
// is required in an engine not using PK-SampleLib for rendering.

// Only required when baking effects
#if (PK_COMPILER_BUILD_COMPILER_D3D != 0)
	// Source shaders code built into a char array.
	// Source shaders can be found in source_tree/SDK/Samples/PK-Samples/PK-SampleLib/Assets/ShaderIncludes/sources/GPUSimInterfaces
#	include <PK-SampleLib/Assets/ShaderIncludes/generated/GPUSimInterface_GBuffer_ProjectToPosition.d3d.h>
#	include <PK-SampleLib/Assets/ShaderIncludes/generated/GPUSimInterface_GBuffer_ProjectToNormal.d3d.h>

#	include <PK-SampleLib/ShaderGenerator/HLSLShaderGenerator.h>
#endif // (PK_COMPILER_BUILD_COMPILER_D3D != 0)

// Required at runtime
#if (PK_PARTICLES_UPDATER_USE_D3D != 0)
#	include <pk_particles/include/Kernels/D3D11/kernel_d3d11.h>
#	include <pk_particles/include/Updaters/D3D11/updater_d3d11.h>
#	include <pk_particles/include/Updaters/D3D12/updater_d3d12.h>
#	include <pk_particles/include/Updaters/D3D12U/updater_d3d12U.h>

#	include <pk_rhi/include/D3D11/D3D11Texture.h>
#	include <pk_rhi/include/D3D12/D3D12Texture.h>
#	include <pk_rhi/include/D3D11/D3D11GpuBuffer.h>
#	include <pk_rhi/include/D3D12/D3D12GpuBuffer.h>
#	include <pk_rhi/include/D3DCommon/D3DPopcornEnumConversion.h>
#endif // (PK_COMPILER_BUILD_COMPILER_D3D != 0)

#include <pk_rhi/include/interfaces/SConstantSetLayout.h>
#include <PK-SampleLib/ShaderDefinitions/SampleLibShaderDefinitions.h>

#include <pk_kernel/include/kr_memoryviews_utils.h>

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

namespace	SimInterfaces
{

#if (PK_COMPILER_BUILD_COMPILER_D3D != 0)
	namespace	D3D
	{
		//----------------------------------------------------------------------------
		//
		//	Compute shader generation (only required to be available for baking)
		//
		//----------------------------------------------------------------------------

		bool	_Declare_Texture2D(CStringId name, u32 regslot, CString &out)
		{
			out += CString::Format("Texture2D		%s : register(t%d);\n", name.ToStringData(), regslot);
			return true;
		}

		bool	_Declare_SceneInfos(CStringId name, u32 regslot, CString &out)
		{
			(void)name;
			// Use SampleLib's shader generation wrapper, to make sure shader code remains up to date with editor code.
			RHI::SConstantSetLayout	sceneInfoLayout;
			if (!PK_VERIFY(PKSample::CreateSceneInfoConstantLayout(sceneInfoLayout)))
				return false;
			PKSample::CHLSLShaderGenerator	hlslGenerator;

			// Assume scene info layout only contains one constant buffer
			if (!PK_VERIFY(sceneInfoLayout.m_Constants.Count() == 1) ||
				!PK_VERIFY(sceneInfoLayout.m_Constants[0].m_Type == RHI::TypeConstantBuffer))
				return false;
			sceneInfoLayout.m_Constants[0].m_ConstantBuffer.m_PerBlockBinding = regslot;

			out += hlslGenerator.GenConstantSets(TMemoryView<const RHI::SConstantSetLayout>(sceneInfoLayout), RHI::ComputeShaderStage);
			return true;
		}

		bool	_Declare_Sampler_HLSL(CStringId name, u32 regslot, CString &out)
		{
			out += CString::Format(	"SamplerState		%s : register(s%d);\n", name.ToStringData(), regslot);
			return true;
		}

		// Data shared by several sim interface EmitBuiltin functions to avoid re-defining the same inputs several times
		bool	EmitBuiltin_SampleData(CLinkerGPU::SExternalFunctionEmitArgs &args)
		{
			// Declare common bindings required by shaders (see where EmitBuiltin_SampleData is referenced)
			CLinkerGPU::SExternalFunctionEmitArgs::SDeclInput	inputSceneInfos(CStringId("SceneInfos"), CLinkerGPU::SExternalFunctionEmitArgs::SDeclInput::ConstantBuffer, _Declare_SceneInfos);
			if (!PK_VERIFY(args.m_ExternalShaderInput.PushBack(inputSceneInfos).Valid()))
				return false;

			switch (args.m_Shaderlang)
			{
			case	CLinkerGPU::SExternalFunctionEmitArgs::HLSL_5_0: // D3D11 GPU sim
			case	CLinkerGPU::SExternalFunctionEmitArgs::HLSL_6_0: // D3D12 GPU sim
				{
					CLinkerGPU::SExternalFunctionEmitArgs::SDeclInput	inputSampler(CStringId("SceneTexturesSampler"), CLinkerGPU::SExternalFunctionEmitArgs::SDeclInput::Sampler, _Declare_Sampler_HLSL);
					if (!PK_VERIFY(args.m_ExternalShaderInput.PushBack(inputSampler).Valid()))
						return false;
				}
				break;
			default:
				PK_ASSERT_NOT_REACHED();
				return false;
			}
			return true;
		}

		// Scene intersect with scene depth map
		bool	EmitBuiltin_ProjectToPosition(CLinkerGPU::SExternalFunctionEmitArgs &args)
		{
			// Declare the unique binding required by this shader
			CLinkerGPU::SExternalFunctionEmitArgs::SDeclInput	inputSceneDepth(CStringId("SceneDepth"), CLinkerGPU::SExternalFunctionEmitArgs::SDeclInput::ShaderResource, _Declare_Texture2D);
			if (!PK_VERIFY(args.m_ExternalShaderInput.PushBack(inputSceneDepth).Valid()))
				return false;

			// Shader content
			args.m_DstKernelSource += g_GPUSimInterface_GBuffer_ProjectToPosition_d3d_data;
			return true;
		}

		// Scene intersect with scene depth map
		bool	EmitBuiltin_ProjectToNormal(CLinkerGPU::SExternalFunctionEmitArgs &args)
		{
			// Declare the unique binding required by this shader
			CLinkerGPU::SExternalFunctionEmitArgs::SDeclInput	inputSceneNormal(CStringId("SceneNormal"), CLinkerGPU::SExternalFunctionEmitArgs::SDeclInput::ShaderResource, _Declare_Texture2D);
			if (!PK_VERIFY(args.m_ExternalShaderInput.PushBack(inputSceneNormal).Valid()))
				return false;

			// Shader content
			args.m_DstKernelSource += g_GPUSimInterface_GBuffer_ProjectToNormal_d3d_data;
			return true;
		}

	} // namespace D3D
#endif // (PK_COMPILER_BUILD_COMPILER_D3D != 0)

	//----------------------------------------------------------------------------
	//
	//	Compute shader dispatch bindings (required at runtime, called on async threads during PK simulation)
	//
	//----------------------------------------------------------------------------

	bool	Bind_ConstantBuffer(const RHI::PGpuBuffer &buffer, const SLinkGPUContext &context)
	{
		(void)context;
		if (!PK_VERIFY(buffer != null))
			return false;

#if (PK_PARTICLES_UPDATER_USE_D3D11 != 0)
		if (context.m_ContextType == ContextD3D11)
		{
			const SBindingContextD3D11	&d3d11Context = context.ToD3D11();
			RHI::PD3D11GpuBuffer		d3d11Buffer = CastD3D11(buffer);
			ID3D11Buffer				*_d3d11Buffer = d3d11Buffer->D3D11GetBuffer();

			if (!PK_VERIFY(_d3d11Buffer != null))
				return false;
			d3d11Context.m_DeviceContext->CSSetConstantBuffers(context.m_Location, 1, &_d3d11Buffer);
			return true;
		}
#endif // (PK_PARTICLES_UPDATER_USE_D3D11 != 0)
#if (PK_PARTICLES_UPDATER_USE_D3D12U != 0)
		if (context.m_ContextType == ContextD3D12U)
		{
			SBindingContextD3D12U	&d3d12UContext = const_cast<SBindingContextD3D12U&>(context.ToD3D12U());
			SBindingHeapD3D12U		&bindingHeap = d3d12UContext.m_Heap;
			RHI::PD3D12GpuBuffer	d3d12Buffer = CastD3D12(buffer);
			ID3D12Resource			*_d3d12UBuffer = d3d12Buffer->D3D12GetResource();

			if (!PK_VERIFY(_d3d12UBuffer != null))
				return false;
			return	bindingHeap.BindConstantBufferView(d3d12UContext.m_Device, _d3d12UBuffer, context.m_Location) &&
					bindingHeap.KeepResourceReference(_d3d12UBuffer);
		}
#endif // (PK_PARTICLES_UPDATER_USE_D3D12U != 0)
#if (PK_PARTICLES_UPDATER_USE_D3D12 != 0)
		if (context.m_ContextType == ContextD3D12)
		{
			SBindingContextD3D12	&d3d12Context = const_cast<SBindingContextD3D12&>(context.ToD3D12());
			SBindingHeapD3D12		&bindingHeap = d3d12Context.m_Heap;
			RHI::PD3D12GpuBuffer	d3d12Buffer = CastD3D12(buffer);
			ID3D12Resource			*_d3d12Buffer = d3d12Buffer->D3D12GetResource();

			if (!PK_VERIFY(_d3d12Buffer != null))
				return false;
			return	bindingHeap.BindConstantBufferView(d3d12Context.m_Device, _d3d12Buffer, context.m_Location) &&
					bindingHeap.KeepResourceReference(_d3d12Buffer);
		}
#endif // (PK_PARTICLES_UPDATER_USE_D3D12 != 0)
		PK_ASSERT_NOT_REACHED();
		return false;
	}

	//----------------------------------------------------------------------------

	bool	Bind_Texture(const RHI::PTexture &texture, const SLinkGPUContext &context)
	{
		(void)context;
		if (!PK_VERIFY(texture != null))
			return false;

#if (PK_PARTICLES_UPDATER_USE_D3D11 != 0)
		if (context.m_ContextType == ContextD3D11)
		{
			const SBindingContextD3D11	&d3d11Context = context.ToD3D11();
			RHI::PD3D11Texture			d3d11Texture = CastD3D11(texture);
			ID3D11ShaderResourceView	*_d3d11Texture = d3d11Texture->D3D11GetView();

			if (!PK_VERIFY(_d3d11Texture != null))
				return false;
			d3d11Context.m_DeviceContext->CSSetShaderResources(context.m_Location, 1, &_d3d11Texture);
			return true;
		}
#endif // (PK_PARTICLES_UPDATER_USE_D3D11 != 0)
#if (PK_PARTICLES_UPDATER_USE_D3D12U != 0)
		if (context.m_ContextType == ContextD3D12U)
		{
			SBindingContextD3D12U	&d3d12UContext = const_cast<SBindingContextD3D12U&>(context.ToD3D12U());
			SBindingHeapD3D12U		&bindingHeap = d3d12UContext.m_Heap;
			RHI::PD3D12Texture		d3d12Texture = CastD3D12(texture);
			ID3D12Resource			*_d3d12UTexture = d3d12Texture->D3D12GetResource();

			if (!PK_VERIFY(_d3d12UTexture != null) ||
				!PK_VERIFY(d3d12UContext.m_DynamicBarrier != null))
				return false;

			// Note: can be used to transition a resource before/after the associated CS is dispatched
#if 0
			const D3D12_RESOURCE_STATES	stateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			const D3D12_RESOURCE_STATES	stateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
			if (!PK_VERIFY(d3d12Context.m_DynamicBarrier->AddBarrier(_d3d12UTexture, stateBefore, stateAfter)) ||
				!PK_VERIFY(d3d12Context.m_DynamicBarrier_PostUpdate->AddBarrier(_d3d12UTexture, stateAfter, stateBefore)))
				return false;
#endif

			const DXGI_FORMAT	dxgiFormat = RHI::D3DConversion::PopcornToD3DPixelFormat(texture->GetFormat());
			return	bindingHeap.BindTextureShaderResourceView(d3d12UContext.m_Device, _d3d12UTexture, context.m_Location, dxgiFormat, D3D12_SRV_DIMENSION_TEXTURE2D, D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING) &&
					bindingHeap.KeepResourceReference(_d3d12UTexture);
		}
#endif // (PK_PARTICLES_UPDATER_USE_D3D12U != 0)
#if (PK_PARTICLES_UPDATER_USE_D3D12 != 0)
		if (context.m_ContextType == ContextD3D12)
		{
			SBindingContextD3D12	&d3d12Context = const_cast<SBindingContextD3D12&>(context.ToD3D12());
			SBindingHeapD3D12		&bindingHeap = d3d12Context.m_Heap;
			RHI::PD3D12Texture		d3d12Texture = CastD3D12(texture);
			ID3D12Resource			*_d3d12Texture = d3d12Texture->D3D12GetResource();

			if (!PK_VERIFY(_d3d12Texture != null) ||
				!PK_VERIFY(d3d12Context.m_DynamicBarrier != null))
				return false;

			// Note: can be used to transition a resource before/after the associated CS is dispatched
#if 0
			const D3D12_RESOURCE_STATES	stateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			const D3D12_RESOURCE_STATES	stateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
			if (!PK_VERIFY(d3d12Context.m_DynamicBarrier->AddBarrier(_d3d12Texture, stateBefore, stateAfter)) ||
				!PK_VERIFY(d3d12Context.m_DynamicBarrier_PostUpdate->AddBarrier(_d3d12Texture, stateAfter, stateBefore)))
				return false;
#endif

			const DXGI_FORMAT	dxgiFormat = RHI::D3DConversion::PopcornToD3DPixelFormat(texture->GetFormat());
			return	bindingHeap.BindTextureShaderResourceView(d3d12Context.m_Device, _d3d12Texture, context.m_Location, dxgiFormat, D3D12_SRV_DIMENSION_TEXTURE2D, D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING) &&
					bindingHeap.KeepResourceReference(_d3d12Texture);
		}
#endif // (PK_PARTICLES_UPDATER_USE_D3D12 != 0)
		PK_ASSERT_NOT_REACHED();
		return false;
	}

	//----------------------------------------------------------------------------

	bool	Bind_Sampler(CStringId mangledName, const SLinkGPUContext &context)
	{
		(void)mangledName; (void)context;
		PK_ASSERT(mangledName.ToStringView() == "SceneTexturesSampler");

#if (PK_PARTICLES_UPDATER_USE_D3D11 != 0)
		if (context.m_ContextType == ContextD3D11)
		{
			SBindingContextD3D11	&d3d11Context = const_cast<SBindingContextD3D11&>(context.ToD3D11());
			ID3D11Device			*device = null;
			d3d11Context.m_DeviceContext->GetDevice(&device);
			if (!PK_VERIFY(device != null))
				return false;

			D3D11_SAMPLER_DESC	desc = {};
			desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
			desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
			desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
			desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
			desc.MaxAnisotropy = 1;
			desc.ComparisonFunc = D3D11_COMPARISON_NEVER;

			ID3D11SamplerState	*samplerState = null;
			HRESULT				errorCode = device->CreateSamplerState(&desc, &samplerState);
			if (!PK_VERIFY(errorCode == S_OK))
				return false;
			d3d11Context.m_DeviceContext->CSSetSamplers(context.m_Location, 1, &samplerState);
			return true;
		}
#endif // (PK_PARTICLES_UPDATER_USE_D3D11 != 0)
#if (PK_PARTICLES_UPDATER_USE_D3D12 != 0)
		if (context.m_ContextType == ContextD3D12)
		{
			SBindingContextD3D12	&d3d12Context = const_cast<SBindingContextD3D12&>(context.ToD3D12()); //......
			SBindingHeapD3D12		&bindingHeap = d3d12Context.m_Heap;

			D3D12_SAMPLER_DESC	desc = {};
			desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			desc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
			desc.MipLODBias = 0;
			desc.MaxAnisotropy = 1;
			desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
			desc.MinLOD = 0;
			desc.MaxLOD = 0;

			return bindingHeap.BindSampler(d3d12Context.m_Device, desc, context.m_Location);
		}
#endif // (PK_PARTICLES_UPDATER_USE_D3D12 != 0)
		PK_ASSERT_NOT_REACHED();
		return false;
	}

	//----------------------------------------------------------------------------
	//
	//	Sim interface definition (required at runtime, when registration of the sim interface occurs)
	//
	//----------------------------------------------------------------------------

	static bool	_BuildSimInterfaceDef_ProjectToPosition(SSimulationInterfaceDefinition &def)
	{
		// Build the graphics API / backend independent definition of the sim interface
		PK_ASSERT(def.m_Inputs.Empty() && def.m_Outputs.Empty());

		// Build the sim interface definition
		def.m_FnNameBase = "GBuffer_ProjectToPosition";
		def.m_Traits = Compiler::F_StreamedReturnValue | Compiler::F_Pure;

		// Pass in scene, to make sure our function does not get constant folded if input World Position is constant: our output isn't.
		def.m_Flags = SSimulationInterfaceDefinition::Flags_Context_Scene;

		// "Space" of the inputs/outputs
		const Compiler::STypeMetaData	kMetaData_Full = u32(MetaData_XForm_World | MetaData_XForm_Full);

		// Declare all inputs
		if (!def.m_Inputs.PushBack(SSimulationInterfaceDefinition::SValueIn("Position", Nodegraph::DataType_Float3, Compiler::Attribute_Stream, kMetaData_Full)).Valid())
			return false;

		// Declare all outputs
		if (!def.m_Outputs.PushBack(SSimulationInterfaceDefinition::SValueOut("WorldPosition", Nodegraph::DataType_Float3, Compiler::Attribute_Stream, kMetaData_Full)).Valid())
			return false;

		return true;
	}

	//----------------------------------------------------------------------------

#if	(PK_COMPILER_BUILD_COMPILER != 0)
	static bool		_CustomRangeBuild_ProjectToNormal(	const CCompilerIR						*ir,
														Compiler::IR::CRangeAnalysis			*ranges,
														const Compiler::IR::SOptimizerConfig	&optimizerConfig,
														const Compiler::IR::SOp_Generic			&op,
														Range::SConstantRange					&outputRange)
	{
		(void)ranges; (void)optimizerConfig; (void)ir;
		if (!PK_VERIFY(op.m_Opcode == Compiler::IR::Opcode_FunctionCall) ||
			!PK_VERIFY(op.m_Inputs.Count() == 2))	// SI_GBuffer_ProjectToNormal(float3 position, int4 sceneCtx)
			return false;

		// Assume the sample normal implementation returns normalized normals: lets the optimizer remove calls to 'normalize' for example
		outputRange = Range::SConstantRange::kF32_m1p1;	// ]-1,+1[
		return true;
	}
#endif // (PK_COMPILER_BUILD_COMPILER != 0)

	//----------------------------------------------------------------------------

	static bool	_BuildSimInterfaceDef_ProjectToNormal(SSimulationInterfaceDefinition &def)
	{
		// Build the graphics API / backend independent definition of the sim interface
		PK_ASSERT(def.m_Inputs.Empty() && def.m_Outputs.Empty());

		// Build the sim interface definition
		def.m_FnNameBase = "GBuffer_ProjectToNormal";
		def.m_Traits = Compiler::F_StreamedReturnValue | Compiler::F_Pure;

		// Pass in scene, to make sure our function does not get constant folded if input World Position is constant: our output isn't.
		def.m_Flags = SSimulationInterfaceDefinition::Flags_Context_Scene;

#if	(PK_COMPILER_BUILD_COMPILER != 0)
		// Step used by the particle compiler / optimizer: defines the range of values of the WorldNormal output.
		def.m_OptimizerDefs.m_FnRangeBuildPtr = &_CustomRangeBuild_ProjectToNormal;
#endif

		// "Space" of the inputs/outputs
		const Compiler::STypeMetaData	kMetaData_Position = u32(MetaData_XForm_World | MetaData_XForm_Full);
		const Compiler::STypeMetaData	kMetaData_Direction = u32(MetaData_XForm_World | MetaData_XForm_Direction);

		// Declare all inputs
		if (!def.m_Inputs.PushBack(SSimulationInterfaceDefinition::SValueIn("Position", Nodegraph::DataType_Float3, Compiler::Attribute_Stream, kMetaData_Position)).Valid())
			return false;

		// Declare all outputs
		if (!def.m_Outputs.PushBack(SSimulationInterfaceDefinition::SValueOut("WorldNormal", Nodegraph::DataType_Float3, Compiler::Attribute_Stream, kMetaData_Direction)).Valid())
			return false;

		return true;
	}

	//----------------------------------------------------------------------------
	//
	//	Sim interface CPU implementation (required at runtime, when registration of the sim interface occurs)
	//
	//----------------------------------------------------------------------------

	void	_ProjectToPosition(	const TStridedMemoryView<CFloat3>		&dstWorldPositions,
								const TStridedMemoryView<const CFloat3>	&srcPositions,
								const CParticleContextScene				*sceneCtx)
	{
		(void)sceneCtx;
		// Forward positions
		Mem::CopyStreamToStream(dstWorldPositions, srcPositions);
	}

	//----------------------------------------------------------------------------

	void	_ProjectToNormal(	const TStridedMemoryView<CFloat3>		&dstWorldNormals,
								const TStridedMemoryView<const CFloat3>	&srcPositions,
								const CParticleContextScene				*sceneCtx)
	{
		(void)sceneCtx;
		// Clear output normals
		(void)srcPositions;
		Mem::ClearStream(dstWorldNormals);
	}

	//----------------------------------------------------------------------------
	//
	//	Linker binding
	//
	//----------------------------------------------------------------------------

	bool	BindGBufferSamplingSimInterfaces(const CString &coreLibPath, HBO::CContext *context)
	{
		(void)context; (void)coreLibPath;
		// Build the sim interface definition (backend independent)
		SSimulationInterfaceDefinition	defProjectToPosition;
		SSimulationInterfaceDefinition	defProjectToNormal;
		if (!PK_VERIFY(_BuildSimInterfaceDef_ProjectToPosition(defProjectToPosition)) ||
			!PK_VERIFY(_BuildSimInterfaceDef_ProjectToNormal(defProjectToNormal)))
			return false;

		const CString			simInterfacePath = coreLibPath / "PopcornFXCore/Templates/Utils.pkfx";
		const CStringUnicode	simInterfaceNameProjectToPosition = L"GBuffer_ProjectToPosition";
		const CStringUnicode	simInterfaceNameProjectToNormal = L"GBuffer_ProjectToNormal";

#	if	(PK_COMPILER_BUILD_COMPILER != 0)
		// Make sure the definition is valid, and can be resolved (only available when the source template file is available).
		if (!PK_VERIFY(CSimulationInterfaceMapper::CheckBinding(simInterfacePath, simInterfaceNameProjectToPosition, defProjectToPosition, context)) ||
			!PK_VERIFY(CSimulationInterfaceMapper::CheckBinding(simInterfacePath, simInterfaceNameProjectToNormal, defProjectToNormal, context)))
			return false;

		// Bind the definition to the sim interface template path in your project
		CSimulationInterfaceMapper	*simInterfaceMapper = CSimulationInterfaceMapper::DefaultMapper();
		if (!simInterfaceMapper->Bind(simInterfacePath, simInterfaceNameProjectToPosition, defProjectToPosition) ||
			!simInterfaceMapper->Bind(simInterfacePath, simInterfaceNameProjectToNormal, defProjectToNormal))
		{
			CLog::Log(PK_INFO, "Failed binding GPU sim interfaces");
		}
#	endif

		// CPU sim implementation fallback (as soon as we bind a sim interface, all supported backends must be implemented)
		PopcornFX::Compiler::SBinding	linkBindingProjectToPosition;
		PopcornFX::Compiler::SBinding	linkBindingProjectToNormal;
		PopcornFX::Compiler::Binders::Bind(linkBindingProjectToPosition, &_ProjectToPosition);
		PopcornFX::Compiler::Binders::Bind(linkBindingProjectToNormal, &_ProjectToNormal);

		if (!CLinkerCPU::Bind(defProjectToPosition.GetCallNameMangledCPU(0), linkBindingProjectToPosition) ||
			!CLinkerCPU::Bind(defProjectToNormal.GetCallNameMangledCPU(0), linkBindingProjectToNormal))
		{
			CLog::Log(PK_ERROR, "Failed linking CPU sim implementation for GPU sim interfaces");
			return false;
		}

		const CStringId	backendName_D3D("GPU_D3D");
#	if	(PK_COMPILER_BUILD_COMPILER_D3D != 0)
		// GPU shader implementation: emit the shader code for that sim interface. Only required when baking
		const CStringId		SPID_Ext_sampleData("_sample_data");
		// Dependencies shared by ProjectToPosition and ProjectToNormal (mainly, the SceneInfo constant buffer, and the shared texture sampler)
		const CStringId		deps[] =
		{
			SPID_Ext_sampleData
		};
		const CString		mangledCallGPUProjectToPosition = defProjectToPosition.GetCallNameMangledGPU(0);
		const CString		mangledCallGPUProjectToNormal = defProjectToNormal.GetCallNameMangledGPU(0);
		if (!CLinkerGPU::Bind(backendName_D3D, SPID_Ext_sampleData, &D3D::EmitBuiltin_SampleData) ||
			!CLinkerGPU::Bind(backendName_D3D, CStringId(mangledCallGPUProjectToPosition), &D3D::EmitBuiltin_ProjectToPosition, false, deps) ||
			!CLinkerGPU::Bind(backendName_D3D, CStringId(mangledCallGPUProjectToNormal), &D3D::EmitBuiltin_ProjectToNormal, false, deps))
		{
			CLog::Log(PK_ERROR, "Failed emitting GPU sim interfaces");
			return false;
		}
#	endif // (PK_COMPILER_BUILD_COMPILER_D3D != 0)

		if (!CLinkerGPU::BindLink(backendName_D3D, CStringId("SceneTexturesSampler"), &Bind_Sampler))
		{
			CLog::Log(PK_ERROR, "Failed linking GPU sim interface bindings");
			return false;
		}

		return true;
	}

	//----------------------------------------------------------------------------

	void	UnbindGBufferSamplingSimInterfaces(const CString &coreLibPath)
	{
		(void)coreLibPath;
		const CString				simInterfacePath = coreLibPath / "PopcornFXCore/Templates/Utils.pkfx";
		const CStringUnicode		simInterfaceNameProjectToPosition = L"GBuffer_ProjectToPosition";
		const CStringUnicode		simInterfaceNameProjectToNormal = L"GBuffer_ProjectToNormal";

		// Build the sim interface definition
		SSimulationInterfaceDefinition	defProjectToPosition;
		SSimulationInterfaceDefinition	defProjectToNormal;
		if (!PK_VERIFY(_BuildSimInterfaceDef_ProjectToPosition(defProjectToPosition)) ||
			!PK_VERIFY(_BuildSimInterfaceDef_ProjectToNormal(defProjectToNormal)))
			return;

#if	(PK_COMPILER_BUILD_COMPILER != 0)
		CSimulationInterfaceMapper	*simInterfaceMapper = CSimulationInterfaceMapper::DefaultMapper();
		// If sim interface is not there, abort
		if (simInterfaceMapper->Map(simInterfacePath, simInterfaceNameProjectToPosition) == null ||
			simInterfaceMapper->Map(simInterfacePath, simInterfaceNameProjectToNormal) == null)
			return;

		// Unbind the sim interface.
		// All subsequent effect compilations will not see it as a sim interface
		// and will compile like in the editor: using the default implementation contained
		// in the sim interface template node.
		simInterfaceMapper->Unbind(simInterfacePath, simInterfaceNameProjectToPosition);
		simInterfaceMapper->Unbind(simInterfacePath, simInterfaceNameProjectToNormal);
#endif // (PK_COMPILER_BUILD_COMPILER != 0)

		// Unbind from GPU linker
		const CStringId	backendName_D3D("GPU_D3D");
		CLinkerGPU::UnbindLink(backendName_D3D, CStringId("SceneDepth"));
		CLinkerGPU::UnbindLink(backendName_D3D, CStringId("SceneNormal"));
		CLinkerGPU::UnbindLink(backendName_D3D, CStringId("SceneInfos"));
		CLinkerGPU::UnbindLink(backendName_D3D, CStringId("SceneTexturesSampler"));

		// Unbind from CPU linker
		CLinkerCPU::Unbind(defProjectToPosition.GetCallNameMangledCPU(0));
		CLinkerCPU::Unbind(defProjectToNormal.GetCallNameMangledCPU(0));
	}

} // namespace SimInterfaces

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
