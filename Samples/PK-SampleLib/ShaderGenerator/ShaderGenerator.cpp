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

#include "ShaderGenerator.h"

#if (PK_SAMPLE_LIB_HAS_SHADER_GENERATOR != 0)

#include "ShaderGenerator/HLSLShaderGenerator.h"
#include "ShaderGenerator/GLSLShaderGenerator.h"
#include "ShaderGenerator/VulkanShaderGenerator.h"
#include "ShaderGenerator/MetalShaderGenerator.h"
#include "ShaderDefinitions/ShaderDefinitions.h"
#include "ShaderGenerator/ParticleShaderGenerator.h"

#include <ApiContextConfig.h>

#if	(PK_BUILD_WITH_PSSL_GENERATOR != 0)
#	include "ShaderGenerator/PSSL/PSSLShaderGenerator.h"
#endif // (PK_BUILD_WITH_PSSL_GENERATOR != 0)

#include <pk_rhi/include/ShaderConstantBindingGenerator.h>
#include <pk_toolkit/include/pk_toolkit_process.h>
#include <pk_base_object/include/hbo_helpers.h>

#include <PK-MCPP/pk_preprocessor.h>

#include "SampleUtils.h"

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

CString		CAbstractShaderGenerator::GenerateShader(const CString &fileContent, RHI::EShaderStage stage, const RHI::SShaderDescription &description, EShaderOptions options)
{
	PK_ASSERT_MESSAGE(description.m_Pipeline != RHI::InvalidShaderStagePipeline, "Shader pipeline is not set");

	CString		shaderCode = GenDefines(description);
	shaderCode += GenHeader(stage);

	TMemoryView<const RHI::SVertexOutput>	fragmentInput;
	if (stage == RHI::FragmentShaderStage)
	{
		switch (description.m_Pipeline)
		{
		case RHI::VsPs:
			fragmentInput = description.m_VertexOutput.View();
			break;
		case RHI::EShaderStagePipeline::VsGsPs:
			fragmentInput = description.m_GeometryOutput.m_GeometryOutput.View();
			break;
		default:
			PK_ASSERT_NOT_REACHED_MESSAGE("This function is either non implemented or you shouldn't be there");
			break;
		}
	}

	shaderCode += "\n";
	if (stage == RHI::VertexShaderStage)
	{
		shaderCode += GenVertexInputs(description.m_Bindings.m_InputAttributes);
		shaderCode += "\n";
		shaderCode += GenVertexOutputs(description.m_VertexOutput, description.m_Pipeline == RHI::VsPs);
		shaderCode += "\n";
	}
	else if (stage == RHI::GeometryShaderStage)
	{
		if (description.m_Pipeline == RHI::EShaderStagePipeline::VsGsPs)
			shaderCode += GenGeometryInputs(description.m_VertexOutput.View(), description.m_DrawMode);
		else
			PK_ASSERT_NOT_REACHED_MESSAGE("This function is either not implemented or you shouldn't be there");
		shaderCode += "\n";
		shaderCode += GenGeometryOutputs(description.m_GeometryOutput);
		shaderCode += "\n";
		shaderCode += GenGeometryEmitVertex(description.m_GeometryOutput, true); // We emit clip space vertices
		shaderCode += "\n";
		shaderCode += GenGeometryEndPrimitive(description.m_GeometryOutput);
		shaderCode += "\n";
	}
	else if (stage == RHI::FragmentShaderStage)
	{
		shaderCode += GenFragmentInputs(fragmentInput);
		shaderCode += "\n";
		shaderCode += GenFragmentOutputs(description.m_FragmentOutput);
		shaderCode += "\n";
	}
	else if (stage == RHI::ComputeShaderStage)
	{
		shaderCode += CString::Format("#define THREADGROUP_SIZE_X %u \n", description.m_DispatchThreadSize.x());
		shaderCode += CString::Format("#define THREADGROUP_SIZE_Y %u \n", description.m_DispatchThreadSize.y());
		shaderCode += CString::Format("#define THREADGROUP_SIZE_Z %u \n", description.m_DispatchThreadSize.z());
		shaderCode += GenComputeInputs();
		shaderCode += "\n";
	}
	shaderCode += GenConstantSets(description.m_Bindings.m_ConstantSets, stage);
	shaderCode += "\n";
	shaderCode += GenPushConstants(description.m_Bindings.m_PushConstants, stage);
	shaderCode += "\n";
	if (stage == RHI::ComputeShaderStage)
	{
		shaderCode += GenGroupsharedVariables(description.m_Bindings.m_GroupsharedVariables);
		shaderCode += "\n";
	}
	shaderCode += "\n";
	if (stage == RHI::VertexShaderStage)
	{
		shaderCode += ParticleShaderGenerator::GenGetMeshTransformHelper();
		shaderCode += "\n";
	}
	shaderCode += fileContent;
	shaderCode += "\n";

	TArray<CString>	funcToCall;
	// Should be inserted before calling GenAdditionalFunction()
	if (!fileContent.Empty())
	{
		if (stage == RHI::VertexShaderStage)
			funcToCall.PushBack("VertexMain");
		else if (stage == RHI::GeometryShaderStage)
			funcToCall.PushBack("GeometryMain");
		else if (stage == RHI::FragmentShaderStage)
			funcToCall.PushBack("FragmentMain");
		else if (stage == RHI::ComputeShaderStage)
			funcToCall.PushBack("ComputeMain");
	}

	shaderCode += ParticleShaderGenerator::GenAdditionalFunction(description, options, funcToCall, stage);
	shaderCode += "\n";

	switch(stage)
	{
	case RHI::VertexShaderStage:
		shaderCode += GenVertexMain(description.m_Bindings.m_InputAttributes, description.m_VertexOutput, funcToCall, description.m_Pipeline == RHI::VsPs);
		shaderCode += "\n";
		break;
	case RHI::GeometryShaderStage:
		shaderCode += GenGeometryMain(description.m_VertexOutput, description.m_GeometryOutput, funcToCall, description.m_DrawMode);
		shaderCode += "\n";
		break;
	case RHI::FragmentShaderStage:
		shaderCode += GenFragmentMain(fragmentInput, description.m_FragmentOutput, funcToCall);
		shaderCode += "\n";
		break;
	case RHI::ComputeShaderStage:
		shaderCode += GenComputeMain(description.m_DispatchThreadSize, funcToCall);
		shaderCode += "\n";
		break;
	default:
		PK_ASSERT_NOT_REACHED_MESSAGE("This function is either not implemented or you shouldn't be there");
	}

	return shaderCode;
}

