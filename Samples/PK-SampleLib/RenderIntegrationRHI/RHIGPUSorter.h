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

#include "PK-SampleLib/PKSample.h"
#include "PK-SampleLib/RenderIntegrationRHI/RHITypePolicy.h"

#include <pk_rhi/include/FwdInterfaces.h>
#include <pk_rhi/include/interfaces/IApiManager.h>
#include <pk_rhi/include/interfaces/IGpuBuffer.h>

#define PK_GPU_SORT_NUM_KEY_PER_THREAD 24

__PK_SAMPLE_API_BEGIN

//----------------------------------------------------------------------------

class CGPUSorter
{
public:
	CGPUSorter() :	m_SortKeySizeInBits(0),
					m_SortKeyStrideInBytes(0),
					m_IndirectionIndexSizeInBits(32),
					m_IndirectionIndexStrideInBytes(0),
					m_AlignedElementCount(0),
					m_SortGroupCount(0),
					m_BuffersMaxElementCount(0) {};

	~CGPUSorter() {};

	bool				Init(u32 sortKeySizeInBits, const RHI::PApiManager &apiManager);

	bool				AllocateBuffers(u32 elementCount, const RHI::PApiManager &apiManager);

	bool				SetInOutBuffers(const RHI::PGpuBuffer &sortKeys, const RHI::PGpuBuffer &indirection);

	bool				AppendDispatchs(const PCRendererCacheInstance &rCacheInstance, TArray<SRHIComputeDispatchs> &outDispatchs);

private:
	u32					m_SortKeySizeInBits;
	u32					m_SortKeyStrideInBytes;
	u32					m_IndirectionIndexSizeInBits = 32u; // This is not exposed yet, but could be to handle more complex indirection (ex: combining particle ID and draw request ID)
	u32					m_IndirectionIndexStrideInBytes;

	u32					m_AlignedElementCount;
	u32					m_SortGroupCount;
	u32					m_BuffersMaxElementCount; // Keeps trace of the greatest element count buffers has been resized to, to avoid resizing buffers when not necessary.

	RHI::PGpuBuffer		m_SortIndirection0;
	RHI::PGpuBuffer		m_SortIndirection1;
	RHI::PGpuBuffer		m_SortKeys0;
	RHI::PGpuBuffer		m_SortKeys1;
	RHI::PGpuBuffer		m_SortCounts;

	TStaticArray<RHI::PConstantSet, 2>	m_UpSweepConstantSets;
	RHI::PConstantSet					m_PrefixSumConstantSet;
	TStaticArray<RHI::PConstantSet, 2>	m_DownSweepConstantSets; // 2 sets to shuffle input and output between passes + one set for final pass

	bool				m_ConstantSetsDirty = true;
};

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
