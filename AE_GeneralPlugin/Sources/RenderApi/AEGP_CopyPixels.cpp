//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#include "ae_precompiled.h"
#include "RenderApi/AEGP_CopyPixels.h"

//Samples
#include <PK-SampleLib/SampleUtils.h>
#include <pk_kernel/include/kr_refcounted_buffer.h>

#include "AEGP_AEPKConversion.h"

__AEGP_PK_BEGIN

//----------------------------------------------------------------------------

CFloat4		Pixel32ToCFloat4(const PF_Pixel32 &pxl)
{
	return CFloat4(pxl.red, pxl.green, pxl.blue, pxl.alpha);
}

//----------------------------------------------------------------------------

CFloat4		Pixel16ToCFloat4(const PF_Pixel16 &pxl)
{
	CFloat4		pxlValue(	static_cast<float>(pxl.red),
							static_cast<float>(pxl.green),
							static_cast<float>(pxl.blue),
							static_cast<float>(pxl.alpha));
	return pxlValue / CFloat4(static_cast<float>(0x7FFF));
}

//----------------------------------------------------------------------------

CFloat4		Pixel8ToCFloat4(const PF_Pixel8 &pxl)
{
	CFloat4		pxlValue(	static_cast<float>(pxl.red),
							static_cast<float>(pxl.green),
							static_cast<float>(pxl.blue),
							static_cast<float>(pxl.alpha));
	return pxlValue / CFloat4(static_cast<float>(0xFF));
}

//----------------------------------------------------------------------------

PF_Pixel32		CFloat4ToPixel32(const CFloat4 &pxl)
{
	PF_Pixel32	out = {};
	out.red = pxl.x();
	out.green = pxl.y();
	out.blue = pxl.z();
	out.alpha = pxl.w();
	return out;
}

//----------------------------------------------------------------------------

PF_Pixel16		CFloat4ToPixel16(const CFloat4 &pxl)
{
	PF_Pixel16	out = {};
	out.red = static_cast<A_u_short>(pxl.x() * static_cast<float>(0x7FFF));
	out.green = static_cast<A_u_short>(pxl.y() * static_cast<float>(0x7FFF));
	out.blue = static_cast<A_u_short>(pxl.z() * static_cast<float>(0x7FFF));
	out.alpha = static_cast<A_u_short>(pxl.w() * static_cast<float>(0x7FFF));
	return out;
}

//----------------------------------------------------------------------------

PF_Pixel8		CFloat4ToPixel8(const CFloat4 &pxl)
{
	PF_Pixel8	out = {};
	out.red = static_cast<A_u_char>(pxl.x() * static_cast<float>(0xFF));
	out.green = static_cast<A_u_char>(pxl.y() * static_cast<float>(0xFF));
	out.blue = static_cast<A_u_char>(pxl.z() * static_cast<float>(0xFF));
	out.alpha = static_cast<A_u_char>(pxl.w() * static_cast<float>(0xFF));
	return out;
}

//----------------------------------------------------------------------------

PF_Err	CopyPixelIn32(	void		*refcon,
						A_long		x,
						A_long		y,
						PF_Pixel32	*inP,
						PF_Pixel32	*)
{
	SCopyPixel			*thiS = reinterpret_cast<SCopyPixel*>(refcon);
	CFloat4				*outP = Mem::AdvanceRawPointer(thiS->m_BufferPtr->Data<CFloat4>(), sizeof(CFloat4) * y * thiS->m_InputWorld->width + x * sizeof(CFloat4));
	CFloat4				value = Pixel32ToCFloat4(*inP);

	value.xyz() = PKSample::ConvertSRGBToLinear(value.xyz());
	value = PKSaturate(value);
	*outP = value;
	if (thiS->m_IsAlphaOverride)
		outP->w() = static_cast<float>(thiS->m_AlphaOverrideValue);
	return PF_Err_NONE;
}

//----------------------------------------------------------------------------

