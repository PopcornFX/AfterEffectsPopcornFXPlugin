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

#include "PKSample.h"

#include <pk_maths/include/pk_maths.h>
#include <pk_maths/include/pk_maths_matrix.h>
#include <pk_maths/include/pk_maths_quaternion.h>
#include <pk_maths/include/pk_maths_primitives_frustum.h>
#include <pk_kernel/include/kr_units.h>
#include <pk_rhi/include/Enums.h>

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

struct	SCamera
{
	CFloat4x4			m_View;
	CFloat4x4			m_Proj; // Embeds the User to RHYup matrix
	CFloat3				m_Position;
	CFloat4x4			m_ViewInv;
	CFloat4x4			m_ProjInv;
	CFloat2				m_WinSize;
	float				m_ProjFovy;
	float				m_ProjNear;
	float				m_ProjFar;
	CAABB				m_ClipSpaceLimits; // (.x: side, .y: vertical, .z: depth)

	CFloat3				Ray(const CFloat2 &mousePosPixel) const;
	const CFloat3		&Position() const { return m_Position; }
	const CFloat4x4		&Proj() const { return m_Proj; }
	const CFloat4x4		&View() const { return m_View; }
	const CFloat2		WindowSize() const { return m_WinSize; }
	const CFloat2		ZLimits() const { return CFloat2(m_ProjNear, m_ProjFar); }
	const CAABB			ClipSpaceLimits() const { return m_ClipSpaceLimits; }

	void				Update()
	{
		m_ViewInv = m_View.Inverse();
		m_ProjInv = m_Proj.Inverse();
	}

	SCamera()
	:	m_View(CFloat4x4::IDENTITY)
	,	m_Proj(CFloat4x4::IDENTITY)
	,	m_Position(0)
	,	m_ViewInv(CFloat4x4::IDENTITY)
	,	m_ProjInv(CFloat4x4::IDENTITY)
	,	m_WinSize(0)
	,	m_ProjFovy(0)
	,	m_ProjNear(0)
	,	m_ProjFar(0)
	,	m_ClipSpaceLimits(CAABB::NORMALIZED_M1P1)
	{
	}
};

//----------------------------------------------------------------------------

class	CCameraBase
{
public:
	CCameraBase();

	const SCamera		&Camera() const { return m_Cam; }

	void				SetLookAt(const CFloat3 &lookAt) { m_LookAt = lookAt; m_NeedUpdate = true; }
	void				SetPosition(const CFloat3 &position)  { m_Cam.m_Position = position; m_NeedUpdate = true; }

	void				OffsetPosition(const CFloat3 &offset) { m_Cam.m_Position += offset; m_NeedUpdate = true; }

	// TODO: Change this, don't pass vertical FOV, this is a shortcut that causes bad behavior in almost every real-life use case when used in UI.
	// openGLYFlipSign: set to '1' to revert the hardcoded OpenGL projection matrix Y axis flip. PopcornFX Editor relies on a flipped proj matrix in OpenGL.
	void				SetProj(float fovyDegrees, const CFloat2 &winDimPixel, float zNear, float zFar, RHI::EGraphicalApi api = RHI::GApi_Vulkan, float openGLYFlipSign = -1); // perspective-projection
	void				SetProj(const CFloat2 &winDimPixel, float zNear, float zFar, RHI::EGraphicalApi api = RHI::GApi_Vulkan, float openGLYFlipSign = -1); // othogonal-projection

	bool				NeedUpdate() const { return m_NeedUpdate; }
	const CFloat3		&Position() const { return m_Cam.m_Position; }
	const CFloat3		&LookAt() const { return m_LookAt; }
	const CFloat4x4		&View() const { return m_Cam.View(); }
	const CFloat4x4		&Proj() const { return m_Cam.Proj(); }
	CFloat2				ZLimits() const { return m_Cam.ZLimits(); }
	float				BoundingZOffset(float radius, bool useLargestFov) const;
	const CAABB			ClipSpaceLimits() const { return m_Cam.ClipSpaceLimits(); }
	bool				IsOrthographic() const { return m_Cam.m_ProjFovy == 0.f; }

	CFloat2				WindowSize() const { return m_Cam.WindowSize(); }

protected:
	SCamera				m_Cam;
	bool				m_NeedUpdate;
	CFloat3				m_LookAt;
};

//----------------------------------------------------------------------------

class	CCameraRotate : public CCameraBase
{
public:
	CCameraRotate();

	void				Update();
	void				UpdateIFN() { if (m_NeedUpdate) Update(); }

	// 2D angles are in { yaw, pitch } order (screenspace mouse-order)
	void				SetAngles(const CFloat2 &angles) { SetAngles(angles.yx0()); }
	void				OffsetAngles(const CFloat2 &offset) { OffsetAngles(offset.yx0()); }

	// 3D angles are in { pitch, yaw, roll } order
	void				SetAngles(const CFloat3 &angles) { m_Angles = angles; m_NeedUpdate = true; }
	void				OffsetAngles(const CFloat3 &offset) { m_Angles += offset; m_NeedUpdate = true; }

	void				SetDistance(float distance) { m_Distance = distance; m_NeedUpdate = true; }
	void				OffsetDistance(float offset) { m_Distance += offset; m_NeedUpdate = true; }

	const CFloat3		&Angles() const { return m_Angles; }
	float				Distance() const { return m_Distance; }

protected:
	float				m_Distance;
	CFloat3				m_Angles;
};

//----------------------------------------------------------------------------

class	CCameraFps : public CCameraBase
{
public:
	CCameraFps();

	void				Update();
	void				UpdateIFN() { if (m_NeedUpdate) Update(); }

	// 2D angles are in { yaw, pitch } order (screenspace mouse-order)
	void				SetAngles(const CFloat2 &angles) { SetAngles(angles.yx0()); }
	void				OffsetAngles(const CFloat2 &offset) { OffsetAngles(offset.yx0()); }

	// 3D angles are in { pitch, yaw, roll } order
	void				SetAngles(const CFloat3 &angles) { m_Angles = angles; m_NeedUpdate = true; }
	void				OffsetAngles(const CFloat3 &offset) { m_Angles += offset; m_NeedUpdate = true; }

	void				ViewOffsetPositionRUF(const CFloat3 &offsetRUF);	// Offset is expected to be in Right, Up, Forward format

	// pitch, yaw, roll euler angles
	const CFloat3		&Angles() const { return m_Angles; }

protected:
	CFloat3				m_Angles;
};

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
