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

#include "RHICustomTasks.h"

#if	(PK_HAS_PARTICLES_SELECTION != 0)

#include <pk_particles/include/Storage/MainMemory/storage_ram.h>
#include <pk_particles/include/Storage/MainMemory/storage_ram_stream.h>

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

const float	DebugColorGrey = 1.0f;
const float	DebugColorWhite = 2.0f;
const float	DebugColorRed = 3.0f;

void	CBillboard_Exec_WireframeDiscard::operator()(const Drawers::SBillboard_ExecPage &batch)
{
	if (m_DstSelectedParticles.Empty())
		return;

	PK_NAMEDSCOPEDPROFILE("CustomTasks CBillboard_Exec_WireframeDiscard");

	const u32	vertexPerParticle = batch.m_Billboarder->BillboardVertexCount();

	const u32	outCount = batch.m_Page->RenderedParticleCount();

	Mem::Clear(&m_DstSelectedParticles[batch.m_VertexOffset], sizeof(float) * outCount * vertexPerParticle);

	TStridedMemoryView<const u8>	enableds = batch.m_Page->StreamForReading<bool>(batch.m_DrawRequest->BaseBillboardingRequest().m_EnabledStreamId);
	const bool						hasEnabled = batch.m_DrawRequest->InputParticleCount() != batch.m_DrawRequest->RenderedParticleCount();

	if (batch.m_DrawRequest->RenderedParticleCount() == 0)
		return;

	for (const auto &renderSelection : m_SrcParticleSelected.m_AllRendererSelectionsView)
	{
		if (renderSelection.m_Renderers.Contains(batch.m_DrawRequest->BaseBillboardingRequest()._UnsafeRenderer()))
		{
			Mem::Fill32(&m_DstSelectedParticles[batch.m_VertexOffset], bit_cast<u32>(DebugColorGrey), outCount * vertexPerParticle);
		}
	}

	CGuid	particleId_First;

	TMemoryView<const SMediumParticleSelection>	&srcSelection = m_SrcParticleSelected.m_AllParticleSelectionsView;
	for (u32 i = 0; i < srcSelection.Count(); ++i)
	{
		if (batch.m_DrawRequest->BaseBillboardingRequest()._UnsafeMedium() == srcSelection[i].m_Medium)
		{
			PK_ASSERT(!srcSelection[i].m_Ranges.Empty());
			// Remap the particle Id from the simulation (in) to the particle id in the rendering buffers (out)
			u32		particleId_in = 0;
			u32		particleId_out = 0;

			u32		offset = 0;

			for (u32 j = 0; j < srcSelection[i].m_Ranges.Count(); ++j)
			{
				if (batch.m_Page->PageIdxInMedium() == srcSelection[i].m_Ranges[j].m_PageId)
				{
					const u32	range_startId = srcSelection[i].m_Ranges[j].StartID();
					const u32	range_count = srcSelection[i].m_Ranges[j].Count();

					u32		rangeOut_startId = range_startId;
					u32		rangeOut_count = range_count;
					if (hasEnabled)
					{
						PK_ASSERT(particleId_in <= range_startId); // Warning: it only works when "m_Ranges" are in ascending order and not overlapping. Otherwise, find another way !!!
						while (particleId_in < range_startId)
						{
							if (enableds[particleId_in] != 0)
								++particleId_out;
							++particleId_in;
						}
						rangeOut_startId = particleId_out;
						while (particleId_in < (range_startId + range_count))
						{
							if (enableds[particleId_in] != 0)
								++particleId_out;
							++particleId_in;
						}
						rangeOut_count = (particleId_out - rangeOut_startId);
					}

					if (rangeOut_count == 0)
						continue; // TODO: if it goes here with j == 0, then the first particle won't be hightlighted ...

					const bool	isFocusedMedium = (i == m_SrcParticleSelected.m_FocusedMedium);
					if (isFocusedMedium && srcSelection[i].m_CurrentSelectedId - offset < rangeOut_count)
						particleId_First = rangeOut_startId + srcSelection[i].m_CurrentSelectedId - offset;

					PK_ASSERT(rangeOut_startId + rangeOut_count <= outCount); // issue with mismatching between the current frame processed from frame-collector and the editor's selection input.
					const u32	writeCount = (rangeOut_startId < outCount) ? PKMin(rangeOut_count, outCount - rangeOut_startId) : 0;
					Mem::Fill32(&m_DstSelectedParticles[batch.m_VertexOffset + rangeOut_startId * vertexPerParticle], bit_cast<u32>((!m_SrcParticleSelected.m_FocusedMedium.Valid() || isFocusedMedium) ? DebugColorWhite : DebugColorGrey), writeCount * vertexPerParticle);
				}
				offset += srcSelection[i].m_Ranges[j].Count();
			}
		}
	}

	if (particleId_First.Valid() && particleId_First < outCount)
	{
		Mem::Fill32(&m_DstSelectedParticles[batch.m_VertexOffset + particleId_First * vertexPerParticle], bit_cast<u32>(DebugColorRed), vertexPerParticle);
	}
}