//----------------------------------------------------------------------------

namespace
{
	PKSample::CGLSLShaderGenerator		glslGenerator;
	PKSample::CGLSLESShaderGenerator	glslesGenerator;
	PKSample::CVulkanShaderGenerator	vulkanGenerator;
	PKSample::CHLSLShaderGenerator		hlslGenerator;
	PKSample::CMetalShaderGenerator		metalGenerator;
#if	(PK_BUILD_WITH_PSSL_GENERATOR != 0)
	PKSample::CPSSLShaderGenerator		psslGenerator;
#endif // (PK_BUILD_WITH_PSSL_GENERATOR != 0)

	PKSample::CAbstractShaderGenerator	*shaderGenerators[RHI::__GApi_Count] =
	{
		null,				// GApi_Null,
		&glslGenerator,		// GApi_OpenGL,
		&glslesGenerator,	// GApi_OES,
		&vulkanGenerator,	// GApi_Vulkan,
#if	(PK_BUILD_WITH_PSSL_GENERATOR != 0)
		&psslGenerator,		// GApi_Orbis,
#else
		null,				// (PK_BUILD_WITH_PSSL_GENERATOR == 0)
#endif
		&hlslGenerator,		// GApi_D3D11,
		&hlslGenerator,		// GApi_D3D12,
		&metalGenerator,	// GApi_Metal
#if	(PK_BUILD_WITH_PSSL_GENERATOR != 0)
		&psslGenerator,		// GApi_UNKNOWN2,
#else
		null,				// (PK_BUILD_WITH_PSSL_GENERATOR == 0)
#endif
	};
}

