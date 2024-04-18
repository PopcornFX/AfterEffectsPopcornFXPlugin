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

#include "PK-SampleLib/ShaderDefinitions/SampleLibShaderDefinitions.h"

#include "RHIRenderIntegrationConfig.h"

#include "RHIGPUSorter.h"

__PK_SAMPLE_API_BEGIN

//----------------------------------------------------------------------------

bool	CGPUSorter::Init(u32 sortKeySizeInBits, const RHI::PApiManager &apiManager)
{
	if (m_SortKeySizeInBits == sortKeySizeInBits)
		return true;

	m_SortKeySizeInBits = sortKeySizeInBits;
	m_SortKeyStrideInBytes = Mem::Align(m_SortKeySizeInBits, 32u) / 8u;
	m_IndirectionIndexStrideInBytes = Mem::Align(m_IndirectionIndexSizeInBits, 32u) / 8u;

	if (!PK_VERIFY_MESSAGE(m_IndirectionIndexSizeInBits == 32, "Sort only supports 32 bit indirection indices."))
		return false;
	if (!PK_VERIFY_MESSAGE(m_SortKeyStrideInBytes == sizeof(u32) || m_SortKeyStrideInBytes == (2 * sizeof(u32)), "Sort keys must be 32 of 64 bit aligned."))
		return false;

	{
		RHI::SConstantSetLayout	layout;
		PKSample::CreateSortUpSweepConstantSetLayout(layout);
		for (auto &constantSet : m_UpSweepConstantSets)
			constantSet = apiManager->CreateConstantSet(RHI::SRHIResourceInfos("UpSweep Constant Set"), layout);
	}
	{
		RHI::SConstantSetLayout	layout;
		PKSample::CreateSortPrefixSumConstantSetLayout(layout);
		m_PrefixSumConstantSet = apiManager->CreateConstantSet(RHI::SRHIResourceInfos("Prefix Sum Constant Set"), layout);
	}
	{
		RHI::SConstantSetLayout	layout;
		PKSample::CreateSortDownSweepConstantSetLayout(layout);
		for (auto &constantSet : m_DownSweepConstantSets)
			constantSet = apiManager->CreateConstantSet(RHI::SRHIResourceInfos("DownSweep Constant Set"), layout);
	}

	return true;
}

//----------------------------------------------------------------------------

bool	CGPUSorter::AllocateBuffers(u32 elementCount, const RHI::PApiManager &apiManager)
{
	// A radix sort thread handles PK_GPU_SORT_NUM_KEY_PER_THREAD key, so a dispatch computes PK_GPU_SORT_NUM_KEY_PER_THREAD * PK_RH_GPU_THREADGROUP_SIZE elements.
	m_AlignedElementCount = Mem::Align<PK_GPU_SORT_NUM_KEY_PER_THREAD * PK_RH_GPU_THREADGROUP_SIZE>(elementCount);
	m_SortGroupCount = m_AlignedElementCount / (PK_GPU_SORT_NUM_KEY_PER_THREAD * PK_RH_GPU_THREADGROUP_SIZE);

	if (m_AlignedElementCount > m_BuffersMaxElementCount)
	{
		m_SortIndirection1 = apiManager->CreateGpuBuffer(RHI::SRHIResourceInfos("Sort Indirection1 Buffer"), RHI::RawBuffer, m_AlignedElementCount * m_IndirectionIndexStrideInBytes);
		m_SortKeys1 = apiManager->CreateGpuBuffer(RHI::SRHIResourceInfos("Sort Keys1 Buffer"), RHI::RawBuffer, m_AlignedElementCount * m_SortKeyStrideInBytes);

		const u32	alignedSortGroupCount = Mem::Align<PK_RH_GPU_THREADGROUP_SIZE>(m_SortGroupCount);

		m_SortCounts = apiManager->CreateGpuBuffer(RHI::SRHIResourceInfos("Sort Counts Buffer"), RHI::RawBuffer, alignedSortGroupCount * sizeof(CUint4) * 4);

		if (!PK_VERIFY(m_SortIndirection1 != null) ||
			!PK_VERIFY(m_SortKeys1 != null) ||
			!PK_VERIFY(m_SortCounts != null))
			return false;

		m_BuffersMaxElementCount = m_AlignedElementCount;
		m_ConstantSetsDirty = true;
	}

	return true;
}