//----------------------------------------------------------------------------

void	CCopyStream_Exec_WireframeDiscard::operator()(const Drawers::SCopyStream_ExecPage &batch)
{
	if (m_DstSelectedParticles.Empty())
		return;

	PK_NAMEDSCOPEDPROFILE("CustomTasks CCopyStream_Exec_WireframeDiscard");

	const u32	outCount = batch.m_Page->RenderedParticleCount();

	Mem::Clear(&m_DstSelectedParticles[batch.m_ParticleOffset], sizeof(float) * outCount);

	TStridedMemoryView<const u8>	enableds = batch.m_Page->StreamForReading<bool>(batch.m_DrawRequest->BaseBillboardingRequest().m_EnabledStreamId);
	const bool						hasEnabled = batch.m_DrawRequest->InputParticleCount() != batch.m_DrawRequest->RenderedParticleCount();

	if (batch.m_DrawRequest->RenderedParticleCount() == 0)
		return;

	for (const auto &renderSelection : m_SrcParticleSelected.m_AllRendererSelectionsView)
	{
		if (renderSelection.m_Renderers.Contains(batch.m_DrawRequest->BaseBillboardingRequest()._UnsafeRenderer()))
		{
			Mem::Fill32(&m_DstSelectedParticles[batch.m_ParticleOffset], bit_cast<u32>(DebugColorGrey), outCount);
		}
	}

	CGuid	particleId_First;

	// Build the array of particle ranges for this page:
	TMemoryView<const SMediumParticleSelection>	&srcSelection = m_SrcParticleSelected.m_AllParticleSelectionsView;
	for (u32 i = 0; i < srcSelection.Count(); ++i)
	{
		if (batch.m_DrawRequest->BaseBillboardingRequest()._UnsafeMedium() == srcSelection[i].m_Medium)
		{
			PK_ASSERT(!srcSelection[i].m_Ranges.Empty());
			// Remap the particle Id from the simulation (in) to the particle id in the rendering buffers (out)
			u32		particleId_in = 0;
			u32		particleId_out = 0;

			u32		offset = 0;

			for (u32 j = 0; j < srcSelection[i].m_Ranges.Count(); ++j)
			{
				if (batch.m_Page->PageIdxInMedium() == srcSelection[i].m_Ranges[j].m_PageId)
				{
					const u32	range_startId = srcSelection[i].m_Ranges[j].StartID();
					const u32	range_count = srcSelection[i].m_Ranges[j].Count();

					u32		rangeOut_startId = range_startId;
					u32		rangeOut_count = range_count;
					if (hasEnabled)
					{
						PK_ASSERT(particleId_in <= range_startId); // Warning: it only works when "m_Ranges" are in ascending order and not overlapping. Otherwise, find another way !!!
						while (particleId_in < range_startId)
						{
							if (enableds[particleId_in] != 0)
								++particleId_out;
							++particleId_in;
						}
						rangeOut_startId = particleId_out;
						while (particleId_in < (range_startId + range_count))
						{
							if (enableds[particleId_in] != 0)
								++particleId_out;
							++particleId_in;
						}
						rangeOut_count = (particleId_out - rangeOut_startId);
					}

					if (rangeOut_count == 0)
						continue; // TODO: if it goes here with j == 0, then the first particle won't be hightlighted ...

					const bool	isFocusedMedium = (i == m_SrcParticleSelected.m_FocusedMedium);
					if (isFocusedMedium && srcSelection[i].m_CurrentSelectedId - offset < rangeOut_count)
						particleId_First = rangeOut_startId + srcSelection[i].m_CurrentSelectedId - offset;

					PK_ASSERT(rangeOut_startId + rangeOut_count <= outCount); // issue with mismatching between the current frame processed from frame-collector and the editor's selection input.
					const u32	writeCount = (rangeOut_startId < outCount) ? PKMin(rangeOut_count, outCount - rangeOut_startId) : 0;
					Mem::Fill32(&m_DstSelectedParticles[batch.m_ParticleOffset + rangeOut_startId], bit_cast<u32>((!m_SrcParticleSelected.m_FocusedMedium.Valid() || isFocusedMedium) ? DebugColorWhite : DebugColorGrey), writeCount);
				}
				offset += srcSelection[i].m_Ranges[j].Count();
			}
		}
	}

	if (particleId_First.Valid() && particleId_First < outCount)
	{
		m_DstSelectedParticles[batch.m_ParticleOffset + particleId_First] = DebugColorRed;
	}
}