//----------------------------------------------------------------------------

CShaderCompilation::CShaderCompilation(	const CString						&inputPath,
										const CString						&outputPath,
										const CString						&shaderPermutation,
										RHI::EShaderStage					shaderStage,
										EShaderOptions						shaderOptions,
										RHI::SShaderDescription				&shaderDesc,
										const SShaderCompilationSettings	&settings)
:	m_InputPath(inputPath)
,	m_OutputPath(outputPath)
,	m_ShaderPermutation(shaderPermutation)
,	m_ShaderStage(shaderStage)
,	m_ParticleOptions(shaderOptions)
,	m_ShaderDescription(shaderDesc)
,	m_GenerateMetaShaderFile(false)
,	m_CompilationSettings(settings)
{
	if (m_CompilationSettings.m_KeepTmpFile)
		m_ShaderMetaFilePath = CFilePath::StripExtension(m_OutputPath) + META_SHADER_EXTENSION;
}

//----------------------------------------------------------------------------

CShaderCompilation::~CShaderCompilation()
{
}

//----------------------------------------------------------------------------

void	CShaderCompilation::Clear()
{
	m_InputPath.Clear();
	m_OutputPath.Clear();
	m_ShaderPermutation.Clear();
}

//----------------------------------------------------------------------------

bool	CShaderCompilation::Exec(RHI::EGraphicalApi api, IFileSystem *controller)
{
	//in this function checking if the path is absolute, as it's legit to compile/load shaders outside of the PopcornFX project (ie. After-Effects plugin relies on this)
	const CString	lockFileName = m_OutputPath + ".lock";
	if (controller->Exists(lockFileName))
		return true; // Already compiling

	const CString	dstFolder = CFilePath::StripFilename(m_OutputPath);
	bool			result = controller->CreateDirectoryChainIFN(dstFolder, CFilePath::IsAbsolute(dstFolder));
	if (!result)
	{
		CLog::Log(PK_ERROR, "Couldn't create directory chain '%s' when trying to compile shader '%s'", dstFolder.Data(), m_OutputPath.Data());
		return false;
	}
	bool		lockFileIsAbsolute = CFilePath::IsAbsolute(lockFileName);
	PFileStream	lockFile = controller->OpenStream(lockFileName, IFileSystem::Access_ReadWriteCreate, lockFileIsAbsolute);
	if (lockFile == null)
		return true; // Already compiling
	lockFile->Flush(); // Should not be needed

	controller->FileDelete(m_OutputPath, CFilePath::IsAbsolute(m_OutputPath)); // Delete destination file to force flush file

	// Additional lock file for meta objects: A same shader hash is shared with all graphics API (so we can't use 'lockFileName')
	// If we don't lock like so, an effect compiled for several graphics API fails with errors because async CShaderCompilation are trying to write to the same .meta file.
	// Ideally, this should be done earlier, per hash instead of per shader/API, but the shaders compilation submission isn't designed like so
	PFileStream	metaLockFile = null;
	CString		metaLockFileName;
	bool		metaLockFilePathIsAbsolute = CFilePath::IsAbsolute(metaLockFileName);
	if (!m_ShaderMetaFilePath.Empty())
	{
		metaLockFileName = m_ShaderMetaFilePath + ".lock";
		m_GenerateMetaShaderFile = !controller->Exists(metaLockFileName, metaLockFilePathIsAbsolute);
		if (m_GenerateMetaShaderFile)
		{
			metaLockFile = controller->OpenStream(metaLockFileName, IFileSystem::Access_ReadWriteCreate, metaLockFilePathIsAbsolute);
			if (metaLockFile == null)
				m_GenerateMetaShaderFile = false;
			else
				metaLockFile->Flush(); // Should not be needed
		}
	}

	result &=	Partial(api, GenerationStage, controller) &&
				Partial(api, CompilationStage, controller);

	if (m_GenerateMetaShaderFile)
	{
		PK_ASSERT(metaLockFile != null);
		PK_ASSERT(!metaLockFileName.Empty());
		metaLockFile->Close();
		controller->FileDelete(metaLockFileName, metaLockFilePathIsAbsolute);
	}

	lockFile->Close();
	controller->FileDelete(lockFileName, lockFileIsAbsolute);

	return result;
}