//----------------------------------------------------------------------------

bool	CGPUSorter::SetInOutBuffers(const RHI::PGpuBuffer &sortKeys, const RHI::PGpuBuffer &indirection)
{
	if (!PK_VERIFY(sortKeys != null && indirection != null))
		return false;
	if (m_SortIndirection0 != indirection)
	{
		m_SortIndirection0 = indirection;
		m_ConstantSetsDirty = true;
	}
	if (m_SortKeys0 != sortKeys)
	{
		m_SortKeys0 = sortKeys;
		m_ConstantSetsDirty = true;
	}

	return true;
}

//----------------------------------------------------------------------------

bool	CGPUSorter::AppendDispatchs(const PCRendererCacheInstance &rCacheInstance, TArray<SRHIComputeDispatchs> &outDispatchs)
{
	if (!PK_VERIFY(m_SortIndirection0->GetByteSize() >= (m_AlignedElementCount * m_IndirectionIndexStrideInBytes)) &&
		!PK_VERIFY(m_SortIndirection1->GetByteSize() >= (m_AlignedElementCount * m_IndirectionIndexStrideInBytes)) &&
		!PK_VERIFY(m_SortKeys0->GetByteSize() >= (m_AlignedElementCount * m_SortKeyStrideInBytes)) &&
		!PK_VERIFY(m_SortKeys1->GetByteSize() >= (m_AlignedElementCount * m_SortKeyStrideInBytes)))
		return false;

	if (m_ConstantSetsDirty)
	{
		// Upsweep (2 sets for shuffle)
		m_UpSweepConstantSets[0]->SetConstants(m_SortIndirection0, 0);
		m_UpSweepConstantSets[0]->SetConstants(m_SortKeys0, 1);
		m_UpSweepConstantSets[0]->SetConstants(m_SortCounts, 2);
		m_UpSweepConstantSets[0]->UpdateConstantValues();
		m_UpSweepConstantSets[1]->SetConstants(m_SortIndirection1, 0);
		m_UpSweepConstantSets[1]->SetConstants(m_SortKeys1, 1);
		m_UpSweepConstantSets[1]->SetConstants(m_SortCounts, 2);
		m_UpSweepConstantSets[1]->UpdateConstantValues();

		// Prefix sum
		m_PrefixSumConstantSet->SetConstants(m_SortCounts, 0);
		m_PrefixSumConstantSet->UpdateConstantValues();

		// Upsweep (2 sets for shuffle + 1 for the last output)
		m_DownSweepConstantSets[0]->SetConstants(m_SortCounts, 0);
		m_DownSweepConstantSets[0]->SetConstants(m_SortKeys0, 1);
		m_DownSweepConstantSets[0]->SetConstants(m_SortIndirection0, 2);
		m_DownSweepConstantSets[0]->SetConstants(m_SortKeys1, 3);
		m_DownSweepConstantSets[0]->SetConstants(m_SortIndirection1, 4);
		m_DownSweepConstantSets[0]->UpdateConstantValues();
		m_DownSweepConstantSets[1]->SetConstants(m_SortCounts, 0);
		m_DownSweepConstantSets[1]->SetConstants(m_SortKeys1, 1);
		m_DownSweepConstantSets[1]->SetConstants(m_SortIndirection1, 2);
		m_DownSweepConstantSets[1]->SetConstants(m_SortKeys0, 3);
		m_DownSweepConstantSets[1]->SetConstants(m_SortIndirection0, 4);
		m_DownSweepConstantSets[1]->UpdateConstantValues();

		m_ConstantSetsDirty = false;
	}

	const u32 kBitCount = (m_SortKeySizeInBits + 3) / 4;

	for (u32 bit = 0; bit < kBitCount; bit++)
	{
		// Compute : Up sweep
		{
			PKSample::SRHIComputeDispatchs	computeDispatch = PKSample::SRHIComputeDispatchs();

			// Constant set
			computeDispatch.m_ConstantSet = m_UpSweepConstantSets[bit % 2];

			// Push constant
			if (!PK_VERIFY(computeDispatch.m_PushConstants.PushBack().Valid()))
				return false;
			u32	*drPushConstant = reinterpret_cast<u32*>(&computeDispatch.m_PushConstants.Last());
			drPushConstant[0] = bit;

			computeDispatch.m_BufferMemoryBarriers.PushBack(bit % 2 == 0 ? m_SortKeys0 : m_SortKeys1);

			// State
			const PKSample::EComputeShaderType	type = m_SortKeyStrideInBytes == sizeof(u32) ? ComputeType_SortUpSweep : ComputeType_SortUpSweep_KeyStride64;
			computeDispatch.m_State = rCacheInstance->m_Cache->GetComputeState(type);
			if (!PK_VERIFY(computeDispatch.m_State != null))
				return false;

			// Dispatch args
			computeDispatch.m_ThreadGroups = CInt3(m_SortGroupCount, 1, 1); // groups are in fact half sized, but we launch the usual number of groups

			outDispatchs.PushBack(computeDispatch);
		}

		// Compute : Prefix sum on group counts
		{
			PKSample::SRHIComputeDispatchs	computeDispatch = PKSample::SRHIComputeDispatchs();

			// Constant set
			computeDispatch.m_ConstantSet = m_PrefixSumConstantSet;

			// Push constant
			if (!PK_VERIFY(computeDispatch.m_PushConstants.PushBack().Valid()))
				return false;
			u32	*drPushConstant = reinterpret_cast<u32*>(&computeDispatch.m_PushConstants.Last());
			drPushConstant[0] = bit;
			drPushConstant[1] = m_SortGroupCount;

			// Adds an explicit barrier: we want to wait for up sweep write
			// before reading and writing the prefix sum in SortCounts
			computeDispatch.m_BufferMemoryBarriers.PushBack(m_SortCounts);

			// State
			const PKSample::EComputeShaderType	type = ComputeType_SortPrefixSum;
			computeDispatch.m_State = rCacheInstance->m_Cache->GetComputeState(type);
			if (!PK_VERIFY(computeDispatch.m_State != null))
				return false;

			// Dispatch args
			computeDispatch.m_ThreadGroups = CInt3(1, 1, 1); // 1 group handles prefix sum on counts

			outDispatchs.PushBack(computeDispatch);
		}

		// Compute : Down sweep and final SortIndirection computation
		{
			PKSample::SRHIComputeDispatchs	computeDispatch = PKSample::SRHIComputeDispatchs();

			// Constant set
			computeDispatch.m_ConstantSet = m_DownSweepConstantSets[bit % 2];

			// Push constant
			if (!PK_VERIFY(computeDispatch.m_PushConstants.PushBack().Valid()))
				return false;
			u32	*drPushConstant = reinterpret_cast<u32*>(&computeDispatch.m_PushConstants.Last());
			drPushConstant[0] = bit;

			computeDispatch.m_BufferMemoryBarriers.PushBack(m_SortCounts);

			// State
			const PKSample::EComputeShaderType	type = m_SortKeyStrideInBytes == sizeof(u32) ? ComputeType_SortDownSweep : ComputeType_SortDownSweep_KeyStride64;
			computeDispatch.m_State = rCacheInstance->m_Cache->GetComputeState(type);
			if (!PK_VERIFY(computeDispatch.m_State != null))
				return false;

			// Dispatch args
			computeDispatch.m_ThreadGroups = CInt3(m_SortGroupCount, 1, 1);

			outDispatchs.PushBack(computeDispatch);
		}
	}

	return true;
}

//----------------------------------------------------------------------------

__PK_SAMPLE_API_END