//----------------------------------------------------------------------------

void	CRibbon_Exec_WireframeDiscard::operator()(const Drawers::SRibbon_ExecBatch &ribbonData)
{
	if (m_DstSelectedParticles.Empty())
		return;

	PK_NAMEDSCOPEDPROFILE("CustomTasks CRibbon_Exec_WireframeDiscard");

	PK_ASSERT(ribbonData.FullVertexOffset() + ribbonData.m_VertexCount <= m_DstSelectedParticles.Count());

	const u32	outCount = ribbonData.m_DrawRequest->m_DrawRequest->RenderedParticleCount();

	Mem::Clear(&m_DstSelectedParticles[ribbonData.FullVertexOffset()], sizeof(float) * ribbonData.m_VertexCount);

	if (outCount == 0)
		return;

	const Drawers::SRibbon_CPU_DrawRequest	*dr = ribbonData.m_DrawRequest;
	PK_ASSERT(dr->m_RibbonSorter);
	PK_ASSERT(dr->m_Billboarder);

	const u32	vertexOffset = ribbonData.FullVertexOffset();
	const u32	pageCount = dr->m_PageCaches.Count();

	// Flag selected particles

	for (const auto &renderSelection : m_SrcParticleSelected.m_AllRendererSelectionsView)
	{
		if (renderSelection.m_Renderers.Contains(dr->m_DrawRequest->BaseBillboardingRequest()._UnsafeRenderer()))
		{
			Mem::Fill32(&m_DstSelectedParticles[vertexOffset], bit_cast<u32>(DebugColorGrey), ribbonData.m_VertexCount);
		}
	}

	bool								particleFirstInitialized = false;
	CRibbonBillboarder::SCentersIndex	particleFirst;
	particleFirst.m_PageIndex = 0;
	particleFirst.m_PartIndex = 0;

	struct	SRange
	{
		SMediumParticleSelection::SSelectedRange	m_MediumRange;
		bool										m_IsFocusedMedium;

		SRange(SMediumParticleSelection::SSelectedRange range, bool isFocusedMedium = false) : m_MediumRange(range), m_IsFocusedMedium(isFocusedMedium) {}

		bool		operator < (const SRange &oth) const { return m_MediumRange.m_PageId < oth.m_MediumRange.m_PageId || (m_MediumRange.m_PageId == oth.m_MediumRange.m_PageId && m_MediumRange.m_Range < oth.m_MediumRange.m_Range); }
		bool		operator <= (const SRange &oth) const { return m_MediumRange.m_PageId < oth.m_MediumRange.m_PageId || (m_MediumRange.m_PageId == oth.m_MediumRange.m_PageId && m_MediumRange.m_Range <= oth.m_MediumRange.m_Range); }
		bool		operator == (const SRange &oth) const { return m_MediumRange.m_PageId == oth.m_MediumRange.m_PageId && m_MediumRange.m_Range == oth.m_MediumRange.m_Range; }
	};

	TSemiDynamicArray<SRange, 0x100>	currentParticleRanges;

	TMemoryView<const SMediumParticleSelection>	&srcSelection = m_SrcParticleSelected.m_AllParticleSelectionsView;
	for (u32 i = 0; i < srcSelection.Count(); ++i)
	{
		if (dr->m_DrawRequest->BaseBillboardingRequest()._UnsafeMedium() == srcSelection[i].m_Medium)
		{
			PK_ASSERT(!srcSelection[i].m_Ranges.Empty());

			u32		offset = 0;

			for (const SMediumParticleSelection::SSelectedRange &range : srcSelection[i].m_Ranges)
			{
				const u32		pageId_InMedium = range.PageID();
				CGuid			pageId_InTasks;
				for (u32 pageIdx = 0; pageIdx < pageCount; ++pageIdx)
				{
					if (dr->m_PageCaches[pageIdx].m_PageIdxInMedium == pageId_InMedium)
					{
						pageId_InTasks = pageIdx;
						break;
					}
				}
				if (!pageId_InTasks.Valid())
					continue; // page has been culled ...

				const bool	isFocusedMedium = (i == m_SrcParticleSelected.m_FocusedMedium);
				if (isFocusedMedium && srcSelection[i].m_CurrentSelectedId - offset < range.Count())
				{
					particleFirst.m_PageIndex = pageId_InTasks;
					particleFirst.m_PartIndex = range.StartID() + srcSelection[i].m_CurrentSelectedId - offset;
					particleFirstInitialized = true;
				}

				currentParticleRanges.PushBack(SRange(range, isFocusedMedium));
				currentParticleRanges.Last().m_MediumRange.m_PageId = pageId_InTasks;

				offset += range.Count();
			}
		}
	}

	if (currentParticleRanges.Empty())
		return; // nothing to do !!

	// optim : sort "currentParticleRanges" and extract sub-array foreach 'pageId' - so in the particle loop, the look-up count will be extremely reduced.

	QuickSort(currentParticleRanges.Begin(), currentParticleRanges.End());

	PK_STACKMEMORYVIEW(u32, rangeOffsetPerPage, pageCount + 1);
	Mem::Clear(rangeOffsetPerPage.Data(), rangeOffsetPerPage.CoveredBytes());

	{
		u32			rangeIdx = 0;
		const u32	rangeCount = currentParticleRanges.Count();
		for (u32 pageIdx = 0; pageIdx < pageCount; ++pageIdx)
		{
			rangeOffsetPerPage[pageIdx] = rangeIdx;
			while (rangeIdx < rangeCount && currentParticleRanges[rangeIdx].m_MediumRange.m_PageId == pageIdx)
				++rangeIdx;
		}
		rangeOffsetPerPage[pageCount] = rangeCount;
		PK_ASSERT(rangeIdx == rangeCount);
	}

	// Loop overall particles

	const u32	vertexPerParticle = dr->m_Billboarder->BillboardVertexCount();

	const TStridedMemoryView<const CRibbonBillboarder::SCentersIndex>	particleReorder = dr->m_RibbonSorter->m_OutSort_Indices.Slice(ribbonData.m_ParticleOffset, ribbonData.m_ParticleCount);
	const TStridedMemoryView<const u64>									particleRibbonId = dr->m_RibbonSorter->m_OutSort_RibbonIds.Slice(ribbonData.m_ParticleOffset, ribbonData.m_ParticleCount);

	for (u32 p = 0; p < ribbonData.m_ParticleCount; ++p)
	{
		const CRibbonBillboarder::SCentersIndex	&pIndexPL = particleReorder[p];

		bool	found = false;
		bool	isFocusedMedium = false;

		for (u32 rangeIdx = rangeOffsetPerPage[pIndexPL.m_PageIndex], rangeStop = rangeOffsetPerPage[pIndexPL.m_PageIndex + 1]; rangeIdx < rangeStop; ++rangeIdx)
		{
			const SRange	&range = currentParticleRanges[rangeIdx];
			const u32		startIdx = range.m_MediumRange.StartID();
			const u32		stopIdx = range.m_MediumRange.StartID() + range.m_MediumRange.Count();

			if (pIndexPL.m_PartIndex >= startIdx && pIndexPL.m_PartIndex < stopIdx)
			{
				found = true;
				isFocusedMedium = range.m_IsFocusedMedium;
				break;
			}
		}

		if (!found)
			continue;

		bool	previousFound = false;
		bool	nextFound = false;

		if (p < ribbonData.m_ParticleCount - 1)
			nextFound = (CRibbon_ThreadSort_Policy::kRibbonIdMask & particleRibbonId[p]) == (CRibbon_ThreadSort_Policy::kRibbonIdMask & particleRibbonId[p + 1]);

		if (p > 0)
		{
			const CRibbonBillboarder::SCentersIndex	&pIndexPLNext = particleReorder[p - 1];

			for (u32 rangeIdx = rangeOffsetPerPage[pIndexPLNext.m_PageIndex], rangeStop = rangeOffsetPerPage[pIndexPLNext.m_PageIndex + 1]; rangeIdx < rangeStop; ++rangeIdx)
			{
				const SRange	&range = currentParticleRanges[rangeIdx];
				const u32		startIdx = range.m_MediumRange.StartID();
				const u32		stopIdx = range.m_MediumRange.StartID() + range.m_MediumRange.Count();

				if (pIndexPLNext.m_PartIndex >= startIdx && pIndexPLNext.m_PartIndex < stopIdx)
				{
					previousFound = ((CRibbon_ThreadSort_Policy::kRibbonIdMask & particleRibbonId[p - 1]) == (CRibbon_ThreadSort_Policy::kRibbonIdMask & particleRibbonId[p]));
					break;
				}
			}
		}

		const u32	pVertex = vertexOffset + p * vertexPerParticle;

		Mem::Fill32(&m_DstSelectedParticles[pVertex], bit_cast<u32>((!m_SrcParticleSelected.m_FocusedMedium.Valid() || isFocusedMedium) ? DebugColorWhite : DebugColorGrey), vertexPerParticle);

		if (particleFirstInitialized && particleFirst.m_PageIndex == pIndexPL.m_PageIndex && particleFirst.m_PartIndex == pIndexPL.m_PartIndex)
		{
			const bool isFirst = !previousFound;
			const bool isLast = !nextFound;
			Mem::Fill32(&m_DstSelectedParticles[isFirst ? pVertex : pVertex - vertexPerParticle / 2], bit_cast<u32>(DebugColorRed), (isFirst || isLast) ? vertexPerParticle / 2 : vertexPerParticle);
		}
	}
}