//----------------------------------------------------------------------------

bool	CShaderCompilation::Partial(RHI::EGraphicalApi api, EStage compilationStage, IFileSystem *controller)
{
	bool	success = true;

	if (m_ShaderDescription.m_DrawMode == RHI::DrawModeInvalid && m_ShaderDescription.m_Pipeline == RHI::InvalidShaderStagePipeline)
		return true; // empty shader - skip

	const CString	preprocessedFilePath = CFilePath::StripExtension(m_OutputPath) + GetShaderExtensionStringFromApi(api, true);
	const CString	generatedFilePath = preprocessedFilePath + INTERMEDIATE_SHADER_EXTENSION;

	if (compilationStage == EStage::GenerationStage)
	{
		const CString	dstFolder = CFilePath::StripFilename(m_OutputPath);
		if (!controller->CreateDirectoryChainIFN(dstFolder, CFilePath::IsAbsolute(dstFolder)))
		{
			CLog::Log(PK_ERROR, "Couldn't create directory chain '%s' when trying to compile shader '%s'", dstFolder.Data(), m_OutputPath.Data());
			return false;
		}
		RHI::ShaderConstantBindingGenerator::ResetBindings(m_ShaderDescription.m_Bindings);
		RHI::ShaderConstantBindingGenerator::GenerateBindingsForApi(api, m_ShaderDescription.m_Bindings);
		success &= GenerateShaderFile(api, m_InputPath, preprocessedFilePath, controller);
	}
	else if (compilationStage == EStage::CompilationStage)
	{
		const SShaderCompilationTargetSettings	*setting = null;

		for (const SShaderCompilationTargetSettings &target : m_CompilationSettings.m_Targets)
		{
			if (target.m_Target == api)
				setting = &target;
		}
		if (setting == null)
			return false;

		CString	compileCmdLine = setting->m_CompilerCmdLine;
		const bool	pssl = setting->m_Target == RHI::GApi_Orbis || setting->m_Target == RHI::GApi_UNKNOWN2;
		switch (m_ShaderStage)
		{
		case RHI::VertexShaderStage:
			compileCmdLine = compileCmdLine.Replace("##ShortStage##", pssl ? "vs_vs" : "vs");
			compileCmdLine = compileCmdLine.Replace("##ShaderStage##", "vert");
			break;
		case RHI::GeometryShaderStage:
			compileCmdLine = compileCmdLine.Replace("##ShortStage##", "gs");
			compileCmdLine = compileCmdLine.Replace("##ShaderStage##", "geom");
			break;
		case RHI::FragmentShaderStage:
			compileCmdLine = compileCmdLine.Replace("##ShortStage##", "ps");
			compileCmdLine = compileCmdLine.Replace("##ShaderStage##", "frag");
			break;
		default: break;
		}

		if (!compileCmdLine.Empty())
			success &= CompileShaderFile(compileCmdLine, preprocessedFilePath, m_OutputPath, controller);
		if (success)
			CLog::Log(PK_INFO, "Shader %s successfully compiled", m_OutputPath.Data());
		else
			CLog::Log(PK_INFO, "Shader failed compilation: \"%s\"", compileCmdLine.Data());

		if (success && !m_CompilationSettings.m_KeepTmpFile)
		{
			controller->FileDelete(generatedFilePath);
			if (!compileCmdLine.Empty())
				controller->FileDelete(preprocessedFilePath);
		}
	}
	return success;
}

