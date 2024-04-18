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

#include <pk_rhi/include/FwdInterfaces.h>
#include <pk_rhi/include/interfaces/IFrameBuffer.h>

#include <pk_maths/include/pk_maths_primitives.h>

#if	defined(PK_DESKTOP) && !defined(PK_RETAIL)
#define		PK_HAS_GIZMO_STD_FUNCTIONS
#include <functional>
#include <pk_base_object/include/hbo_object.h>
#endif

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

class	CCameraBase;
class	CShaderLoader;

//----------------------------------------------------------------------------

enum	EGizmoType
{
	GizmoNone					= 0x00,
	GizmoTranslating			= 0x01,
	GizmoRotating				= 0x02,
	GizmoScaling				= 0x04,
	GizmoShowAxis				= 0x10,
	GizmoShowAxisWithoutNames	= 0x20,
};

//----------------------------------------------------------------------------

struct	SGizmoGeometryDesc
{
	float	m_RootSize;
	float	m_AxisLength;
	float	m_AxisWidth;
	float	m_SquareDist;
	float	m_SquareSize;
	float	m_ArrowHeight;
	float	m_ArrowWidth;

	SGizmoGeometryDesc()
	:	m_RootSize(0.10f)
	,	m_AxisLength(0.75f)
	,	m_AxisWidth(0.02f)
	,	m_SquareDist(0.05f)
	,	m_SquareSize(0.25f)
	,	m_ArrowHeight(0.25f)
	,	m_ArrowWidth(0.08f)
	{
	}

	bool	operator == (const SGizmoGeometryDesc &other) const
	{
		return	PKAbs(m_RootSize - other.m_RootSize) < (1.e-3f * m_RootSize) &&
				PKAbs(m_AxisLength - other.m_AxisLength) < (1.e-3f * m_AxisLength) &&
				PKAbs(m_AxisWidth - other.m_AxisWidth) < (1.e-3f * m_AxisWidth) &&
				PKAbs(m_SquareDist - other.m_SquareDist) < (1.e-3f * m_SquareDist) &&
				PKAbs(m_SquareSize - other.m_SquareSize) < (1.e-3f * m_SquareSize) &&
				PKAbs(m_ArrowHeight - other.m_ArrowHeight) < (1.e-3f * m_ArrowHeight) &&
				PKAbs(m_ArrowWidth - other.m_ArrowWidth) < (1.e-3f * m_ArrowWidth);
	}
};

//----------------------------------------------------------------------------

class	CGizmo
{
public:
	CGizmo(const SGizmoGeometryDesc &geomDesc = SGizmoGeometryDesc());
	~CGizmo() {}

	void			SetMode(EGizmoType mode) { if (!IsGrabbed()) m_Type = mode; }
	EGizmoType		Mode() const { return m_Type; }
	void			SetLocalFrame(bool enabled) { if (m_FrameLocal != enabled) { m_FrameLocal = enabled; _TransformFromOutput(); }; }
	void			SetParentTransform(const CFloat4x4 &transform);		// Set the "world-space" (ie. global-space)
	void			Disable() { if (!IsGrabbed()) m_Type = GizmoNone; }
	void			ClearOutputToUpdate();								// Reset gizmo reference to its outputs
	bool			PushBackObject(CFloat4x4 *transform, u32 allowedModesMask = GizmoTranslating | GizmoRotating | GizmoScaling); // Bind the "transform" with the gizmo.
#ifdef PK_HAS_GIZMO_STD_FUNCTIONS
	typedef			std::function<void(EGizmoType, const CFloat4x4 &)> fnObjectUpdate;
	bool			PushBackObject(const CFloat4x4 &transform, const fnObjectUpdate &updateFromTransform,  u32 allowedModesMask = GizmoTranslating | GizmoRotating | GizmoScaling); // Bind a transform and a update-function with the gizmo.
	bool			PushBackObject(CBaseObject *object_position, u32 fieldID_position, CBaseObject *object_eulerOrientation, u32 fieldID_eulerOrientation); // Helper with HBO Objects
#endif
	bool			IsGrabbed() const { return m_Clicked; }
	bool			TransformChanged() const { return m_TransformChanged; }
	void			ApplyGizmoTransform(); // On this call, for all bound objects, their transform is written to the new values (or the update-function is called with the new values).

	// Events
	void			UpdateCameraInfo(const CCameraBase &camera);
	void			SetMouseClicked(const CInt2 &mousePosition, bool clicked);
	void			SetMouseMoved(const CInt2 &mousePosition);

	float			&GizmoSelfScale() { return m_SelfScale; }

private:
	// MATH UTILS
	static CFloat2	ProjectPointOnScreen(const CFloat3 &point, const CFloat4x4 &mvp, const CUint2 &screenSize);
	static CFloat3	UnprojectPointToWorld(const CFloat3 &pixelCoords, const CFloat4x4 &inverseMVP, const CUint2 &screenSize);

	float			_GetViewScale() const; // scale of the Gizmo, so it will always have the same size on the screen.
	float			_GetViewScalePre() const;
	void			_TransformReset();
	void			_TransformFromOutput(); // compute the m_Transform and other intern transforms from the outputs values
	void			_TransformComputeRotating();
	void			_ApplyMouseMove(const CInt2 &mouseScreenPosition);