//----------------------------------------------------------------------------

void	CMesh_Exec_WireframeDiscard::operator()(const Drawers::SMesh_ExecPage &batch)
{
	if (m_DstSelectedParticles.Empty())
		return;

	PK_NAMEDSCOPEDPROFILE("CustomTasks CMesh_Exec_WireframeDiscard");

	const u32	outCount = batch.m_Page->RenderedParticleCount();

	TStridedMemoryView<const u8>	enableds = batch.m_Page->StreamForReading<bool>(batch.m_DrawRequest->BaseBillboardingRequest().m_EnabledStreamId);
	const bool						hasEnabled = batch.m_DrawRequest->InputParticleCount() != batch.m_DrawRequest->RenderedParticleCount();

	if (batch.m_DrawRequest->RenderedParticleCount() == 0)
		return;

	PK_ASSERT(batch.m_Self != null);
	TMemoryView<const u32>	particleReorder = batch.m_Self->ParticleReorder();

	if (particleReorder.Empty())
	{
		Mem::Clear(&m_DstSelectedParticles[batch.m_ParticleOffset], sizeof(float) * outCount);

		for (const auto &renderSelection : m_SrcParticleSelected.m_AllRendererSelectionsView)
		{
			if (renderSelection.m_Renderers.Contains(batch.m_DrawRequest->BaseBillboardingRequest()._UnsafeRenderer()))
			{
				Mem::Fill32(&m_DstSelectedParticles[batch.m_ParticleOffset], bit_cast<u32>(DebugColorGrey), outCount);
			}
		}

		CGuid	particleId_First;

		TMemoryView<const SMediumParticleSelection>	&srcSelection = m_SrcParticleSelected.m_AllParticleSelectionsView;
		for (u32 i = 0; i < srcSelection.Count(); ++i)
		{
			if (batch.m_DrawRequest->BaseBillboardingRequest()._UnsafeMedium() == srcSelection[i].m_Medium)
			{
				PK_ASSERT(!srcSelection[i].m_Ranges.Empty());
				// Remap the particle Id from the simulation (in) to the particle id in the rendering buffers (out)
				u32		particleId_in = 0;
				u32		particleId_out = 0;

				u32		offset = 0;

				for (u32 j = 0; j < srcSelection[i].m_Ranges.Count(); ++j)
				{
					if (batch.m_Page->PageIdxInMedium() == srcSelection[i].m_Ranges[j].m_PageId)
					{
						const u32	range_startId = srcSelection[i].m_Ranges[j].StartID();
						const u32	range_count = srcSelection[i].m_Ranges[j].Count();

						u32		rangeOut_startId = range_startId;
						u32		rangeOut_count = range_count;
						if (hasEnabled)
						{
							PK_ASSERT(particleId_in <= range_startId); // Warning: it only works when "m_Ranges" are in ascending order and not overlapping. Otherwise, find another way !!!
							while (particleId_in < range_startId)
							{
								if (enableds[particleId_in] != 0)
									++particleId_out;
								++particleId_in;
							}
							rangeOut_startId = particleId_out;
							while (particleId_in < (range_startId + range_count))
							{
								if (enableds[particleId_in] != 0)
									++particleId_out;
								++particleId_in;
							}
							rangeOut_count = (particleId_out - rangeOut_startId);
						}

						if (rangeOut_count == 0)
							continue; // TODO: if it goes here with j == 0, then the first particle won't be hightlighted ...

						const bool	isFocusedMedium = (i == m_SrcParticleSelected.m_FocusedMedium);
						if (isFocusedMedium && srcSelection[i].m_CurrentSelectedId - offset < rangeOut_count)
							particleId_First = rangeOut_startId + srcSelection[i].m_CurrentSelectedId - offset;

						PK_ASSERT(rangeOut_startId + rangeOut_count <= outCount); // issue with mismatching between the current frame processed from frame-collector and the editor's selection input.
						const u32	writeCount = (rangeOut_startId < outCount) ? PKMin(rangeOut_count, outCount - rangeOut_startId) : 0;
						Mem::Fill32(&m_DstSelectedParticles[batch.m_ParticleOffset + rangeOut_startId], bit_cast<u32>((!m_SrcParticleSelected.m_FocusedMedium.Valid() || isFocusedMedium) ? DebugColorWhite : DebugColorGrey), writeCount);
					}
					offset += srcSelection[i].m_Ranges[j].Count();
				}
			}
		}

		if (particleId_First.Valid() && particleId_First < outCount)
		{
			m_DstSelectedParticles[batch.m_ParticleOffset + particleId_First] = DebugColorRed;
		}
	}
	else
	{
		struct SRangeAndFocusedMedium
		{
			SMediumParticleSelection::SRange	m_Range;
			bool								m_IsFocusedMedium;

			SRangeAndFocusedMedium(SMediumParticleSelection::SRange range, bool isFocusedMedium = false) : m_Range(range), m_IsFocusedMedium(isFocusedMedium) {}
		};

		TSemiDynamicArray<SRangeAndFocusedMedium, 0x100>	currentParticleRanges;  // 1kb of stack space ?!! I know we should hunt allocs down but isn't there a better way? that's pretty horrible

		for (const auto &renderSelection : m_SrcParticleSelected.m_AllRendererSelectionsView)
		{
			if (renderSelection.m_Renderers.Contains(batch.m_DrawRequest->BaseBillboardingRequest()._UnsafeRenderer()))
			{
				currentParticleRanges.PushBack(SMediumParticleSelection::SRange(0, batch.m_Page->InputParticleCount()));
			}
		}

		CGuid	particleId_First;

		TMemoryView<const SMediumParticleSelection>	&srcSelection = m_SrcParticleSelected.m_AllParticleSelectionsView;
		for (u32 i = 0; i < srcSelection.Count(); ++i)
		{
			if (batch.m_DrawRequest->BaseBillboardingRequest()._UnsafeMedium() == srcSelection[i].m_Medium)
			{
				PK_ASSERT(!srcSelection[i].m_Ranges.Empty());
				// Remap the particle Id from the simulation (in) to the particle id in the rendering buffers (out)
				u32		particleId_in = 0;
				u32		particleId_out = 0;

				u32		offset = 0;

				for (u32 j = 0; j < srcSelection[i].m_Ranges.Count(); ++j)
				{
					if (batch.m_Page->PageIdxInMedium() == srcSelection[i].m_Ranges[j].m_PageId)
					{
						const u32	range_startId = srcSelection[i].m_Ranges[j].StartID();
						const u32	range_count = srcSelection[i].m_Ranges[j].Count();

						u32		rangeOut_startId = range_startId;
						u32		rangeOut_count = range_count;
						if (hasEnabled)
						{
							PK_ASSERT(particleId_in <= range_startId); // Warning: it only works when "m_Ranges" are in ascending order and not overlapping. Otherwise, find another way !!!
							while (particleId_in < range_startId)
							{
								if (enableds[particleId_in] != 0)
									++particleId_out;
								++particleId_in;
							}
							rangeOut_startId = particleId_out;
							while (particleId_in < (range_startId + range_count))
							{
								if (enableds[particleId_in] != 0)
									++particleId_out;
								++particleId_in;
							}
							rangeOut_count = (particleId_out - rangeOut_startId);
						}

						if (rangeOut_count == 0)
							continue; // TODO: if it goes here with j == 0, then the first particle won't be hightlighted ...

						const bool	isFocusedMedium = (i == m_SrcParticleSelected.m_FocusedMedium);
						if (isFocusedMedium && srcSelection[i].m_CurrentSelectedId - offset < rangeOut_count)
							particleId_First = rangeOut_startId + srcSelection[i].m_CurrentSelectedId - offset;

						PK_ASSERT(rangeOut_startId + rangeOut_count <= outCount); // issue with mismatching between the current frame processed from frame-collector and the editor's selection input.
						const u32	writeCount = (rangeOut_startId < outCount) ? PKMin(rangeOut_count, outCount - rangeOut_startId) : 0;
						currentParticleRanges.PushBack(SRangeAndFocusedMedium(SMediumParticleSelection::SRange(rangeOut_startId, writeCount), isFocusedMedium));
					}
					offset += srcSelection[i].m_Ranges[j].Count();
				}
			}
		}

		// naive test
		u32		currentParticleIdx = batch.m_ParticleOffset;
		for (u32 p = 0; p < outCount; ++p)
		{
			const u32	pIndex = particleReorder[currentParticleIdx++];

			bool	found = false;
			bool	isFocusedMedium = false;
			for (const SRangeAndFocusedMedium &range : currentParticleRanges)
			{
				const u32	startIdx = range.m_Range.StartID();
				const u32	stopIdx = range.m_Range.StartID() + range.m_Range.Count();

				if (p >= startIdx && p < stopIdx)
				{
					found = true;
					isFocusedMedium = range.m_IsFocusedMedium;
					break;
				}
			}
			m_DstSelectedParticles[pIndex] = found ? ((!m_SrcParticleSelected.m_FocusedMedium.Valid() || isFocusedMedium) ? DebugColorWhite : DebugColorGrey) : 0.0f;
		}

		if (particleId_First.Valid() && particleId_First < outCount)
		{
			m_DstSelectedParticles[particleReorder[batch.m_ParticleOffset + particleId_First]] = DebugColorRed;
		}
	}
}

