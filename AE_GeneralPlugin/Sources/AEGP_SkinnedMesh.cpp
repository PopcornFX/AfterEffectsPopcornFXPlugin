//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#include "ae_precompiled.h"
#include "AEGP_SkinnedMesh.h"

#include <pk_geometrics/include/ge_matrix_tools.h>		// for random XForms tests
#include <pk_maths/include/pk_maths_simd_matrix.h>		// for 'Skin_PostProcess'
#include <pk_geometrics/include/ge_mesh_sampler_accel.h>	// for CMeshSurfaceSamplerStructuresRandom && CMeshVolumeSamplerStructuresRandom

// For skinned mesh backdrops
#include <pk_particles_toolbox/include/pt_skeleton_anim.h>
#include <pk_geometrics/include/ge_coordinate_frame.h>
#include <pk_particles_toolbox/include/pt_mesh_deformers_skin.h>

__AEGP_PK_BEGIN

//----------------------------------------------------------------------------
//
//	Skinned mesh instance wrapper implementation
//
//----------------------------------------------------------------------------

CSkinnedMesh::CSkinnedMesh()
	: m_SkinDt(0)
	, m_FirstFrame(false)
	, m_SamplingChannels(0)
{
}

//----------------------------------------------------------------------------

CSkinnedMesh::~CSkinnedMesh()
{
	Reset();
}

//----------------------------------------------------------------------------

void	CSkinnedMesh::Reset()
{
	WaitAsyncUpdateSkin();

	m_SubMeshes.Clear();

	m_SkinDt = 0;
	m_FirstFrame = 0;
	m_SamplingChannels = 0;

	m_Timeline = CTimeline();	// must reset before clearing m_SkeletonState
	m_SkeletonState = null;
}

//----------------------------------------------------------------------------

bool	CSkinnedMesh::Init(const TResourcePtr<CResourceMesh> &meshResource, u32 samplingChannels, const PSkeletonState &srcSkeletonState)
{
	const bool	success = _Init_Impl(meshResource, samplingChannels, srcSkeletonState);
	if (!success)
		Reset();
	return success;
}

//----------------------------------------------------------------------------