//----------------------------------------------------------------------------

bool	CShaderCompilation::GenerateShaderFile(RHI::EGraphicalApi apiName, const CString &inputPath, const CString &outputPath, IFileSystem *controller) const
{
	//in this function checking if the path is absolute, as it's legit to compile/load shaders outside of the PopcornFX project (ie. After-Effects plugin relies on this)
	PKSample::CAbstractShaderGenerator	*generator = shaderGenerators[apiName];
	if (generator == null)
		return false;

	CString	content;
	if (!inputPath.Empty())
	{
		content = controller->BufferizeToString(inputPath, CFilePath::IsAbsolute(inputPath));
		if (content.Empty())
		{
			CLog::Log(PK_ERROR, "Couldn't open '%s' for reading when trying to compile shader '%s'", inputPath.Data(), outputPath.Data());
			return false;
		}
	}

	if (!generator->GatherShaderInfo(content, CFilePath::StripFilename(inputPath), controller))
	{
		CLog::Log(PK_ERROR, "Shader generator could not gather info from shader '%s'", inputPath.Data());
		return false;
	}

	const CString	newContent = generator->GenerateShader(content, m_ShaderStage, m_ShaderDescription, m_ParticleOptions);

	// Preprocess shader:
	CPreprocessor::SPreprocessOutput	preprocOut;
	TArray<CString>						shaderDefines;

	PKSample::GenerateDefinesFromDefinition(shaderDefines, m_ShaderDescription, m_ShaderStage);

	const CString	curDir = CFilePath::StripFilename(controller->VirtualToPhysical(inputPath, IFileSystem::Access_Read));
	const bool		pathNotVirtual = CFilePath::IsAbsolute(outputPath);
	const CString	generatedFilePath = outputPath + INTERMEDIATE_SHADER_EXTENSION; // Only used when preprocessing fails
	const CString	errorFilePath = outputPath + SHADER_ERROR_EXTENSION; // Only used when preprocessing fails
	const bool		preprocSuccess = CPreprocessor::PreprocessString(shaderDefines, newContent, curDir, preprocOut);

	if (!preprocOut.m_Error.Empty())
		CLog::Log(PK_ERROR, "preprocess errors:\n%s", preprocOut.m_Error.Data());
	if (!preprocOut.m_Info.Empty())
		CLog::Log(PK_ERROR, "preprocess info:\n%s", preprocOut.m_Info.Data());

	if (preprocSuccess)
	{
		PFileStream		fileView = controller->OpenStream(outputPath, IFileSystem::Access_WriteCreate, pathNotVirtual);
		if (fileView == null)
		{
			CLog::Log(PK_ERROR, "Couldn't create/open shader file '%s'", outputPath.Data());
			return false;
		}
		fileView->Write(preprocOut.m_Output.Data(), preprocOut.m_Output.Length());
		fileView->Close();
		if (controller->Exists(generatedFilePath))
			controller->FileDelete(generatedFilePath, pathNotVirtual);
		if (controller->Exists(errorFilePath))
			controller->FileDelete(errorFilePath, pathNotVirtual);
	}
	else
	{
		CLog::Log(PK_ERROR, "Shader precompiling failed");
		PFileStream		preprocessFailFileStream = controller->OpenStream(	generatedFilePath,
																			IFileSystem::Access_WriteCreate,
																			pathNotVirtual);
		PFileStream		errorFileStream = controller->OpenStream(	errorFilePath,
																	IFileSystem::Access_WriteCreate,
																	pathNotVirtual);
		if (preprocessFailFileStream == null)
		{
			CLog::Log(PK_ERROR, "Couldn't create/open shader intermediate file '%s'", generatedFilePath.Data());
			return false;
		}
		if (errorFileStream == null)
		{
			CLog::Log(PK_ERROR, "Couldn't create/open shader error file '%s'", errorFilePath.Data());
			return false;
		}
		preprocessFailFileStream->Write(newContent.Data(), newContent.Length());
		preprocessFailFileStream->Close();
		errorFileStream->Write(preprocOut.m_Error.Data(), preprocOut.m_Error.Length());
		errorFileStream->Close();
		if (controller->Exists(outputPath))
			controller->FileDelete(outputPath, pathNotVirtual);
		return false;
	}

	// Write meta file content, only if m_KeepTmpFile is enabled
	if (m_GenerateMetaShaderFile)
	{
		PK_ASSERT(!m_ShaderMetaFilePath.Empty());
		PFileStream		metaFileStream = controller->OpenStream(m_ShaderMetaFilePath, IFileSystem::Access_WriteCreate);
		if (metaFileStream == null)
		{
			CLog::Log(PK_ERROR, "Couldn't create/open shader meta file '%s'", m_ShaderMetaFilePath.Data());
			return false;
		}
		metaFileStream->Write(m_ShaderPermutation.Data(), m_ShaderPermutation.Length());
		metaFileStream->Close();
	}
	return true;
}