//----------------------------------------------------------------------------

void CTriangle_Exec_WireframeDiscard::operator()(const Drawers::STriangle_ExecPage &batch)
{
	if (m_DstSelectedParticles.Empty())
		return;

	PK_NAMEDSCOPEDPROFILE("CustomTasks CCopyStream_Exec_WireframeDiscard");

	const u32	outCount = batch.m_Page->RenderedParticleCount();

	Mem::Clear(&m_DstSelectedParticles[batch.m_VertexOffset], sizeof(float) * outCount * 3);

	TStridedMemoryView<const u8>	enableds = batch.m_Page->StreamForReading<bool>(batch.m_DrawRequest->BaseBillboardingRequest().m_EnabledStreamId);
	const bool						hasEnabled = batch.m_DrawRequest->InputParticleCount() != batch.m_DrawRequest->RenderedParticleCount();

	if (batch.m_DrawRequest->RenderedParticleCount() == 0)
		return;

	for (const auto &renderSelection : m_SrcParticleSelected.m_AllRendererSelectionsView)
	{
		if (renderSelection.m_Renderers.Contains(batch.m_DrawRequest->BaseBillboardingRequest()._UnsafeRenderer()))
		{
			Mem::Fill32(&m_DstSelectedParticles[batch.m_VertexOffset], bit_cast<u32>(DebugColorGrey), outCount * 3);
		}
	}

	CGuid	particleId_First;

	// Build the array of particle ranges for this page:
	TMemoryView<const SMediumParticleSelection>	&srcSelection = m_SrcParticleSelected.m_AllParticleSelectionsView;
	for (u32 i = 0; i < srcSelection.Count(); ++i)
	{
		if (batch.m_DrawRequest->BaseBillboardingRequest()._UnsafeMedium() == srcSelection[i].m_Medium)
		{
			PK_ASSERT(!srcSelection[i].m_Ranges.Empty());
			// Remap the particle Id from the simulation (in) to the particle id in the rendering buffers (out)
			u32		particleId_in = 0;
			u32		particleId_out = 0;

			u32		offset = 0;

			for (u32 j = 0; j < srcSelection[i].m_Ranges.Count(); ++j)
			{
				if (batch.m_Page->PageIdxInMedium() == srcSelection[i].m_Ranges[j].m_PageId)
				{
					const u32	range_startId = srcSelection[i].m_Ranges[j].StartID();
					const u32	range_count = srcSelection[i].m_Ranges[j].Count();

					u32		rangeOut_startId = range_startId;
					u32		rangeOut_count = range_count;
					if (hasEnabled)
					{
						PK_ASSERT(particleId_in <= range_startId); // Warning: it only works when "m_Ranges" are in ascending order and not overlapping. Otherwise, find another way !!!
						while (particleId_in < range_startId)
						{
							if (enableds[particleId_in] != 0)
								++particleId_out;
							++particleId_in;
						}
						rangeOut_startId = particleId_out;
						while (particleId_in < (range_startId + range_count))
						{
							if (enableds[particleId_in] != 0)
								++particleId_out;
							++particleId_in;
						}
						rangeOut_count = (particleId_out - rangeOut_startId);
					}

					if (rangeOut_count == 0)
						continue; // TODO: if it goes here with j == 0, then the first particle won't be hightlighted ...

					const bool	isFocusedMedium = (i == m_SrcParticleSelected.m_FocusedMedium);
					if (isFocusedMedium && srcSelection[i].m_CurrentSelectedId - offset < rangeOut_count)
						particleId_First = rangeOut_startId + srcSelection[i].m_CurrentSelectedId - offset;

					PK_ASSERT(rangeOut_startId + rangeOut_count <= outCount); // issue with mismatching between the current frame processed from frame-collector and the editor's selection input.
					const u32	writeCount = (rangeOut_startId < outCount) ? PKMin(rangeOut_count, outCount - rangeOut_startId) : 0;
					Mem::Fill32(&m_DstSelectedParticles[batch.m_VertexOffset + rangeOut_startId * 3], bit_cast<u32>((!m_SrcParticleSelected.m_FocusedMedium.Valid() || isFocusedMedium) ? DebugColorWhite : DebugColorGrey), writeCount * 3);
				}
				offset += srcSelection[i].m_Ranges[j].Count();
			}
		}
	}

	if (particleId_First.Valid() && particleId_First < outCount)
	{
		Mem::Fill32(&m_DstSelectedParticles[batch.m_VertexOffset + particleId_First * 3], bit_cast<u32>(DebugColorRed), 3);
	}
}

//----------------------------------------------------------------------------

RHI::PGpuBuffer	 GetIsSelectedBuffer(const SEffectParticleSelectionView &selectionView, const Drawers::SBillboard_DrawRequest &dr)
{
	for (const auto &s : selectionView.m_AllParticleSelectionsView_GPU)
	{
		if (s.m_Selection->m_Medium == dr.m_BB._UnsafeMedium())
			return s.m_DstBuffer;
	}
	return null;
}

//----------------------------------------------------------------------------

RHI::PGpuBuffer	 GetIsSelectedBuffer(const SEffectParticleSelectionView &selectionView, const Drawers::SMesh_DrawRequest &dr)
{
	for (const auto &s : selectionView.m_AllParticleSelectionsView_GPU)
	{
		if (s.m_Selection->m_Medium == dr.m_BB._UnsafeMedium())
			return s.m_DstBuffer;
	}
	return null;
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END

#endif	// (PK_HAS_PARTICLES_SELECTION != 0)
