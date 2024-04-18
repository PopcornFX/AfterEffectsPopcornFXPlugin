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

#include "PK-SampleLib/PKSample.h"
#include <pk_rhi/include/interfaces/SRenderState.h>
#include <pk_rhi/include/AllInterfaces.h>
#include <PK-SampleLib/ShaderGenerator/ShaderGenerator.h>
#include <PK-SampleLib/ShaderDefinitions/ShaderDefinitions.h>

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

class	CShaderLoader
{
public:
	struct	SShadersPaths
	{
		CString		m_Vertex;
		CString		m_Geometry;
		CString		m_Fragment;

		bool		HasGeometry() const { return !m_Geometry.Empty(); }
	};

	CShaderLoader() : m_DefaultController(null) { }
	~CShaderLoader() { }

	bool		Release();

	bool		LoadShader(	RHI::SRenderState &renderState,
							const SShadersPaths &shadersPaths,
							const RHI::PApiManager &apiManager,
							IFileSystem *controller = null); // Give preprocess shader description to enable preprocessing


	bool		LoadShader(	RHI::SComputeState &pipelineState,
							const CString &shaderPath,
							const RHI::PApiManager &apiManager,
							IFileSystem *controller = null); // Give preprocess shader description to enable preprocessing

	void		SetDefaultFSController(IFileSystem *controller) { m_DefaultController = controller; }

private:
	struct	SLoadedModule
	{
		CString					m_Path;

		struct	SModule
		{
			CDigestMD5				m_Hash;
			RHI::PShaderModule		m_Module;
#ifdef PK_DEBUG
			RHI::SShaderBindings	m_ShaderBindingsForDebug;
#endif
		};

		TArray<SModule>			m_Modules;
	};

	struct	SLoadedProgram
	{
		CString					m_VertexPath;
		CString					m_GeometryPath;
		CString					m_FragmentPath;
		CString					m_ComputePath;

		bool HasSamePathThan(const SShadersPaths & shadersPaths) const { return m_VertexPath == shadersPaths.m_Vertex && m_FragmentPath == shadersPaths.m_Fragment && m_GeometryPath == shadersPaths.m_Geometry; }
		bool HasSamePathThan(const CString & computeShaderPath) const { return m_ComputePath == computeShaderPath; }

		struct	SProgram
		{
			CDigestMD5				m_VertexHash;
			CDigestMD5				m_GeometryHash;
			CDigestMD5				m_FragmentHash;
			CDigestMD5				m_ComputeHash;
			RHI::PShaderProgram		m_Program;
#ifdef PK_DEBUG
			RHI::SShaderBindings	m_ShaderBindingsForDebug;
#endif
		};

		TArray<SProgram>		m_Programs;
	};

	RHI::PShaderModule	FindOrLoadShaderModule(	TArray<SLoadedModule> &moduleLibrary,
												const CString &shaderPath,
												RHI::EShaderStage stage,
												const RHI::PApiManager &apiManager,
												const RHI::SShaderBindings &bindings,
												const CDigestMD5 &hash,
												IFileSystem *controller);
	RHI::PShaderModule	LoadShaderModule(	const CString &path,
											const CDigestMD5 &hash,
											const RHI::PApiManager &apiManager,
											RHI::EShaderStage stage,
											IFileSystem *controller);

	TArray<SLoadedModule>		m_VertexModules;
	TArray<SLoadedModule>		m_GeometryModules;
	TArray<SLoadedModule>		m_FragmentModules;
	TArray<SLoadedModule>		m_ComputeModules;

	TArray<SLoadedProgram>		m_Programs;

	IFileSystem		*m_DefaultController;
};

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