//----------------------------------------------------------------------------

bool	CShaderCompilation::CompileShaderFile(	const CString	&cmdLine,
												const CString	&inputPath,
												const CString	&outputPath,
												IFileSystem		*controller) const
{
	// Check if the path is absolute, as it's legit to compile/load shaders outside of the PopcornFX project (ie. After-Effects plugin relies on this)
	const CString	inputPhysicalPath = CFilePath::IsAbsolute(inputPath) ? inputPath : controller->VirtualToPhysical(inputPath, IFileSystem::Access_Read);
	const CString	outputPhysicalPath = CFilePath::IsAbsolute(outputPath) ? outputPath : controller->VirtualToPhysical(outputPath, IFileSystem::Access_ReadWriteCreate);
	const CString	outputDirPhysicalPath = CFilePath::StripFilename(outputPhysicalPath);
	if (inputPhysicalPath.Empty() || outputPhysicalPath.Empty())
	{
		CLog::Log(PK_ERROR, "Could not resolve the physical path for the shader preprocessing");
		return false;
	}

	CString			program;
	TArray<CString>	processArgs;

	CProcess::ParseCommandLine(cmdLine, program, processArgs);

	// Inject path in commandline arguments
	for (CString &argument : processArgs)
	{
		if (argument.Contains("##InputPath##"))
			argument = argument.Replace("##InputPath##", inputPhysicalPath.Data());
		else if (argument.Contains("##OutputPath##"))
			argument = argument.Replace("##OutputPath##", outputPhysicalPath.Data());
		else if (argument.Contains("##OutputDir##"))
			argument = argument.Replace("##OutputDir##", outputDirPhysicalPath.Data());
#if defined(PK_WINDOWS)
		// Fix #11905: Rendering is broken when creating a project on a remote network drive with a path of the form '//123.45.67.89'
		// fxc.exe does not support network paths with forward slashes :|
		argument = argument.Replace("/", "\\");
#endif
	}

	// We end by compiling the shader
	CProcess	compileProcess;
	if (compileProcess.Start(program, processArgs))
		compileProcess.WaitForExit();

	const CProcess::EExitStatus	exitStatus = compileProcess.GetExitStatus();
	if (!PK_VERIFY_MESSAGE(exitStatus == CProcess::StatusSuccess, "Failed compiling shader"))
	{
		PK_ASSERT_MESSAGE(exitStatus != CProcess::StatusStillActive, "Internal error: process.GetExitStatus()");
		PK_ASSERT_MESSAGE(exitStatus != CProcess::StatusNotStarted, "Process failed to start");
		PK_ASSERT_MESSAGE(exitStatus != CProcess::StatusFailed, "Process failed to finish");

		const CString	command = CProcess::CreateCommandLine(program, processArgs);

		CLog::Log(PK_ERROR, "Shader compile command failed: \"%s\"", command.Data());
		return false;
	}
	return true;
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END

#endif	// (PK_SAMPLE_LIB_HAS_SHADER_GENERATOR != 0)