	// State
	EGizmoType				m_Type;
	CFloat3					m_MouseWorldPositionPrev;
	float					m_SelfScale;
	SGizmoGeometryDesc		m_GeomDesc;
	bool					m_Clicked;
	bool					m_FrameLocal;	// false to keep the gizmo in the global frame, true to have the gizmo follow the local frame
											// in fact, other frames could be definied (euler-frame, global-after-parent, ...)
	u32						m_AllowedModesMask;

	// Transforms
	CFloat4x4				m_ParentTransform;			// transform that defines the "world-space"
	CFloat4x4				m_ParentPureRotation;		// the rotationnal-part of m_ParentTransform
	CFloat4x4				m_TransformPrev;			// save of "m_Transform" while the Gizmo is grabbed (in world-space)
	CFloat4x4				m_Transform;				// current-transform of the Gizmo (in world-space)
	CFloat4x4				m_DeltaTransform;			// Transform that converts "m_TransformPrev" to "m_Transform" (in world-space)
	CFloat4x4				m_TransformRotating[4];		// specific transforms for the rotation handles (x,y,z axis + screen-space)

	// Output
	struct	STargetObject
	{
		CFloat4x4								m_TransformPrev = CFloat4x4::IDENTITY;		// save of "m_Transform" while the Gizmo is grabbed
		CFloat4x4								m_Transform = CFloat4x4::IDENTITY;			// current-transform of the object (in the "local-space")
		CFloat4x4								*m_Object_Transform = null;
#ifdef PK_HAS_GIZMO_STD_FUNCTIONS
		fnObjectUpdate							m_UpdateFnct;
#endif
		u32										m_AllowedGizmoModes = 0;

		STargetObject() {}
		STargetObject(CFloat4x4 *tr, u32 allowedGizmoModes) : m_Transform(*tr), m_Object_Transform(tr), m_AllowedGizmoModes(allowedGizmoModes) {}
#ifdef PK_HAS_GIZMO_STD_FUNCTIONS
		STargetObject(const CFloat4x4 &tr, const fnObjectUpdate &fn, u32 allowedGizmoModes) : m_Transform(tr), m_UpdateFnct(fn), m_AllowedGizmoModes(allowedGizmoModes) {}
#endif
	};
	TStaticCountedArray<STargetObject, 32>		m_TargetObjects; // Used a "Static-Counted-Array" to prevent crash with MSVC build, when the array is being relocated on growth.

	bool					m_TransformChanged;		// true when transform values have been modified

	// Events
	CUint4					m_GrabbedAxis; // x,y,z and screen-space
	CUint4					m_HoveredAxis; // x,y,z and screen-space

	void		_CheckSelection(CUint4 &outSelection, const CInt2 &mouseScreenPosition, CFloat3 *outPosition = null);
	void		_CheckSelectionRoot(const CRay &ray, CUint4 &outSelectionAxis, CFloat3 *outPosition = null);
	void		_CheckSelectionQuads(const CRay &ray, CUint4 &outSelectionAxis, CFloat3 *outPosition = null);
	void		_CheckSelectionAxis(const CRay &ray, CUint4 &outSelectionAxis, CFloat3 *outPosition = null);
	void		_CheckSelectionSphere(const CRay &ray, CUint4 &outSelectionAxis, CFloat3 *outPosition = null);

	// Save viewport and camera info
	CFloat2					m_ContextSize;
	CFloat3					m_CameraPosition;
	CFloat4x4				m_ViewTransposed;
	CFloat4x4				m_ViewProj;
	CFloat4x4				m_InvViewProj;

	friend class CGizmoDrawer;
};

//----------------------------------------------------------------------------

#define GIZMODRAWER_USE_IMGUI 1

class	CGizmoDrawer
{
public:
	CGizmoDrawer(const SGizmoGeometryDesc &geomDesc = SGizmoGeometryDesc())
	:	m_Solid(null)
	,	m_Additive(null)
	,	m_GizmoVtx(null)
	,	m_GizmoIdx(null)
	,	m_GeomDesc(geomDesc)
	{
	}

	~CGizmoDrawer()
	{
	}

	bool		CreateRenderStates(const RHI::PApiManager &apiManager,
									CShaderLoader &loader,
									const TMemoryView<const RHI::SRenderTargetDesc> &frameBufferLayout,
									const RHI::PRenderPass &bakedRenderPass,
									u32 subPassIdx);

	bool		CreateVertexBuffer(	const RHI::PApiManager &apiManager);

	bool		DrawGizmo(const RHI::PCommandBuffer &cmdBuff, const CGizmo &gizmo) const;

private:
	RHI::PRenderState	m_Solid;
	RHI::PRenderState	m_Additive;
	RHI::PGpuBuffer		m_GizmoVtx;
	RHI::PGpuBuffer		m_GizmoIdx;

	SGizmoGeometryDesc	m_GeomDesc;

	struct SDrawIndexedRange
	{
		u32	offset;
		u32	count;
	};
	SDrawIndexedRange	m_DrawRoot;
	SDrawIndexedRange	m_DrawSphere;
	SDrawIndexedRange	m_DrawAxisTX; // DrawAxeT{X,Y,Z} such as DrawAxeTY = {axeX.offset + axeX.count, axeX.count} and DrawAxeTZ = {axeX.offset + 2 * axeX.count, axeX.count}
	SDrawIndexedRange	m_DrawAxisSX; // DrawAxeS{X,Y,Z}
	SDrawIndexedRange	m_DrawSquareX; // m_DrawSquare{X,Y,Z}
	SDrawIndexedRange	m_DrawRotationX; // m_DrawRotation{X,Y,Z}
	SDrawIndexedRange	m_DrawRotationSP; // screen-space handle
};

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