bool	CSkinnedMesh::_Init_Impl(const TResourcePtr<CResourceMesh> &meshResource, u32 samplingChannels, const PSkeletonState &srcSkeletonState)
{
	WaitAsyncUpdateSkin();

	m_FirstFrame = true;
	m_SamplingChannels = samplingChannels;

	if (meshResource == null || srcSkeletonState == null)
		return false;

	for (auto &mesh : m_SubMeshes)
	{
		mesh.m_SkeletalMesh = null;
		mesh.m_SkeletonState = null;
	}

	m_Timeline = CTimeline();	// must reset before clearing m_SkeletonState
	m_SkeletonState = null;

	// create and copy skeleton state if necessary:
	if (m_SkeletonState == null)
	{
		m_SkeletonState = PK_NEW(CSkeletonState);
		if (m_SkeletonState == null)
			return false;
	}

	if (!m_SkeletonState->DeepCopy(*srcSkeletonState))
		return false;

	m_SkeletonState->SetTimeline(&m_Timeline);

	const u32	kLodLevel = 0;
	TMemoryView<const PResourceMeshBatch>	batchList = meshResource->BatchList(kLodLevel);
	if (!PK_VERIFY(m_SubMeshes.Resize(batchList.Count())))
		return false;

	for (u32 batchId = 0; batchId < m_SubMeshes.Count(); batchId++)
	{
		SSubMesh	&mesh = m_SubMeshes[batchId];

		// grab the desired skeletal mesh:
		mesh.m_SkeletalMesh = batchList[batchId].Get();
		if (!PK_VERIFY(mesh.m_SkeletalMesh != null))
			return false;

		// FIXME: Handle when some submeshes aren't skinned
		if (!mesh.m_SkeletalMesh->IsSkinned())
		{
			mesh.m_SkeletalMesh = null;
			mesh.m_SkeletonState = null;
			continue;
		}

		mesh.m_SkeletonState = m_SkeletonState;

		// grab the raw geometry:
		const CMeshTriangleBatch	&srcGeomBatch = mesh.m_SkeletalMesh->RawMesh()->TriangleBatch();
		const CMeshVStream			&srcVStream = srcGeomBatch.m_VStream;
		if (srcVStream.Empty())
			return false;

		// do we need to create or resize our locally skinned vertex streams?
		if (mesh.m_RawSkinnedDataElementCount != srcVStream.VertexCount())
		{
			// nedds initial alloc, or resize
			const u32	vertexCount = srcVStream.VertexCount();
			const u32	offsetPos = 0;
			const u32	offsetNor = Mem::Align<Memory::CacheLineSize>(offsetPos + vertexCount * u32(sizeof(CFloat4)));	// pad Positions & Normals to Float4 for fast aligned SIMD processing
			const u32	offsetTan = Mem::Align<Memory::CacheLineSize>(offsetNor + vertexCount * u32(sizeof(CFloat4)));
			const u32	offsetOld = Mem::Align<Memory::CacheLineSize>(offsetTan + vertexCount * u32(sizeof(CFloat4)));
			const u32	offsetVel = Mem::Align<Memory::CacheLineSize>(offsetOld + vertexCount * u32(sizeof(CFloat4)));
			const u32	offsetEnd = Mem::Align<Memory::CacheLineSize>(offsetVel + vertexCount * u32(sizeof(CFloat4)));

			const u32	totalStreamsFootprint = offsetEnd;
			void		*rawDataPtr = PK_REALLOC_ALIGNED(mesh.m_RawSkinnedData, totalStreamsFootprint, Memory::CacheLineSize);	// alloc or grow existing buffer
			if (rawDataPtr == null)
			{
				PK_FREE(mesh.m_RawSkinnedData);
				mesh.m_RawSkinnedData = null;
				mesh.m_Positions = TStridedMemoryView<CFloat3>();
				mesh.m_Normals = TStridedMemoryView<CFloat3>();
				mesh.m_Tangents = TStridedMemoryView<CFloat4>();
				mesh.m_OldPositions = TStridedMemoryView<CFloat3>();
				mesh.m_Velocities = TStridedMemoryView<CFloat3>();
				return false;
			}

			mesh.m_RawSkinnedDataElementCount = vertexCount;
			mesh.m_RawSkinnedData = rawDataPtr;

			// build views into the memory we just (re)allocated:
			mesh.m_Positions = TStridedMemoryView<CFloat3>(static_cast<CFloat3*>(Mem::AdvanceRawPointer(mesh.m_RawSkinnedData, offsetPos)), vertexCount, sizeof(CFloat4));
			mesh.m_Normals = TStridedMemoryView<CFloat3>(static_cast<CFloat3*>(Mem::AdvanceRawPointer(mesh.m_RawSkinnedData, offsetNor)), vertexCount, sizeof(CFloat4));
			mesh.m_Tangents = TStridedMemoryView<CFloat4>(static_cast<CFloat4*>(Mem::AdvanceRawPointer(mesh.m_RawSkinnedData, offsetTan)), vertexCount, sizeof(CFloat4));
			mesh.m_OldPositions = TStridedMemoryView<CFloat3>(static_cast<CFloat3*>(Mem::AdvanceRawPointer(mesh.m_RawSkinnedData, offsetOld)), vertexCount, sizeof(CFloat4));
			mesh.m_Velocities = TStridedMemoryView<CFloat3>(static_cast<CFloat3*>(Mem::AdvanceRawPointer(mesh.m_RawSkinnedData, offsetVel)), vertexCount, sizeof(CFloat4));

			mesh.m_SampleSourceOverride.m_PositionsOverride = TStridedMemoryView<CFloat3>();
			mesh.m_SampleSourceOverride.m_NormalsOverride = TStridedMemoryView<CFloat3>();
			mesh.m_SampleSourceOverride.m_TangentsOverride = TStridedMemoryView<CFloat4>();
			mesh.m_SampleSourceOverride.m_VelocitiesOverride = TStridedMemoryView<CFloat3>();
		}

		if (samplingChannels & Channel_Position)
			mesh.m_SampleSourceOverride.m_PositionsOverride = mesh.m_Positions;

		if (samplingChannels & Channel_Velocity)
			mesh.m_SampleSourceOverride.m_VelocitiesOverride = mesh.m_Velocities;

		if (samplingChannels & Channel_Normal)
			mesh.m_SampleSourceOverride.m_NormalsOverride = mesh.m_Normals;

		if (samplingChannels & Channel_Tangent)
			mesh.m_SampleSourceOverride.m_TangentsOverride = mesh.m_Tangents;

		PK_ASSERT(mesh.m_RawSkinnedData != null);

		// copy source vertex-buffer streams to dst
		// Note: here we only copy the tangents. the 'w' components of the tangents will not be touched by the skinner, and have to be valid in the dst streams
		// (this might change in the future, where no initial copy necessary at all)
#ifdef	PK_SAMPLE_SKIN_TANGENTS
		TStridedMemoryView<const CFloat4>	srcVbTangents = srcVStream.Tangents();
		PK_ASSERT(srcVbTangents.Count() == srcVStream.VertexCount());
		PK_ASSERT(srcVbTangents.Count() == mesh.m_RawSkinnedDataElementCount);
		PK_ASSERT(mesh.m_Tangents.Stride() == sizeof(CFloat4));

		if (srcVbTangents.Stride() == mesh.m_Tangents.Stride())
		{
			// common-case fast-path
			Mem::Copy(mesh.m_Tangents.Data(), srcVbTangents.Data(), mesh.m_RawSkinnedDataElementCount * sizeof(CFloat4));
		}
		else
		{
			// do a slow copy (should not happen, except if we've got AOS VBs (we shouldn't, skinner will take a monstruous perf hit)
			for (u32 i = 0; i < mesh.m_RawSkinnedDataElementCount; i++)
				mesh.m_Tangents[i] = srcVbTangents[i];
		}
#endif
	}

	return true;
}