PF_Err	CopyPixelIn16(	void			*refcon,
						A_long			x,
						A_long			y,
						PF_Pixel16		*inP,
						PF_Pixel16		*)
{
	SCopyPixel			*thiS = reinterpret_cast<SCopyPixel*>(refcon);
	CFloat4				*outP = Mem::AdvanceRawPointer(thiS->m_BufferPtr->Data<CFloat4>(), sizeof(CFloat4) * y * thiS->m_InputWorld->width + x * sizeof(CFloat4));
	CFloat4				value = Pixel16ToCFloat4(*inP);

	value.xyz() = PKSample::ConvertSRGBToLinear(value.xyz());
	value = PKSaturate(value);
	*outP = value;
	if (thiS->m_IsAlphaOverride)
		outP->w() = static_cast<float>(thiS->m_AlphaOverrideValue);
	return PF_Err_NONE;
}

//----------------------------------------------------------------------------

PF_Err	CopyPixelIn8(	void			*refcon,
						A_long			x,
						A_long			y,
						PF_Pixel8		*inP,
						PF_Pixel8		*)
{
	SCopyPixel			*thiS = reinterpret_cast<SCopyPixel*>(refcon);
	CFloat4				*outP = Mem::AdvanceRawPointer(thiS->m_BufferPtr->Data<CFloat4>(), sizeof(CFloat4) * y * thiS->m_InputWorld->width + x * sizeof(CFloat4));
	CFloat4				value = Pixel8ToCFloat4(*inP);

	value.xyz() = PKSample::ConvertSRGBToLinear(value.xyz());
	value = PKSaturate(value);
	*outP = value;
	if (thiS->m_IsAlphaOverride)
		outP->w() = static_cast<float>(thiS->m_AlphaOverrideValue);
	return PF_Err_NONE;
}

//----------------------------------------------------------------------------

PF_Err	CopyPixelOut32(	void			*refcon,
						A_long			x,
						A_long			y,
						PF_Pixel32		*,
						PF_Pixel32		*outP)
{
	SCopyPixel				*thiS = reinterpret_cast<SCopyPixel*>(refcon);
	const u32				size = sizeof(CFloat4);
	const CFloat4			*inP = Mem::AdvanceRawPointer(thiS->m_BufferPtr->Data<CFloat4>(), size * y * thiS->m_InputWorld->width + x * size);
	CFloat4					value = CFloat4(PKSample::ConvertLinearToSRGB(inP->xyz()), inP->w());
	value = PKSaturate(value);
	*outP = CFloat4ToPixel32(value);
	return PF_Err_NONE;
}

//----------------------------------------------------------------------------

PF_Err	CopyPixelOut16(	void			*refcon,
						A_long			x,
						A_long			y,
						PF_Pixel16		*,
						PF_Pixel16		*outP)
{
	SCopyPixel				*thiS = reinterpret_cast<SCopyPixel*>(refcon);
	const u32				size = sizeof(CFloat4);
	const CFloat4			*inP = Mem::AdvanceRawPointer(thiS->m_BufferPtr->Data<CFloat4>(), size * y * thiS->m_InputWorld->width + x * size);
	CFloat4					value = CFloat4(PKSample::ConvertLinearToSRGB(inP->xyz()), inP->w());
	value = PKSaturate(value);
	*outP = CFloat4ToPixel16(value);
	return PF_Err_NONE;
}

//----------------------------------------------------------------------------

PF_Err	CopyPixelOut8(	void			*refcon,
						A_long			x,
						A_long			y,
						PF_Pixel8		*,
						PF_Pixel8		*outP)
{
	SCopyPixel				*thiS = reinterpret_cast<SCopyPixel*>(refcon);
	const u32				size = sizeof(CFloat4);
	const CFloat4			*inP = Mem::AdvanceRawPointer(thiS->m_BufferPtr->Data<CFloat4>(), size * y * thiS->m_InputWorld->width + x * size);
	CFloat4					value = CFloat4(PKSample::ConvertLinearToSRGB(inP->xyz()), inP->w());
	value = PKSaturate(value);
	*outP = CFloat4ToPixel8(value);
	return PF_Err_NONE;
}

//----------------------------------------------------------------------------
__AEGP_PK_END

