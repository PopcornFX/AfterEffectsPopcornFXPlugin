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

#include <ApiContextConfig.h>

#include "ShaderLoader.h"
#include "SampleUtils.h"
#include "pk_rhi/include/ShaderConstantBindingGenerator.h"
#include "pk_rhi/include/interfaces/SShaderBindings.h"
#include <pk_kernel/include/kr_refcounted_buffer.h>
#include <pk_toolkit/include/pk_toolkit_process.h>

#include <PK-SampleLib/ShaderGenerator/HLSLShaderGenerator.h>
#include <PK-SampleLib/ShaderGenerator/GLSLShaderGenerator.h>
#include <PK-SampleLib/ShaderGenerator/VulkanShaderGenerator.h>
#include <PK-SampleLib/ShaderGenerator/MetalShaderGenerator.h>
#if	(PK_BUILD_WITH_PSSL_GENERATOR != 0)
#	include <PK-SampleLib/ShaderGenerator/PSSL/PSSLShaderGenerator.h>
#endif // (PK_BUILD_WITH_PSSL_GENERATOR != 0)

#define	PATH_TO_RESOURCES				"../Resources/"

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

bool	CShaderLoader::Release()
{
	m_VertexModules.Clear();
	m_GeometryModules.Clear();
	m_FragmentModules.Clear();
	m_ComputeModules.Clear();
	m_Programs.Clear();
	return true;
}

//----------------------------------------------------------------------------

bool	CShaderLoader::LoadShader(	RHI::SComputeState		&pipelineState,
									const CString			&shaderPath,
									const RHI::PApiManager	&apiManager,
									IFileSystem				*controller /*= null*/)
{
	RHI::ShaderConstantBindingGenerator::GenerateBindingsForApi(apiManager->ApiName(), pipelineState.m_ShaderBindings);
//	if (preprocessDescription != null)
//		RHI::ShaderConstantBindingGenerator::GenerateBindingsForApi(apiManager->ApiName(), preprocessDescription->m_Bindings);

	const CDigestMD5	&computeHash = pipelineState.m_ShaderBindings.Hash(RHI::ComputeShaderStage);
	CGuid				loadedProgramId;

	// Check if the program has already been loaded:
	for (u32 i = 0; i < m_Programs.Count(); ++i)
	{
		if (m_Programs[i].HasSamePathThan(shaderPath))
		{
			PK_FOREACH(program, m_Programs[i].m_Programs)
			{
				if (computeHash == program->m_ComputeHash)
				{
					PK_ONLY_IF_ASSERTS(
						// Check hash collisions
						if (!program->m_ShaderBindingsForDebug.AreBindingsEqual(pipelineState.m_ShaderBindings, RHI::ComputeShaderStage))
						{
							CLog::Log(PK_ERROR, "Hash collision detected for the shader CS: \"%s\"", shaderPath.Data());
							PK_ASSERT_NOT_REACHED_MESSAGE("Hash collision detected");
							return false;
						}
					);
					pipelineState.m_ShaderProgram = program->m_Program;
					return true;
				}
			}
			loadedProgramId = i;
			break;
		}
	}

	RHI::PShaderModule	computeModule = FindOrLoadShaderModule(m_ComputeModules, shaderPath, RHI::ComputeShaderStage, apiManager, pipelineState.m_ShaderBindings, computeHash, controller);

	if (computeModule == null)
		return false;

	RHI::PShaderProgram		program = apiManager->CreateShaderProgram(RHI::SRHIResourceInfos("PK-RHI Shader Program"));
	if (program->CreateFromShaderModule(computeModule) == false)
		return false;

	if (!loadedProgramId.Valid())
	{
		loadedProgramId = m_Programs.PushBack();
		if (!loadedProgramId.Valid())
			return false;
	}

	{
		SLoadedProgram	&loadedProgram = m_Programs[loadedProgramId];
		if (!loadedProgram.m_Programs.PushBack().Valid())
		{
			m_Programs.Remove(loadedProgramId);
			return false;
		}
		loadedProgram.m_ComputePath = shaderPath;
		loadedProgram.m_Programs.Last().m_ComputeHash = computeHash;
		PK_ONLY_IF_ASSERTS(
			loadedProgram.m_Programs.Last().m_ShaderBindingsForDebug = pipelineState.m_ShaderBindings;
		);
		loadedProgram.m_Programs.Last().m_Program = program;
	}

	pipelineState.m_ShaderProgram = program;
	return true;

}

//----------------------------------------------------------------------------