//----------------------------------------------------------------------------

bool	CSkinnedMesh::Play(const PSkeletonAnimationInstance &animInstance)
{
	if (m_SkeletonState != null)
	{
		if (m_SkeletonState->__TMP_GORE_PlayAnim(animInstance) == null)	// FIXME: gore
			return false;
	}
	return true;
}

//----------------------------------------------------------------------------

void	CSkinnedMesh::Stop()
{
	if (m_SkeletonState != null)
		m_SkeletonState->__TMP_GORE_StopAnim();	// FIXME: gore
}

//----------------------------------------------------------------------------

void	CSkinnedMesh::Update(float dt)
{
	m_Timeline.Update(dt);
	if (m_SkeletonState != null)
		m_SkeletonState->Update(dt);
}

//----------------------------------------------------------------------------

#if 0
void	CSkinnedMesh::Render()
{
	m_Renderer.AnimMesh(m_Positions);
	m_Renderer.Render();
}
#endif

//----------------------------------------------------------------------------

void	CSkinnedMesh::StartAsyncUpdateSkin(float dt)
{
	if (m_SkeletonState == null)
		return;

	const u32	samplingChannels = SamplingChannels();
	if (samplingChannels & Channel_Velocity)
		m_SkinDt = dt;

	for (auto &mesh : m_SubMeshes)
	{
		if (mesh.Empty())
			continue;

		SSkinContext	asyncSkinContext;

		const CMeshVStream	*vStream = &mesh.m_SkeletalMesh->RawMesh()->TriangleBatch().m_VStream;

		asyncSkinContext.m_SkinningStreams = mesh.m_SkeletalMesh->m_OptimizedStreams;

		// Skinner always expects positions, even if the effect won't be sampling them
		asyncSkinContext.m_SrcPositions = vStream->Positions();
		asyncSkinContext.m_DstPositions = mesh.m_Positions;

		// The rendering may need normals (in fact, the most of the cases)
		//if (samplingChannels & Channel_Normal)
		{
			asyncSkinContext.m_SrcNormals = vStream->Normals();
			asyncSkinContext.m_DstNormals = mesh.m_Normals;
		}

		// The rendering may need tangents (not for now, as skined mesh does not have normal-map)
		// for tangents, they are in fact a CFloat4 stream, with the 'w' component containing a sign telling if the tangent basis is mirrored.
		// we are only interested in skinning the float3 part, and we need to keep the 'w' component intact for correct tangent-space reconstruction in the shaders.
		if (samplingChannels & Channel_Tangent)
		{
			asyncSkinContext.m_SrcTangents = vStream->Tangents();
			asyncSkinContext.m_DstTangents = mesh.m_Tangents;
		}

		if (samplingChannels & Channel_Velocity)
		{
			// hook our pre-skin callback where we'll copy the positions to m_OldPositions so that the skin job can correctly
			// differentiate the two and compute the instantaneous mesh surface velocities using 'asyncSkinContext.m_SrcDt'
			asyncSkinContext.m_CustomProcess_PreSkin = SSkinContext::CbCustomProcess(&mesh, &SSubMesh::Skin_PreProcess);
			asyncSkinContext.m_CustomProcess_PostSkin = SSkinContext::CbCustomProcess(&mesh, &SSubMesh::Skin_PostProcess);
		}

		asyncSkinContext.m_SkinFlags = SkinFlags_UseMultipassSkinning;
		// If you handle unskinned verts in the PostSkin pass, you can set the following flag,
		// it'll avoid unnecessary copies in the skinner jobs:
//		asyncSkinContext.m_SkinFlags = SkinFlags_DontCopyUnskinnedVerts;

		mesh.m_SkinDt = m_SkinDt;
		mesh.m_FirstFrame = m_FirstFrame;

		CSkeletalSkinnerSimple::AsyncSkinStart(mesh.m_SkinUpdateContext, m_SkeletonState->View(), asyncSkinContext);
	}
}

