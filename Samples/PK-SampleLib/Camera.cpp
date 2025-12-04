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

#include "Camera.h"

#include <pk_geometrics/include/ge_matrix_tools.h>
#include <pk_geometrics/include/ge_coordinate_frame.h>

#include <cmath>

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

CFloat3	SCamera::Ray(const CFloat2 &mousePosPixel) const
{
	CFloat4				pos(((2.f * mousePosPixel / m_WinSize) - 1.f) * CFloat2(1.f, -1.f), -1.f, 1.f);
	const CFloat4x4		&invView = m_ViewInv;
	const CFloat4x4		invProj = m_ProjInv;
	pos = invProj.TransformVector(pos);
	pos = invView.RotateVector(pos);
	return pos.xyz().Normalized();
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

CCameraBase::CCameraBase()
:	m_NeedUpdate(true)
,	m_LookAt(0.f)
{
}

//----------------------------------------------------------------------------

void	CCameraBase::SetProj(float fovyDegrees, const CFloat2 &winDimPixel, float zNear, float zFar, RHI::EGraphicalApi api /* = GApi_Vulkan */, float openGLYFlipSign/* = -1*/)
{
	m_Cam.m_ProjFovy = fovyDegrees;
	m_Cam.m_ProjNear = zNear;
	m_Cam.m_ProjFar = zFar;
	m_Cam.m_WinSize = winDimPixel;

	const float	aspect = winDimPixel.x() / winDimPixel.y();
	const float	fY = 1.f / tanf(Units::DegreesToRadians(fovyDegrees * 0.5f));
	const float	fX = fY / aspect;

	const float	kRcpRange = 1.0f / (zNear - zFar);
	const float	kA = (zFar + zNear) * kRcpRange;
	const float	kB = (zFar * zNear) * kRcpRange;

	if (api == RHI::GApi_OpenGL || api == RHI::GApi_OES)
	{
		// OpenGL:
		const float	_fY = fY * openGLYFlipSign;
		m_Cam.m_Proj = CFloat4x4(	fX, 0, 0, 0,
									0, _fY, 0, 0,
									0, 0, kA, -1,
									0, 0, 2 * kB, 0);
		m_Cam.m_ClipSpaceLimits = CAABB::NORMALIZED_M1P1;
	}
	else if (api == RHI::GApi_Orbis || api == RHI::GApi_UNKNOWN2)
	{
		// Orbis:
		m_Cam.m_Proj = CFloat4x4(	fX, 0, 0, 0,
									0, fY, 0, 0,
									0, 0, kA, -1,
									0, 0, 2 * kB, 0);
		m_Cam.m_ClipSpaceLimits = CAABB::NORMALIZED_M1P1;
	}
	else if (api == RHI::GApi_Vulkan || api == RHI::GApi_D3D11 || api == RHI::GApi_D3D12 || api == RHI::GApi_Metal)
	{
		// Vulkan:
		m_Cam.m_Proj = CFloat4x4(	fX, 0, 0, 0,
									0, -fY, 0, 0,
									0, 0, 0.5f * kA - 0.5f, -1,
									0, 0, kB, 0);
		m_Cam.m_ClipSpaceLimits = CAABB(CFloat3(-1.f, -1.f, 0.f), CFloat3::ONE);
	}
	else
	{
		PK_ASSERT_NOT_REACHED_MESSAGE("No projection matrix for this API");
	}

	// Patch the projection matrix only
	CFloat4x4		current2RHYUp = CFloat4x4::IDENTITY;
	CCoordinateFrame::BuildTransitionFrame(CCoordinateFrame::GlobalFrame(), Frame_RightHand_Y_Up, current2RHYUp);

	m_Cam.m_Proj = current2RHYUp * m_Cam.m_Proj;

	m_NeedUpdate = true;
}

//----------------------------------------------------------------------------

void	CCameraBase::SetProj(const CFloat2 &winDimPixel, float zNear, float zFar, RHI::EGraphicalApi api /* = GApi_Vulkan */, float openGLYFlipSign/* = -1*/)
{
	m_Cam.m_ProjNear = zNear;
	m_Cam.m_ProjFar = zFar;
	m_Cam.m_WinSize = winDimPixel;

	const float	aspect = winDimPixel.x() / winDimPixel.y();
	const float	fY = 1.f;
	const float	fX = fY / aspect;

	const float	kRcpRange = 1.0f / (zNear - zFar);
	const float	kA = 2.f * kRcpRange;
	const float	kB = (zFar + zNear) * kRcpRange;

	if (api == RHI::GApi_OpenGL || api == RHI::GApi_OES)
	{
		// OpenGL:
		const float	_fY = fY * openGLYFlipSign;
		m_Cam.m_Proj = CFloat4x4(	fX, 0, 0, 0,
									0, _fY, 0, 0,
									0, 0, kA, 0,
									0, 0, kB, 1);
		m_Cam.m_ClipSpaceLimits = CAABB::NORMALIZED_M1P1;
	}
	else if (api == RHI::GApi_Orbis || api == RHI::GApi_UNKNOWN2)
	{
		// Orbis:
		m_Cam.m_Proj = CFloat4x4(	fX, 0, 0, 0,
									0, fY, 0, 0,
									0, 0, kA, 0,
									0, 0, kB, 1);
		m_Cam.m_ClipSpaceLimits = CAABB::NORMALIZED_M1P1;
	}
	else if (api == RHI::GApi_Vulkan || api == RHI::GApi_D3D11 || api == RHI::GApi_D3D12 || api == RHI::GApi_Metal)
	{
		// Vulkan:
		m_Cam.m_Proj = CFloat4x4(	fX, 0, 0, 0,
									0, -fY, 0, 0,
									0, 0, kRcpRange, 0,
									0, 0, zNear * kRcpRange, 1);
		m_Cam.m_ClipSpaceLimits = CAABB(CFloat3(-1.f, -1.f, 0.f), CFloat3::ONE);
	}
	else
	{
		PK_ASSERT_NOT_REACHED_MESSAGE("No projection matrix for this API");
	}

	// Patch the projection matrix only
	CFloat4x4		current2RHYUp = CFloat4x4::IDENTITY;
	CCoordinateFrame::BuildTransitionFrame(CCoordinateFrame::GlobalFrame(), Frame_RightHand_Y_Up, current2RHYUp);

	m_Cam.m_Proj = current2RHYUp * m_Cam.m_Proj;

	m_NeedUpdate = true;
}

//----------------------------------------------------------------------------

float	CCameraBase::BoundingZOffset(float radius, bool useLargestFov) const
{
	float	aspect = m_Cam.m_WinSize.y() / m_Cam.m_WinSize.x();
	if (aspect == 0.0f || !TNumericTraits<float>::IsFinite(aspect))
		aspect = 1.0f;

#if 0
		// NOTE: v1.x, how do we want to handle non-square screen pixels in v2 ?
		if (!m_IgnoreScreenPixelAspect)
		{
			if (m_ScreenPixelAspect != 0.0f)
				aspect *= m_ScreenPixelAspect;//Caps::Monitors().ScreenPhysicalPixelAspect();
			else
				PK_ASSERT_NOT_REACHED_MESSAGE("You must set the screenPixelAspect in CBaseCamera before that call");
		}
#endif

	const float	fovY = m_Cam.m_ProjFovy;
	const float	fovX = fovY / aspect;
	const float	fov = useLargestFov ? PKMax(fovX, fovY) : PKMin(fovX, fovY);
	PK_ASSERT(fov != 0);

	// here, get the distance from the sphere's center that makes 'fov' bind the sphere:
	const float		f = tanf(TNumericConstants<float>::PiHalf() - fov * TNumericConstants<float>::PiHalf() / 180.0f);
	return radius * sqrtf(f * f + 1.0f);
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

CCameraRotate::CCameraRotate()
:	m_Distance(0)
,	m_Angles(0)
{
}

//----------------------------------------------------------------------------

void	CCameraRotate::Update()
{
	CFloat4x4	outMatrix;
	CFloat4x4	pitch;
	CFloat4x4	yaw;
	CFloat4x4	roll;

	const CInt3		&axesRemapper = CCoordinateFrame::AxesRemapper(CCoordinateFrame::GlobalFrame());
	const CUint3	axesIndexer = PKAbs(axesRemapper) - 1;
	const CFloat3	signs = CFloat3((axesRemapper >> 31) | 1) * CCoordinateFrame::CrossSign();

	MatrixTools::BuildRotationMatrixAxis(axesIndexer.x(), m_Angles.x() * signs.x(), pitch);
	MatrixTools::BuildRotationMatrixAxis(axesIndexer.y(), m_Angles.y() * signs.y(), yaw);
	MatrixTools::BuildRotationMatrixAxis(axesIndexer.z(), m_Angles.z() * signs.z(), roll);

	outMatrix = (yaw * pitch) * roll;

	// FIXME(Julien): This is just plain wrong, and countless unneeded matrix inversions.
	// We should build the WORLD matrix. And use it to compute the VIEW matrix.
	// The view matrix converts from world to view, the world matrix converts from view to world,
	// and is more straightforward to compute. Fix this

	CFloat4x4	worldMatrix = outMatrix.Inverse();
	worldMatrix.StrippedTranslations() = m_LookAt - CCoordinateFrame::MatrixAxis(worldMatrix, Axis_Forward) * m_Distance;

	m_Cam.m_View = worldMatrix.Inverse();

	m_Cam.Update();

	m_Cam.m_Position = m_Cam.m_ViewInv.StrippedTranslations();

	m_NeedUpdate = false;
}

//----------------------------------------------------------------------------
//
// CCameraFps
//
//----------------------------------------------------------------------------

CCameraFps::CCameraFps()
:	m_Angles(0)
{
}

//----------------------------------------------------------------------------

void	CCameraFps::Update()
{
	m_NeedUpdate = false;

	CFloat4x4	outMatrix;
	CFloat4x4	pitch;
	CFloat4x4	yaw;
	CFloat4x4	roll;

	const CInt3		&axesRemapper = CCoordinateFrame::AxesRemapper(CCoordinateFrame::GlobalFrame());
	const CUint3	axesIndexer = PKAbs(axesRemapper) - 1;
	const CFloat3	signs = CFloat3((axesRemapper >> 31) | 1) * CCoordinateFrame::CrossSign();

	MatrixTools::BuildRotationMatrixAxis(axesIndexer.x(), m_Angles.x() * signs.x(), pitch);
	MatrixTools::BuildRotationMatrixAxis(axesIndexer.y(), m_Angles.y() * signs.y(), yaw);
	MatrixTools::BuildRotationMatrixAxis(axesIndexer.z(), m_Angles.z() * signs.z(), roll);

	outMatrix = (yaw * pitch) * roll;

	// FIXME(Julien): This is just plain wrong, and countless unneeded matrix inversions.
	// We should build the WORLD matrix. And use it to compute the VIEW matrix.
	// The view matrix converts from world to view, the world matrix converts from view to world,
	// and is more straightforward to compute. Fix this

	CFloat4x4	matTr = CFloat4x4::IDENTITY;
	matTr.StrippedTranslations() = -m_Cam.m_Position;

	outMatrix = matTr * outMatrix;

	m_Cam.m_View = outMatrix;

	m_Cam.Update();
}

//----------------------------------------------------------------------------

void	CCameraFps::ViewOffsetPositionRUF(const CFloat3 &offsetRUF)
{
	if (offsetRUF == CFloat3(0))
		return;

	const CFloat3	worldOffset =	offsetRUF.x() * CCoordinateFrame::MatrixAxis(m_Cam.m_ViewInv, Axis_Right) +
									offsetRUF.y() * CCoordinateFrame::MatrixAxis(m_Cam.m_ViewInv, Axis_Up) +
									offsetRUF.z() * CCoordinateFrame::MatrixAxis(m_Cam.m_ViewInv, Axis_Forward);

	m_Cam.m_Position += worldOffset;
	m_NeedUpdate = true;
}

//----------------------------------------------------------------------------

__PK_SAMPLE_API_END