bool	CShaderLoader::LoadShader(	RHI::SRenderState		&renderState,
									const SShadersPaths		&shadersPaths,
									const RHI::PApiManager	&apiManager,
									IFileSystem				*controller /*= null*/)
{
	RHI::ShaderConstantBindingGenerator::GenerateBindingsForApi(apiManager->ApiName(), renderState.m_ShaderBindings);
//	if (preprocessDescription != null)
//		RHI::ShaderConstantBindingGenerator::GenerateBindingsForApi(apiManager->ApiName(), preprocessDescription->m_Bindings);

	const CDigestMD5		&vertexHash = renderState.m_ShaderBindings.Hash(RHI::VertexShaderStage);
	const CDigestMD5		&geometryHash = renderState.m_ShaderBindings.Hash(RHI::GeometryShaderStage);
	const CDigestMD5		&fragmentHash = renderState.m_ShaderBindings.Hash(RHI::FragmentShaderStage);
	CGuid					loadedProgramId;

	// Check if the program has already been loaded:
	for (u32 i = 0; i < m_Programs.Count(); ++i)
	{
		if (m_Programs[i].HasSamePathThan(shadersPaths))
		{
			PK_FOREACH(program, m_Programs[i].m_Programs)
			{
				if (vertexHash == program->m_VertexHash && fragmentHash == program->m_FragmentHash && geometryHash == program->m_GeometryHash)
				{
					PK_ONLY_IF_ASSERTS(
						// Check hash collisions
						if (!program->m_ShaderBindingsForDebug.AreBindingsEqual(renderState.m_ShaderBindings, RHI::VertexShaderStage) ||
							!program->m_ShaderBindingsForDebug.AreBindingsEqual(renderState.m_ShaderBindings, RHI::FragmentShaderStage) ||
							!program->m_ShaderBindingsForDebug.AreBindingsEqual(renderState.m_ShaderBindings, RHI::GeometryShaderStage))
						{
							CLog::Log(	PK_ERROR,
										"Hash collision detected for the shader VS: \"%s\" GS: \"%s\" PS: \"%s\"",
										shadersPaths.m_Vertex.Data(),
										shadersPaths.m_Geometry.Data(),
										shadersPaths.m_Fragment.Data());
							PK_ASSERT_NOT_REACHED_MESSAGE("Hash collision detected");
							return false;
						}
					);
					renderState.m_ShaderProgram = program->m_Program;
					return true;
				}
			}
			loadedProgramId = i;
			break;
		}
	}

	RHI::PShaderModule	vertexModule = FindOrLoadShaderModule(m_VertexModules, shadersPaths.m_Vertex, RHI::VertexShaderStage, apiManager, renderState.m_ShaderBindings, vertexHash, controller);
	RHI::PShaderModule	geometryModule = null;
	if (shadersPaths.HasGeometry())
		geometryModule = FindOrLoadShaderModule(m_GeometryModules, shadersPaths.m_Geometry, RHI::GeometryShaderStage, apiManager, renderState.m_ShaderBindings, geometryHash, controller);
	RHI::PShaderModule	fragmentModule = FindOrLoadShaderModule(m_FragmentModules, shadersPaths.m_Fragment, RHI::FragmentShaderStage, apiManager, renderState.m_ShaderBindings, fragmentHash, controller);

	if (vertexModule == null || fragmentModule == null || (shadersPaths.HasGeometry() && geometryModule == null))
		return false;

	RHI::PShaderProgram		program = apiManager->CreateShaderProgram(RHI::SRHIResourceInfos("PK-RHI Shader Program"));
	if (program->CreateFromShaderModules(vertexModule, geometryModule, fragmentModule) == false)
		return false;

	if (!loadedProgramId.Valid())
	{
		loadedProgramId = m_Programs.PushBack();
		if (!loadedProgramId.Valid())
			return false;
	}

	{
		SLoadedProgram	&loadedProgram = m_Programs[loadedProgramId];
		if (!loadedProgram.m_Programs.PushBack().Valid())
		{
			m_Programs.Remove(loadedProgramId);
			return false;
		}
		loadedProgram.m_FragmentPath = shadersPaths.m_Fragment;
		loadedProgram.m_VertexPath = shadersPaths.m_Vertex;
		loadedProgram.m_GeometryPath = shadersPaths.m_Geometry;
		loadedProgram.m_Programs.Last().m_VertexHash = vertexHash;
		loadedProgram.m_Programs.Last().m_FragmentHash = fragmentHash;
		loadedProgram.m_Programs.Last().m_GeometryHash = geometryHash;
		PK_ONLY_IF_ASSERTS(
			loadedProgram.m_Programs.Last().m_ShaderBindingsForDebug = renderState.m_ShaderBindings;
		);
		loadedProgram.m_Programs.Last().m_Program = program;
	}

	renderState.m_ShaderProgram = program;
	return true;
}

//----------------------------------------------------------------------------