//----------------------------------------------------------------------------

void	CSkinnedMesh::SSubMesh::Skin_PreProcess(u32 vertexStart, u32 vertexCount, const SSkinContext &ctx)
{
	(void)ctx;
	if (m_SkeletonState == null)
		return;

	TStridedMemoryView<const CFloat3>	src = m_Positions.Slice(vertexStart, vertexCount);
	TStridedMemoryView<CFloat3>			dst = m_OldPositions.Slice(vertexStart, vertexCount);

	PK_ASSERT(src.Stride() == 0x10 && dst.Stride() == 0x10);
	Mem::Copy(dst.Data(), src.Data(), dst.Count() * dst.Stride());
}

//----------------------------------------------------------------------------

void	CSkinnedMesh::SSubMesh::Skin_PostProcess(u32 vertexStart, u32 vertexCount, const SSkinContext &ctx)
{
	if (m_SkeletonState == null)
		return;

	PK_SCOPEDPROFILE();	// record this function in the visual profiler

	// compute instantaneous surface velocities IFN:
	TStridedMemoryView<CFloat3>			vel = m_Velocities.Slice(vertexStart, vertexCount);
	TStridedMemoryView<const CFloat3>	posCur = ctx.m_DstPositions.Slice(vertexStart, vertexCount);
	TStridedMemoryView<const CFloat3>	posOld = m_OldPositions.Slice(vertexStart, vertexCount);
	const float							dt = m_SkinDt;

	const bool	continuousAnim = true;//(m_Skeleton != null) && (m_Skeleton->LastUpdateFrameID() == m_LastSkinnedSkeletonFrameID + 1);
	if ((m_FirstFrame || !continuousAnim) && !vel.Empty())
	{
		Mem::Clear(vel.Data(), vel.CoveredBytes());

		PK_ASSERT(posCur.Stride() == posOld.Stride());
		PK_ASSERT(posCur.Count() == posOld.Count());
		PK_ASSERT(posCur.CoveredBytes() == posOld.CoveredBytes());
		if (m_FirstFrame)
		{
			// Avoid glitches during the first frame: our old positions will be the bindpose,
			// we don't want particles to lerp between bindpose and first real frame
			Mem::Copy((void*)posOld.Data(), posCur.Data(), posOld.CoveredBytes());
		}
	}
	else if (!posCur.Empty() && !posOld.Empty() && !vel.Empty())
	{
		PK_ASSERT(posCur.Stride() == 0x10);
		PK_ASSERT(posOld.Stride() == 0x10);
		PK_ASSERT(vel.Stride() == 0x10);

		CFloat3			* __restrict dstVel = vel.Data();
		const CFloat3	*dstVelEnd = Mem::AdvanceRawPointer(dstVel, vertexCount * 0x10);
		const CFloat3	*srcPosCur = posCur.Data();
		const CFloat3	*srcPosOld = posOld.Data();

		PK_ASSERT(Mem::IsAligned<0x10>(dstVel));
		PK_ASSERT(Mem::IsAligned<0x10>(dstVelEnd));
		PK_ASSERT(Mem::IsAligned<0x10>(srcPosCur));
		PK_ASSERT(Mem::IsAligned<0x10>(srcPosOld));

		const SIMD::Float4	invDt = SIMD::Float4(1.0f / dt);
		dstVelEnd = Mem::AdvanceRawPointer(dstVelEnd, -0x40);
		while (dstVel < dstVelEnd)
		{
			const SIMD::Float4	pA0 = SIMD::Float4::LoadAligned16(srcPosOld, 0x00);
			const SIMD::Float4	pA1 = SIMD::Float4::LoadAligned16(srcPosOld, 0x10);
			const SIMD::Float4	pA2 = SIMD::Float4::LoadAligned16(srcPosOld, 0x20);
			const SIMD::Float4	pA3 = SIMD::Float4::LoadAligned16(srcPosOld, 0x30);
			const SIMD::Float4	pB0 = SIMD::Float4::LoadAligned16(srcPosCur, 0x00);
			const SIMD::Float4	pB1 = SIMD::Float4::LoadAligned16(srcPosCur, 0x10);
			const SIMD::Float4	pB2 = SIMD::Float4::LoadAligned16(srcPosCur, 0x20);
			const SIMD::Float4	pB3 = SIMD::Float4::LoadAligned16(srcPosCur, 0x30);
			const SIMD::Float4	v0 = (pB0 - pA0) * invDt;
			const SIMD::Float4	v1 = (pB1 - pA1) * invDt;
			const SIMD::Float4	v2 = (pB2 - pA2) * invDt;
			const SIMD::Float4	v3 = (pB3 - pA3) * invDt;
			v0.StoreAligned16(dstVel, 0x00);
			v1.StoreAligned16(dstVel, 0x10);
			v2.StoreAligned16(dstVel, 0x20);
			v3.StoreAligned16(dstVel, 0x30);

			dstVel = Mem::AdvanceRawPointer(dstVel, 0x40);
			srcPosCur = Mem::AdvanceRawPointer(srcPosCur, 0x40);
			srcPosOld = Mem::AdvanceRawPointer(srcPosOld, 0x40);
		}
		dstVelEnd = Mem::AdvanceRawPointer(dstVelEnd, +0x40);

		while (dstVel < dstVelEnd)
		{
			const SIMD::Float4	pA = SIMD::Float4::LoadAligned16(srcPosOld);
			const SIMD::Float4	pB = SIMD::Float4::LoadAligned16(srcPosCur);
			const SIMD::Float4	v = (pB - pA) * invDt;
			v.StoreAligned16(dstVel);

			dstVel = Mem::AdvanceRawPointer(dstVel, 0x10);
			srcPosCur = Mem::AdvanceRawPointer(srcPosCur, 0x10);
			srcPosOld = Mem::AdvanceRawPointer(srcPosOld, 0x10);
		}
	}
}

//----------------------------------------------------------------------------

bool	CSkinnedMesh::WaitAsyncUpdateSkin()
{
	if (m_SkeletonState == null)
		return true;

	for (auto &mesh : m_SubMeshes)
	{
		if (mesh.Empty())
			continue;
		CAABB	dummy;
		if (!CSkeletalSkinnerSimple::AsyncSkinWait(mesh.m_SkinUpdateContext, &dummy, true))
			return false;

		if (mesh.m_FirstFrame && !mesh.m_Velocities.Empty())
			Mem::Clear(mesh.m_Velocities.Data(), mesh.m_Velocities.CoveredBytes());

		mesh.m_FirstFrame = false;
	}

	m_FirstFrame = false;
	return true;
}

//----------------------------------------------------------------------------

void	CSkinnedMesh::ClearVelocities()
{
	for (auto &mesh : m_SubMeshes)
	{
		if (!mesh.Empty())
			Mem::Clear(mesh.m_Velocities.Data(), mesh.m_Velocities.CoveredBytes());
	}
}

//----------------------------------------------------------------------------

__AEGP_PK_END