RHI::PShaderModule	CShaderLoader::FindOrLoadShaderModule(	TArray<SLoadedModule>		&moduleLibrary,
															const CString				&shaderPath,
															RHI::EShaderStage			stage,
															const RHI::PApiManager		&apiManager,
															const RHI::SShaderBindings	&bindings,
															const CDigestMD5			&hash,
															IFileSystem					*controller)
{
	(void)bindings;
	for (u32 i = 0; i < moduleLibrary.Count(); ++i)
	{
		SLoadedModule	&loadedModule = moduleLibrary[i];
		if (loadedModule.m_Path == shaderPath)
		{
			for (u32 j = 0; j < loadedModule.m_Modules.Count(); ++j)
			{
				SLoadedModule::SModule	&currentModule = loadedModule.m_Modules[j];
				if (hash == currentModule.m_Hash)
				{
					PK_ONLY_IF_ASSERTS(
						// Check hash collisions
						if (!currentModule.m_ShaderBindingsForDebug.AreBindingsEqual(bindings, stage))
						{
							CLog::Log(PK_ERROR, "Hash collision detected for the shader \"%s\"", shaderPath.Data());
							PK_ASSERT_NOT_REACHED_MESSAGE("Hash collision detected");
							return null;
						}
					);
					return currentModule.m_Module;
				}
			}

			RHI::PShaderModule	module = LoadShaderModule(shaderPath, hash, apiManager, stage, controller);
			if (module == null)
				return null;

			if (!loadedModule.m_Modules.PushBack().Valid())
				return null;
			loadedModule.m_Modules.Last().m_Module = module;
			loadedModule.m_Modules.Last().m_Hash = hash;
			PK_ONLY_IF_ASSERTS(
				loadedModule.m_Modules.Last().m_ShaderBindingsForDebug = bindings;
			);
			return module;
		}
	}

	RHI::PShaderModule	module = LoadShaderModule(shaderPath, hash, apiManager, stage, controller);
	if (module == null)
		return null;
	if (!moduleLibrary.PushBack().Valid())
		return null;
	if (!moduleLibrary.Last().m_Modules.PushBack().Valid())
	{
		moduleLibrary.PopBackAndDiscard();
		return null;
	}

	moduleLibrary.Last().m_Path = shaderPath;
	moduleLibrary.Last().m_Modules.Last().m_Hash = hash;
	PK_ONLY_IF_ASSERTS(
		moduleLibrary.Last().m_Modules.Last().m_ShaderBindingsForDebug = bindings;
	);
	moduleLibrary.Last().m_Modules.Last().m_Module = module;
	return module;
}

//----------------------------------------------------------------------------

RHI::PShaderModule	CShaderLoader::LoadShaderModule(const CString				&path,
													const CDigestMD5			&hash,
													const RHI::PApiManager		&apiManager,
													RHI::EShaderStage			stage,
													IFileSystem					*controller)
{
	if (controller == null)
		controller = m_DefaultController;
	if (controller == null)
		controller = File::DefaultFileSystem();

	char				_hashStorage[32+1];
	const CStringView	finalHash = RHI::ShaderHashToStringView(hash, _hashStorage);
	const CString		filePathNoExt = path + "." + finalHash;
	const CString		filePath = filePathNoExt + GetShaderExtensionStringFromApi(apiManager->ApiName());
#ifndef PK_RETAIL
	const CString		shaderDebugName = CFilePath::ExtractFilename(filePathNoExt);
#else
	const CString		shaderDebugName;
#endif // ifndef PK_RETAIL

	// Load the compiled shader
	RHI::PShaderModule	module = apiManager->CreateShaderModule(RHI::SRHIResourceInfos(shaderDebugName));
	PFileStream			fileView = controller->OpenStream(filePath, IFileSystem::Access_Read);
	if (fileView == null)
		fileView = controller->OpenStream(filePath, IFileSystem::Access_Read, true);

	if (fileView == null)
	{
		CLog::Log(PK_ERROR, "Could not load the shader file %s", filePath.Data());
		return null;
	}
	PRefCountedMemoryBuffer	byteCode = fileView->BufferizeToRefCountedMemoryBuffer();
	fileView->Close();

	bool	err = false;
	if (apiManager->ApiDesc().m_SupportPrecompiledShader)
		err = module->LoadFromPrecompiled(byteCode, stage);
	else
		err = module->CompileFromCode(byteCode->Data<char>(), byteCode->DataSizeInBytes(), stage);

	if (err == false)
	{
		CLog::Log(PK_ERROR, "Shader '%s' did not compile: '%s'", path.Data(), module->GetShaderModuleCreationInfo());
		if (!apiManager->ApiDesc().m_SupportPrecompiledShader)
		{
			CString				shaderCode = CString(byteCode->Data<char>(), byteCode->DataSizeInBytes());
			TArray<CString>		lines;

			shaderCode.Split('\n', lines);

			CLog::Log(PK_ERROR, "Shader code:");
			for (u32 i = 0; i < lines.Count(); ++i)
			{
				CLog::Log(PK_ERROR, "[%u]%s", i, lines[i].Data());
			}
		}
		return null;
	}
	return module;
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
