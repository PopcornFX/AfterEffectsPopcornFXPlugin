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

#include "Gizmo.h"
#include "SampleUtils.h"
#include "Camera.h"
#include "ShaderLoader.h"
#include "ShaderDefinitions/SampleLibShaderDefinitions.h"

#include <pk_rhi/include/AllInterfaces.h>

#include <pk_kernel/include/kr_units.h>

#include <pk_maths/include/pk_maths_matrix.h>
#include <pk_maths/include/pk_maths_transforms.h>

#include <pk_discretizers/include/dc_discretizers.h>

#include <pk_geometrics/include/ge_mesh_vertex_declarations.h>
#include <pk_geometrics/include/ge_colliders.h>
#include <pk_geometrics/include/ge_coordinate_frame.h>

#include <pk_base_object/include/hbo_helpers.h>

#if GIZMODRAWER_USE_IMGUI == 1
#include <imgui.h>
#endif

#define	GIZMO_VERTEX_SHADER_PATH		"./Shaders/Gizmo.vert"
#define	GIZMO_FRAGMENT_SHADER_PATH		"./Shaders/Gizmo.frag"

#define GIZMO_AXIS_FLIP					0	// 1 : The Gizmo axis are always looking-front to the camera.

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

CGizmo::CGizmo(const SGizmoGeometryDesc &geomDesc)
:	m_Type(GizmoNone)
,	m_SelfScale(0.13f)
,	m_GeomDesc(geomDesc)
,	m_Clicked(false)
,	m_FrameLocal(false)
,	m_AllowedModesMask(GizmoNone | GizmoShowAxis | GizmoShowAxisWithoutNames)
,	m_ParentTransform(CFloat4x4::IDENTITY)
,	m_ParentPureRotation(CFloat4x4::IDENTITY)
,	m_Transform(CFloat4x4::IDENTITY)
,	m_DeltaTransform(CFloat4x4::IDENTITY)
,	m_TransformChanged(false)
,	m_GrabbedAxis(0)
,	m_HoveredAxis(0)
{
}

//----------------------------------------------------------------------------

void	CGizmo::SetParentTransform(const CFloat4x4 &transform)
{
	if (!IsGrabbed()) // if the user is manipulating the Gizmo, then we ignore concurrent update.
	{
		m_ParentTransform = transform;

		const CFloat3	scalingFactors = m_ParentTransform.ScalingFactors();
		m_ParentPureRotation.XAxis() = m_ParentTransform.XAxis() / scalingFactors.x();
		m_ParentPureRotation.YAxis() = m_ParentTransform.YAxis() / scalingFactors.y();
		m_ParentPureRotation.ZAxis() = m_ParentTransform.ZAxis() / scalingFactors.z();

		_TransformFromOutput();
	}
}

//----------------------------------------------------------------------------

void	CGizmo::ClearOutputToUpdate()
{
	if (!IsGrabbed())
	{
		m_AllowedModesMask = GizmoNone;
		m_TargetObjects.Clear();
		m_TransformChanged = false;
	}
}

//----------------------------------------------------------------------------

bool	CGizmo::PushBackObject(CFloat4x4 *transform, u32 allowedModesMask /*= GizmoTranslating | GizmoRotating | GizmoScaling*/)
{
	if (IsGrabbed())
		return false; // when grabbed, you can't add objects (even if this object is already bound).

	{
		m_AllowedModesMask |= allowedModesMask;

		const CGuid		objectId = m_TargetObjects.PushBack(STargetObject(transform, allowedModesMask));
		_TransformFromOutput();
		return objectId.Valid();
	}
}

//----------------------------------------------------------------------------

#ifdef PK_HAS_GIZMO_STD_FUNCTIONS

bool	CGizmo::PushBackObject(const CFloat4x4 &transform, const fnObjectUpdate &updateFromTransform,  u32 allowedModesMask /*= GizmoTranslating | GizmoRotating | GizmoScaling*/)
{
	if (IsGrabbed())
		return false; // when grabbed, you can't add objects (even if this object is already bound).

	{
		m_AllowedModesMask |= allowedModesMask;

		const CGuid		objectId = m_TargetObjects.PushBack(STargetObject(transform, updateFromTransform, allowedModesMask));
		_TransformFromOutput();
		return objectId.Valid();
	}
}

//----------------------------------------------------------------------------

bool	CGizmo::PushBackObject(CBaseObject *object_position, u32 fieldID_position, CBaseObject *object_eulerOrientation, u32 fieldID_eulerOrientation)
{
	if (IsGrabbed())
		return false; // when grabbed, you can't add objects (even if this object is already bound).

	CFloat4x4	transform = CFloat4x4::IDENTITY;
	u32			allowedModes = 0;
	bool		isPositionF4 = false;
	bool		isRotationF4 = false;

	if (object_position != null)
	{
		isPositionF4 = object_position->GetFieldDefinition(fieldID_position).Type() == HBO::SGenericType(HBO::GenericType_Float4, false);
		PK_ASSERT(isPositionF4 || object_position->GetFieldDefinition(fieldID_position).Type() == HBO::SGenericType(HBO::GenericType_Float3, false));

		const CFloat3	posXYZ = (isPositionF4) ? object_position->GetField<CFloat4>(fieldID_position).xyz() : object_position->GetField<CFloat3>(fieldID_position);
		const CFloat3	posSUF = CCoordinateFrame::TransposeFrame(ECoordinateFrame::Frame_RightHand_Y_Up, CCoordinateFrame::GlobalFrame(), posXYZ);
		transform.StrippedTranslations() = posSUF;
		allowedModes |= GizmoTranslating;
	}

	if (object_eulerOrientation != null)
	{
		isRotationF4 = object_eulerOrientation->GetFieldDefinition(fieldID_eulerOrientation).Type() == HBO::SGenericType(HBO::GenericType_Float4, false);
		PK_ASSERT(isRotationF4 || object_eulerOrientation->GetFieldDefinition(fieldID_eulerOrientation).Type() == HBO::SGenericType(HBO::GenericType_Float3, false));

		const CFloat3		rotXYZ = (isRotationF4) ? object_eulerOrientation->GetField<CFloat4>(fieldID_eulerOrientation).xyz() : object_eulerOrientation->GetField<CFloat3>(fieldID_eulerOrientation);
		const CQuaternion	orientationXYZ = Transforms::Quaternion::FromEuler(Units::DegreesToRadians(rotXYZ));
		const CQuaternion	orientationSUF = CCoordinateFrame::TransposeFrame(ECoordinateFrame::Frame_RightHand_Y_Up, CCoordinateFrame::GlobalFrame(), orientationXYZ);
		transform = Transforms::Matrix::FromQuaternion(orientationSUF, transform.WAxis());
		allowedModes |= GizmoRotating;
	}

	fnObjectUpdate	updateFn = [object_position, fieldID_position, object_eulerOrientation, fieldID_eulerOrientation, isPositionF4, isRotationF4](u32 currentMode, const CFloat4x4 &xform)
	{
		if (object_position != null && currentMode == GizmoTranslating)
		{
			const CFloat3	posXYZ = CCoordinateFrame::TransposeFrame(CCoordinateFrame::GlobalFrame(), ECoordinateFrame::Frame_RightHand_Y_Up, xform.StrippedTranslations());
			if (isPositionF4)
				object_position->SetField<CFloat4>(fieldID_position, posXYZ.xyz0());
			else
				object_position->SetField<CFloat3>(fieldID_position, posXYZ);
		}

		if (object_eulerOrientation != null && currentMode == GizmoRotating)
		{
			const CQuaternion	orientationSUF = Transforms::Quaternion::FromMatrix(xform);
			const CQuaternion	orientationXYZ = CCoordinateFrame::TransposeFrame(CCoordinateFrame::GlobalFrame(), ECoordinateFrame::Frame_RightHand_Y_Up, orientationSUF);
			const CFloat3		eulerAnglesXYZ = Units::RadiansToDegrees(Transforms::Euler::FromQuaternion(orientationXYZ));
			if (isRotationF4)
				object_eulerOrientation->SetField<CFloat4>(fieldID_eulerOrientation, eulerAnglesXYZ.xyz0());
			else
				object_eulerOrientation->SetField<CFloat3>(fieldID_eulerOrientation, eulerAnglesXYZ);
		}
	};

	return PushBackObject(transform, updateFn, allowedModes);
}

#endif // PK_HAS_GIZMO_STD_FUNCTIONS

//----------------------------------------------------------------------------

void	CGizmo::ApplyGizmoTransform()
{
	m_TransformChanged = false;	// Clear dirty flag (fix #6002)

	if (m_Type == GizmoTranslating)
	{
		const CFloat3	outTranslationWorld = m_DeltaTransform.StrippedTranslations();
		const CFloat3	outTranslationLocal = m_ParentTransform.Crop<3, 3>().Inverse().RotateVector(outTranslationWorld); // TODO better

		for (auto &object : m_TargetObjects)
		{
			if ((object.m_AllowedGizmoModes & GizmoTranslating) != 0)
			{
				object.m_Transform.StrippedTranslations() = object.m_TransformPrev.StrippedTranslations() + outTranslationLocal;
			}
		}
	}
	else if (m_Type == GizmoScaling)
	{
		const CFloat4x4	&scaleMatrix = m_DeltaTransform;

		if (m_FrameLocal)
		{
			for (auto &object : m_TargetObjects)
			{
				if ((object.m_AllowedGizmoModes & GizmoScaling) != 0)
				{
					object.m_Transform = scaleMatrix * object.m_TransformPrev;
				}
			}
		}
		else
		{
			// this case will make the local-transform matrix non-orthogonal anymore, but we must avoid this !!
			// So the resulting transform will not match exactly with the gizmo transform.

			for (auto &object : m_TargetObjects)
			{
				if ((object.m_AllowedGizmoModes & GizmoScaling) != 0)
				{
					const CFloat4x4	newTransform = object.m_TransformPrev * m_ParentPureRotation * scaleMatrix * m_ParentPureRotation.Transposed();
					const CFloat3	scaleFactorOld = object.m_TransformPrev.AbsScalingFactors();
					CFloat3			scaleFactorNew = newTransform.AbsScalingFactors();

					if (object.m_TransformPrev.XAxis().Dot(newTransform.XAxis()) < 0.f)
						scaleFactorNew.x() *= -1.f;
					if (object.m_TransformPrev.YAxis().Dot(newTransform.YAxis()) < 0.f)
						scaleFactorNew.y() *= -1.f;
					if (object.m_TransformPrev.ZAxis().Dot(newTransform.ZAxis()) < 0.f)
						scaleFactorNew.z() *= -1.f;

					CFloat4x4	localScaleMatrix;
					localScaleMatrix.XAxis() = CFloat4::XAXIS * (scaleFactorNew.x() / scaleFactorOld.x());
					localScaleMatrix.YAxis() = CFloat4::YAXIS * (scaleFactorNew.y() / scaleFactorOld.y());
					localScaleMatrix.ZAxis() = CFloat4::ZAXIS * (scaleFactorNew.z() / scaleFactorOld.z());
					localScaleMatrix.WAxis() = CFloat4::WAXIS;

					const CFloat3	center = object.m_Transform.StrippedTranslations();
					object.m_Transform = localScaleMatrix * object.m_TransformPrev;
					object.m_Transform.StrippedTranslations() = center;
				}
			}
		}
	}
	else if (m_Type == GizmoRotating)
	{
		const CFloat4x4		&rotateMatrix = m_DeltaTransform; // Already in local-space.

		for (auto &object : m_TargetObjects)
		{
			if ((object.m_AllowedGizmoModes & GizmoRotating) != 0)
			{
				object.m_Transform = object.m_TransformPrev * rotateMatrix;
				object.m_Transform.StrippedTranslations() = object.m_TransformPrev.StrippedTranslations();
			}
		}
	}
	else
	{
		PK_ASSERT_NOT_REACHED();
	}

	// Feed back to the bound objects: update their out-transform (or call the update-function).
	for (auto &object : m_TargetObjects)
	{
		if ((object.m_AllowedGizmoModes & m_Type) != 0)
		{
			if (object.m_Object_Transform != null)
				*object.m_Object_Transform = object.m_Transform;

#ifdef PK_HAS_GIZMO_STD_FUNCTIONS
			if (object.m_UpdateFnct)
				object.m_UpdateFnct(m_Type, object.m_Transform);
#endif
		}
	}

}

//----------------------------------------------------------------------------

void	CGizmo::UpdateCameraInfo(const CCameraBase &camera)
{
	const CFloat4x4	viewProj = camera.View() * camera.Proj();

	m_ContextSize = camera.WindowSize();
	m_ViewTransposed = camera.View().Transposed();
	m_ViewProj = viewProj;
	m_CameraPosition = camera.Position();
	m_InvViewProj = viewProj.Inverse();

	if (!IsGrabbed())
		_TransformReset();
}

//----------------------------------------------------------------------------

void	CGizmo::SetMouseClicked(const CInt2 &mousePosition, bool clicked)
{
	if (clicked)
	{
		// check if something has been selected
		_CheckSelection(m_GrabbedAxis, mousePosition, &m_MouseWorldPositionPrev);
		m_Clicked = m_GrabbedAxis != CUint4::ZERO;
		m_HoveredAxis = CUint4::ZERO;
		if (m_Clicked) // set the "prev"-transforms
		{
			m_TransformPrev = m_Transform;
			for (auto &object : m_TargetObjects)
				object.m_TransformPrev = object.m_Transform;
		}
	}
	else
	{
		m_GrabbedAxis = CUint4::ZERO;
		m_Clicked = false;
		_TransformReset();
	}
}

//----------------------------------------------------------------------------

void	CGizmo::SetMouseMoved(const CInt2 &mousePosition)
{
	const bool	isGrabbed = m_GrabbedAxis != CUint4::ZERO;

	if (m_Clicked && isGrabbed)
	{
		// mouse-drag
		_ApplyMouseMove(mousePosition);
	}

	if (!m_Clicked && !isGrabbed)
	{
		// check mouse hover
		_CheckSelection(m_HoveredAxis, mousePosition);
	}
}

//----------------------------------------------------------------------------

CFloat2	CGizmo::ProjectPointOnScreen(const CFloat3 &point, const CFloat4x4 &mvp, const CUint2 &screenSize)
{
	CFloat4	projected = mvp.TransformVector(CFloat4(point, 1));

	projected /= projected.w();
	projected.xy() = projected.xy() * 0.5f + 0.5f;
	projected.xy() *= screenSize;
	return projected.xy();
}

//----------------------------------------------------------------------------

CFloat3	CGizmo::UnprojectPointToWorld(const CFloat3 &pixelCoords, const CFloat4x4 &inverseMVP, const CUint2 &screenSize)
{
	CFloat4	unproj = CFloat4(pixelCoords.xy() / CFloat2(screenSize), pixelCoords.z(), 1);

	unproj.xy() = unproj.xy() * 2.0f - 1.0f;
	unproj = inverseMVP.TransformVector(unproj);
	return unproj.xyz() / unproj.w();
}

//----------------------------------------------------------------------------

float	CGizmo::_GetViewScale() const
{
	const CFloat3	viewFoward = m_ViewTransposed.Axis(CCoordinateFrame::AxisIndexer(Axis_Forward)).xyz();
	const CFloat3	camToPos = m_Transform.StrippedTranslations() - m_CameraPosition;
	return PKAbs(m_SelfScale * viewFoward.Dot(camToPos));
}

//----------------------------------------------------------------------------

float	CGizmo::_GetViewScalePre() const
{
	const CFloat3	viewFoward = m_ViewTransposed.Axis(CCoordinateFrame::AxisIndexer(Axis_Forward)).xyz();
	const CFloat3	camToPos = m_TransformPrev.StrippedTranslations() - m_CameraPosition;
	return PKAbs(m_SelfScale * viewFoward.Dot(camToPos));
}

//----------------------------------------------------------------------------

void	CGizmo::_TransformReset()
{
	if (m_FrameLocal)
	{
		const float	sX = m_Transform.StrippedXAxis().Length();
		const float	invSX = sX < 1.e-12f ? 1.e12f : 1.f / sX;
		m_Transform.XAxis() *= invSX;

		const float	sY = m_Transform.StrippedYAxis().Length();
		const float	invSY = sY < 1.e-12f ? 1.e12f : 1.f / sY;
		m_Transform.YAxis() *= invSY;

		const float	sZ = m_Transform.StrippedZAxis().Length();
		const float	invSZ = sZ < 1.e-12f ? 1.e12f : 1.f / sZ;
		m_Transform.ZAxis() *= invSZ;
	}
	else
	{
		m_Transform.XAxis() = CFloat4::XAXIS;
		m_Transform.YAxis() = CFloat4::YAXIS;
		m_Transform.ZAxis() = CFloat4::ZAXIS;
	}

#if GIZMO_AXIS_FLIP != 0
	// also compute the sign to have the Gizmo facing the camera ...

	const CFloat3	camToPos = m_ParentTransform.TransformVector(m_Transform.StrippedTranslations()) - m_CameraPosition;

	if (camToPos.Dot(m_Transform.StrippedXAxis()) > 0.f)
		m_Transform.XAxis() = -m_Transform.XAxis();

	if (camToPos.Dot(m_Transform.StrippedYAxis()) > 0.f)
		m_Transform.YAxis() = -m_Transform.YAxis();

	if (camToPos.Dot(m_Transform.StrippedZAxis()) > 0.f)
		m_Transform.ZAxis() = -m_Transform.ZAxis();
#endif

	// Build View-space transform for rotation handles (eq. half-torus drawing)
	if (m_Type == GizmoRotating)
	{
		_TransformComputeRotating();
	}

	m_DeltaTransform = CFloat4x4::IDENTITY;
}

//----------------------------------------------------------------------------

void	CGizmo::_TransformFromOutput()
{
	if (IsGrabbed())
	{
		// if grabbed, cancel the move
		m_Transform = m_TransformPrev;
	}
	else
	{
		// merge value from all objects' transform
		CFloat4x4	transformObjects = CFloat4x4::IDENTITY;

		{
			u32		canRotateCount = 0;
			for (const auto &object : m_TargetObjects)
			{
				transformObjects = object.m_Transform;
				if ((object.m_AllowedGizmoModes & GizmoRotating) != 0)
					++canRotateCount;
			}
			transformObjects.WAxis() = CFloat4::WAXIS;
			if (canRotateCount > 1)
				transformObjects = CFloat4x4::IDENTITY;
		}

		{
			u32		canTranslateCount = 0;
			CFloat3	posMid = CFloat3::ZERO;
			for (const auto &object : m_TargetObjects)
			{
				posMid += object.m_Transform.StrippedWAxis();
				if ((m_AllowedModesMask & GizmoTranslating) != 0)
					++canTranslateCount;
			}
			if (canTranslateCount != 0)
				transformObjects.StrippedWAxis() = posMid / canTranslateCount;
		}

		m_Transform = m_ParentTransform * transformObjects;
	}

	_TransformReset();
}

//----------------------------------------------------------------------------

void	CGizmo::_TransformComputeRotating()
{
	PK_ASSERT(m_Transform.Orthonormal());

	m_TransformRotating[0] = m_TransformRotating[1] = m_TransformRotating[2] = m_Transform;

	PK_ASSERT(m_Type == GizmoRotating);
	const CFloat3	viewDepth = (m_CameraPosition - m_Transform.StrippedTranslations()).Normalized();

	// m_TransformRotating[0] => axisX -> transformX, axisY -> viewDepth
	const CFloat3	new0_Y = viewDepth - viewDepth.Dot(m_TransformRotating[0].StrippedXAxis()) * m_TransformRotating[0].StrippedXAxis();
	if (!new0_Y.IsZero())
	{
		m_TransformRotating[0].StrippedYAxis() = new0_Y.Normalized();
		m_TransformRotating[0].StrippedZAxis() = m_TransformRotating[0].StrippedXAxis().Cross(m_TransformRotating[0].StrippedYAxis());
	}

	// m_TransformRotating[1] => axisY -> transformY, axisZ -> viewDepth
	const CFloat3	new1_Z = viewDepth - viewDepth.Dot(m_TransformRotating[1].StrippedYAxis()) * m_TransformRotating[1].StrippedYAxis();
	if (!new1_Z.IsZero())
	{
		m_TransformRotating[1].StrippedZAxis() = new1_Z.Normalized();
		m_TransformRotating[1].StrippedXAxis() = m_TransformRotating[1].StrippedYAxis().Cross(m_TransformRotating[1].StrippedZAxis());
	}

	// m_TransformRotating[2] => axisZ -> transformZ, axisX -> viewDepth
	const CFloat3	new2_X = viewDepth - viewDepth.Dot(m_TransformRotating[2].StrippedZAxis()) * m_TransformRotating[2].StrippedZAxis();
	if (!new2_X.IsZero())
	{
		m_TransformRotating[2].StrippedXAxis() = new2_X.Normalized();
		m_TransformRotating[2].StrippedYAxis() = m_TransformRotating[2].StrippedZAxis().Cross(m_TransformRotating[2].StrippedXAxis());
	}

	// m_TransformRotating[3] => axisZ -> viewDepth
	const CUint3	axesIndexer = CCoordinateFrame::AxesIndexer();
	const CFloat3	trY = viewDepth.Cross(m_ViewTransposed.Axis(axesIndexer.x()).xyz());
	if (!trY.IsZero())
	{
		m_TransformRotating[3].XAxis() = trY.Cross(viewDepth).Normalized().xyz0();
		m_TransformRotating[3].YAxis() = trY.Normalized().xyz0();
		m_TransformRotating[3].ZAxis() = viewDepth.xyz0();
	}
	else
	{
		m_TransformRotating[3].XAxis() = m_ViewTransposed.Axis(axesIndexer.x()).xyz0();
		m_TransformRotating[3].YAxis() = m_ViewTransposed.Axis(axesIndexer.y()).xyz0();
		m_TransformRotating[3].ZAxis() = m_ViewTransposed.Axis(axesIndexer.z()).xyz0();
	}
	m_TransformRotating[3].Translations() = m_Transform.Translations();
}

//----------------------------------------------------------------------------

void	CGizmo::_ApplyMouseMove(const CInt2 &mouseScreenPosition)
{
	if ((m_Type & m_AllowedModesMask) == 0)
		return;

	const CFloat4x4	frame = m_Transform;

	PK_ASSERT(m_Clicked && m_GrabbedAxis != CUint4::ZERO);

	CFloat3		viewDepth;
	if (m_Type == GizmoRotating)
	{
		viewDepth = (m_CameraPosition - m_Transform.StrippedTranslations()).Normalized();
	}
	else
	{
		viewDepth = m_ViewTransposed.Axis(CCoordinateFrame::AxisIndexer(Axis_Backward)).xyz();
		if (CCoordinateFrame::AxisRemapper(Axis_Backward) < 0)
			viewDepth = -viewDepth;
	}

	// Construct the "mouseWorldPos" depending on projection
	CFloat3		mouseWorldPos = CFloat3::ZERO;

	{
		const CFloat3	mouseWorldPosFar = UnprojectPointToWorld(CFloat3(mouseScreenPosition, 1.f), m_InvViewProj, m_ContextSize);
		const CFloat3	mouseDirection = mouseWorldPosFar - m_CameraPosition;

		const CFloat3	cameraToPos = frame.StrippedTranslations() - m_CameraPosition;

		const bool	projAxisX = m_Type != GizmoRotating ? m_GrabbedAxis.Axis(0) != 0 : m_GrabbedAxis.Axis(0) == 0;
		const bool	projAxisY = m_Type != GizmoRotating ? m_GrabbedAxis.Axis(1) != 0 : m_GrabbedAxis.Axis(1) == 0;
		const bool	projAxisZ = m_Type != GizmoRotating ? m_GrabbedAxis.Axis(2) != 0 : m_GrabbedAxis.Axis(2) == 0;
		const bool	projSphere = m_Type == GizmoRotating && m_GrabbedAxis.xyz() == CUint3::ONE;
		const bool	projScreenP = (m_Type != GizmoRotating && m_GrabbedAxis.xyz() == CUint3::ONE) || m_GrabbedAxis.w() != 0;

		if (projSphere)
		{
			// project on sphere
			const float		radius = _GetViewScale() * (m_GeomDesc.m_AxisLength + m_GeomDesc.m_ArrowHeight);

			const CFloat3	ray = mouseDirection.Normalized();
			const float		tRayCenter = cameraToPos.Dot(ray);
			const float		tOffsetSquared = tRayCenter * tRayCenter - cameraToPos.LengthSquared() + radius * radius;

			if (tOffsetSquared < 0.f) // the ray does not hit the sphere
			{
				const CFloat3	PosToSphere = radius * ((tRayCenter * ray - cameraToPos).Normalized());
				mouseWorldPos = frame.StrippedTranslations() + PosToSphere;
			}
			else // the ray hits the sphere
			{
				const float		tOffset = PKSqrt(tOffsetSquared);
				const float		tRayClosestPoint = tRayCenter - tOffset;
				mouseWorldPos = m_CameraPosition + tRayClosestPoint * ray;
			}
		}
		else if (projScreenP)
		{
			// project on screen-plane with direction = mouseDirection
			const CFloat3	planeNormal = viewDepth;
			mouseWorldPos = m_CameraPosition + mouseDirection * (planeNormal.Dot(cameraToPos) / planeNormal.Dot(mouseDirection));
		}
		else if (projAxisX & projAxisY)
		{
			// project on XY-plane with direction = mouseDirection
			const CFloat3	planeNormal = frame.Axis(2).xyz();
			mouseWorldPos = m_CameraPosition + mouseDirection * (planeNormal.Dot(cameraToPos) / planeNormal.Dot(mouseDirection));
		}
		else if (projAxisX & projAxisZ)
		{
			// project on XZ-plane with direction = mouseDirection
			const CFloat3	planeNormal = frame.Axis(1).xyz();
			mouseWorldPos = m_CameraPosition + mouseDirection * (planeNormal.Dot(cameraToPos) / planeNormal.Dot(mouseDirection));
		}
		else if (projAxisY & projAxisZ)
		{
			// project on YZ-plane with direction = mouseDirection
			const CFloat3	planeNormal = frame.Axis(0).xyz();
			mouseWorldPos = m_CameraPosition + mouseDirection * (planeNormal.Dot(cameraToPos) / planeNormal.Dot(mouseDirection));
		}
		else if (projAxisX | projAxisY | projAxisZ)
		{
			const u32		axisIdx = projAxisZ ? 2 : (projAxisY ? 1 : 0);
			const CFloat3	axis = frame.Axis(axisIdx).xyz();
#if 1 // pre-2D-Projection (not perfect but better than nothing)
			CFloat3			newMouseDirection = mouseDirection;
			const CFloat2	gizmoScreenPosition = ProjectPointOnScreen(m_TransformPrev.StrippedTranslations(), m_ViewProj, m_ContextSize);
			const CFloat2	gizmoScreenPositionDx = ProjectPointOnScreen(m_TransformPrev.StrippedTranslations() + axis * _GetViewScale(), m_ViewProj, m_ContextSize);
			CFloat2			axis2D = gizmoScreenPositionDx - gizmoScreenPosition;
			if (!axis2D.IsZero())
			{
				axis2D.Normalize();
				const CFloat2	mouseScreenProjected = gizmoScreenPosition + axis2D.Dot(mouseScreenPosition - gizmoScreenPosition) * axis2D;
				// mouseScreenProjected : we can do even better, with a 2D-projection using the on-screen-viewSide direction along the (gizmoScreenPosition, axis2D).
				// get a new 3D-ray
				const CFloat3	mouseWorldProjected = UnprojectPointToWorld(mouseScreenProjected.xy1(), m_InvViewProj, m_ContextSize);
				newMouseDirection = mouseWorldProjected - m_CameraPosition;
			}
#else
			const CFloat3	newMouseDirection = mouseDirection;
#endif
			// project on X-axis with direction = mouseDirection
			const CFloat3	cross = axis.Cross(newMouseDirection);
			// -> project on (rayDir,cross)-plane with direction = mouseDirection
			const CFloat3	planeNormal1 = cross.Cross(axis);
			const CFloat3	posToProj = newMouseDirection * (planeNormal1.Dot(cameraToPos) / planeNormal1.Dot(newMouseDirection)) - cameraToPos;
			// -> distance along the axis
			const float		tAxis = posToProj.Dot(axis) / axis.LengthSquared();
			mouseWorldPos = frame.StrippedTranslations() + tAxis * axis;
		}
		else
		{
			PK_ASSERT_NOT_REACHED();
		}
	}

	// Now, apply the new transform on the Gizmo

	if (m_Type == GizmoTranslating)
	{
		if (viewDepth.Dot(mouseWorldPos - m_CameraPosition) > 0.f)
		{
			// cancel translating
			_TransformFromOutput();
			m_TransformChanged = true;
			return;
		}

		const CFloat3	outTranslationWorld = mouseWorldPos - m_MouseWorldPositionPrev;
		m_DeltaTransform.StrippedTranslations() = outTranslationWorld;
		m_Transform.StrippedTranslations() = m_TransformPrev.StrippedTranslations() + outTranslationWorld;

		m_TransformChanged = true;
	}
	else if (m_Type == GizmoScaling)
	{
		if (viewDepth.Dot(mouseWorldPos - m_CameraPosition) > 0.f)
		{
			// cancel scaling
			_TransformFromOutput();
			m_TransformChanged = true;
			return;
		}

		const CFloat3	position = frame.StrippedTranslations();

		if (m_GrabbedAxis.xyz() == CUint3::ONE) // specific case : global scaling
		{
			const float		projParam = (mouseWorldPos - position).Dot(m_ViewTransposed.Axis(CCoordinateFrame::AxisIndexer(Axis_Left)).xyz());
			const float		scaleFactor = projParam + 1.f;

			m_Transform.XAxis() = scaleFactor * m_TransformPrev.XAxis();
			m_Transform.YAxis() = scaleFactor * m_TransformPrev.YAxis();
			m_Transform.ZAxis() = scaleFactor * m_TransformPrev.ZAxis();

			m_DeltaTransform.XAxis() = scaleFactor * CFloat4::XAXIS;
			m_DeltaTransform.YAxis() = scaleFactor * CFloat4::YAXIS;
			m_DeltaTransform.ZAxis() = scaleFactor * CFloat4::ZAXIS;

			m_TransformChanged = true;
		}
		else
		{
			const CFloat3	vectorPrev = m_MouseWorldPositionPrev - position;
			const float		scaleNewOld = vectorPrev.Dot(mouseWorldPos - position);
			const float		scaleSquaredPrev = vectorPrev.LengthSquared();
			const float		scaleFactor = scaleSquaredPrev < 1.e-6f ? 0.f : scaleNewOld / scaleSquaredPrev; // = scaleNew * scaleOld / (scaleOld * scaleOld) = scaleNew / scaleOld

			{
				CFloat4x4	scaleMatrix = CFloat4x4::IDENTITY;
				if (m_GrabbedAxis.Axis(0) != 0)
					scaleMatrix.Axis(0) *= scaleFactor;
				if (m_GrabbedAxis.Axis(1) != 0)
					scaleMatrix.Axis(1) *= scaleFactor;
				if (m_GrabbedAxis.Axis(2) != 0)
					scaleMatrix.Axis(2) *= scaleFactor;

				m_Transform = scaleMatrix * m_TransformPrev;

				m_DeltaTransform = scaleMatrix;

				m_TransformChanged = true;
			}
		}
	}
	else if (m_Type == GizmoRotating)
	{
		const CFloat3	position = frame.StrippedTranslations();

		const CFloat3	vectOld = (m_MouseWorldPositionPrev - position).Normalized();
		const CFloat3	vectNew = (mouseWorldPos - position).Normalized();

		CFloat4x4		rotateMatrix = CFloat4x4::IDENTITY;

		if (m_GrabbedAxis.xyz() == CUint3::ONE) // specific case : global rotation
		{
			const CFloat3	rotationVector = vectOld.Cross(vectNew);
			const float		sinTheta = rotationVector.Length();

			if (sinTheta > 1.e-6f)
			{
				const CFloat3	rv = rotationVector.Normalized();
				const float		cosTheta = vectOld.Dot(vectNew);
				const float		OneMcosTheta = 1.f - cosTheta;

				rotateMatrix.StrippedXAxis() = CFloat3(cosTheta + OneMcosTheta * rv.x() * rv.x(), sinTheta * rv.z() + OneMcosTheta * rv.x() * rv.y(), -sinTheta * rv.y() + OneMcosTheta * rv.x() * rv.z());
				rotateMatrix.StrippedYAxis() = CFloat3(-sinTheta * rv.z() + OneMcosTheta * rv.y() * rv.x(), cosTheta + OneMcosTheta * rv.y() * rv.y(), sinTheta * rv.x() + OneMcosTheta * rv.y() * rv.z());
				rotateMatrix.StrippedZAxis() = CFloat3(sinTheta * rv.y() + OneMcosTheta * rv.z() * rv.x(), -sinTheta * rv.x() + OneMcosTheta * rv.z() * rv.y(), cosTheta + OneMcosTheta * rv.z() * rv.z());
			}
		}
		else
		{
			CFloat3	rv;
			if (m_GrabbedAxis.Axis(0) != 0)
				rv = m_TransformPrev.StrippedXAxis();
			else if (m_GrabbedAxis.Axis(1) != 0)
				rv = m_TransformPrev.StrippedYAxis();
			else if (m_GrabbedAxis.Axis(2) != 0)
				rv = m_TransformPrev.StrippedZAxis();
			else // (m_GrabbedAxis.Axis(3) != 0)
				rv = viewDepth;

			PK_ASSERT(rv.IsNormalized());

#if 0 // Rotate by projection
			const float		sinTheta = vectOld.Cross(vectNew).Dot(rv);
			const float		cosTheta = vectOld.Dot(vectNew);
#else // Rotate by screen-space move
			const CFloat3	moveTangent3D = rv.Cross(vectOld);
			CFloat2			moveDirection2D = m_ViewProj.RotateVector(moveTangent3D).xy();
			if (moveDirection2D.IsZero())
				moveDirection2D = CFloat2(1.f, -1.f);
			moveDirection2D.Normalize();
			const CInt2		mouseScreenPositionPrev = ProjectPointOnScreen(m_MouseWorldPositionPrev, m_ViewProj, m_ContextSize);
			const CFloat2	deltaXY = mouseScreenPosition - mouseScreenPositionPrev;
			const float		theta = moveDirection2D.Dot(deltaXY) * 0.006f; // rotation-angle by pixels
			const float		sinTheta = sin(theta);
			const float		cosTheta = cos(theta);
#endif

			if (PKAbs(sinTheta) > 1.e-6f)
			{
				const float		OneMcosTheta = 1.f - cosTheta;

				rotateMatrix.StrippedXAxis() = CFloat3(cosTheta + OneMcosTheta * rv.x() * rv.x(), sinTheta * rv.z() + OneMcosTheta * rv.x() * rv.y(), -sinTheta * rv.y() + OneMcosTheta * rv.x() * rv.z());
				rotateMatrix.StrippedYAxis() = CFloat3(-sinTheta * rv.z() + OneMcosTheta * rv.y() * rv.x(), cosTheta + OneMcosTheta * rv.y() * rv.y(), sinTheta * rv.x() + OneMcosTheta * rv.y() * rv.z());
				rotateMatrix.StrippedZAxis() = CFloat3(sinTheta * rv.y() + OneMcosTheta * rv.z() * rv.x(), -sinTheta * rv.x() + OneMcosTheta * rv.z() * rv.y(), cosTheta + OneMcosTheta * rv.z() * rv.z());
			}
		}

		PK_ASSERT(m_TransformPrev.Orthonormal());

		m_DeltaTransform = rotateMatrix; // Warning: m_DeltaTransform will be in local-space in this case.

		rotateMatrix = m_ParentPureRotation * rotateMatrix * m_ParentPureRotation.Transposed();

		m_Transform = m_TransformPrev * rotateMatrix;
		m_Transform.StrippedTranslations() = m_TransformPrev.StrippedTranslations();

		_TransformComputeRotating();

		m_TransformChanged = true;
	}
	else
	{
		PK_ASSERT_NOT_REACHED();
	}
}

//----------------------------------------------------------------------------

void	CGizmo::_CheckSelection(CUint4 &outSelection, const CInt2 &mouseScreenPosition, CFloat3 *outPosition)
{
	outSelection = CUint4::ZERO;

	if ((m_Type & m_AllowedModesMask) == 0)
		return;

	const CFloat3	mouseWorldPosFar = UnprojectPointToWorld(CFloat3(mouseScreenPosition, 1.f), m_InvViewProj, m_ContextSize);
	const CFloat3	mouseDirection = (mouseWorldPosFar - m_CameraPosition).Normalized();
	const CRay		mouseToWorld = CRay(m_CameraPosition, mouseDirection);

	if (m_Type == GizmoRotating)
	{
		_CheckSelectionSphere(mouseToWorld, outSelection, outPosition);
	}
	else
	{
		_CheckSelectionQuads(mouseToWorld, outSelection, outPosition);

		if (outSelection == CUint4::ZERO)
			_CheckSelectionRoot(mouseToWorld, outSelection, outPosition);
		if (outSelection == CUint4::ZERO)
			_CheckSelectionAxis(mouseToWorld, outSelection, outPosition);
	}
}

//----------------------------------------------------------------------------

void	CGizmo::_CheckSelectionQuads(const CRay &ray, CUint4 &outSelectionAxis, CFloat3 *outPosition)
{
	const CFloat4x4	frame = m_Transform;

	const CFloat3	position = frame.StrippedTranslations();
	const float		scale = _GetViewScale();

	const float		squareSizeIn = scale * m_GeomDesc.m_SquareDist;
	const float		squareSizeOu = scale * m_GeomDesc.m_SquareSize;

	float	distMin = TNumericTraits<float>::kMax;
	s32		hintMin = -1;

	{
		Colliders::STraceResult	result;
		const CFloat3			vecU = frame.Axis(0).xyz();
		const CFloat3			vecV = frame.Axis(1).xyz();
		const CFloat3			pos0 = position + squareSizeIn * (vecU + vecV);
		const Colliders::STriangleTraceAccelerator	triangle1(pos0, pos0 + squareSizeOu * vecU, pos0 + squareSizeOu * (vecU + vecV));
		const Colliders::STriangleTraceAccelerator	triangle2(pos0 + squareSizeOu * (vecU + vecV), pos0 + squareSizeOu * vecV, pos0);
		if (Colliders::RayTraceTriangle(ray, triangle1, result) ||
			Colliders::RayTraceTriangle(ray, triangle2, result))
		{
			distMin = result.m_BasicReport.t;
			hintMin = 0;
		}
	}

	{
		Colliders::STraceResult	result;
		const CFloat3			vecU = frame.Axis(0).xyz();
		const CFloat3			vecV = frame.Axis(2).xyz();
		const CFloat3			pos0 = position + squareSizeIn * (vecU + vecV);
		const Colliders::STriangleTraceAccelerator	triangle1(pos0, pos0 + squareSizeOu * vecU, pos0 + squareSizeOu * (vecU + vecV));
		const Colliders::STriangleTraceAccelerator	triangle2(pos0 + squareSizeOu * (vecU + vecV), pos0 + squareSizeOu * vecV, pos0);
		if (Colliders::RayTraceTriangle(ray, triangle1, result) ||
			Colliders::RayTraceTriangle(ray, triangle2, result))
		{
			if (result.m_BasicReport.t < distMin)
			{
				distMin = result.m_BasicReport.t;
				hintMin = 1;
			}
		}
	}

	{
		Colliders::STraceResult	result;
		const CFloat3			vecU = frame.Axis(1).xyz();
		const CFloat3			vecV = frame.Axis(2).xyz();
		const CFloat3			pos0 = position + squareSizeIn * (vecU + vecV);
		const Colliders::STriangleTraceAccelerator	triangle1(pos0, pos0 + squareSizeOu * vecU, pos0 + squareSizeOu * (vecU + vecV));
		const Colliders::STriangleTraceAccelerator	triangle2(pos0 + squareSizeOu * (vecU + vecV), pos0 + squareSizeOu * vecV, pos0);
		if (Colliders::RayTraceTriangle(ray, triangle1, result) ||
			Colliders::RayTraceTriangle(ray, triangle2, result))
		{
			if (result.m_BasicReport.t < distMin)
			{
				distMin = result.m_BasicReport.t;
				hintMin = 2;
			}
		}
	}

	outSelectionAxis =	(hintMin == 0) * CUint4(1, 1, 0, 0) +
						(hintMin == 1) * CUint4(1, 0, 1, 0) +
						(hintMin == 2) * CUint4(0, 1, 1, 0);

	if (outPosition != null && hintMin >= 0)
		*outPosition = ray.Origin() + distMin * ray.Direction();
}

//----------------------------------------------------------------------------

void	CGizmo::_CheckSelectionRoot(const CRay &ray, CUint4 &outSelectionAxis, CFloat3 *outPosition)
{
	const CFloat4x4	frame = m_Transform;

	const CFloat3	position = frame.StrippedTranslations();
	const CFloat3	CamToPos = position - ray.Origin();

	const float		sphereSize = _GetViewScale() * m_GeomDesc.m_RootSize;

	const CFloat3	tang = CamToPos.Cross(ray.Direction());
	const float		distSquared = tang.LengthSquared() / ray.Direction().LengthSquared();

	//Note: we assume that ray.Origin() == m_CameraPosition

	if (distSquared <= sphereSize * sphereSize)
	{
		outSelectionAxis = CUint4(1, 1, 1, 0);

		if (outPosition != null)
		{
			// project on screen-plane with direction = ray.Direction()
			PK_ASSERT(m_Type != GizmoRotating);
			const CFloat3	planeNormal = m_ViewTransposed.Axis(CCoordinateFrame::AxisIndexer(Axis_Forward)).xyz();
			*outPosition = m_CameraPosition + ray.Direction() * (planeNormal.Dot(CamToPos) / planeNormal.Dot(ray.Direction()));
		}
	}
}

//----------------------------------------------------------------------------

void	CGizmo::_CheckSelectionAxis(const CRay &ray, CUint4 &outSelectionAxis, CFloat3 *outPosition)
{
	const CFloat4x4	frame = m_Transform;

	const CFloat3	position = frame.StrippedTranslations();
	const float		scale = _GetViewScale();
	const CFloat3	CamToPos = position - ray.Origin();

	const float		axisSize = scale * (m_GeomDesc.m_AxisLength + m_GeomDesc.m_ArrowHeight);
	const float		axisWidth = scale * m_GeomDesc.m_AxisWidth;

	float	distMin = TNumericTraits<float>::kMax;
	s32		hintMin = -1;
	CFloat3	pointOnAxis = CFloat3::ZERO;

	// Note: we assume that ray.Origin() == m_CameraPosition

	// Check axis X-Y-Z
	for (u32 iAxis = 0; iAxis < 3; ++iAxis)
	{
		const CFloat3	axis = frame.Axis(iAxis).xyz();
		const CFloat3	cross = axis.Cross(ray.Direction());
		// -> project on (rayDir,cross)-plane with direction = ray.Direction()
		const CFloat3	planeNormal1 = cross.Cross(axis);
		const CFloat3	posToProj = ray.Direction() * (planeNormal1.Dot(CamToPos) / planeNormal1.Dot(ray.Direction())) - CamToPos;
		// -> distance along the axis
		const float		tAxis = posToProj.Dot(axis) / axis.LengthSquared();
		if (tAxis >= 0.f && tAxis <= axisSize)
		{
			const CFloat3	crossDir = ray.Direction().Cross(axis);
			const float		denom = crossDir.Length();
			if (denom > 1.e-6f)
			{
				const float	dist = PKAbs(CamToPos.Dot(crossDir)) / denom;
				if (dist < distMin && dist < axisWidth * 7.f)
				{
					distMin = dist;
					hintMin = s32(iAxis);
					pointOnAxis = position + tAxis * axis;
				}
			}
		}
	}

	outSelectionAxis =	(hintMin == 0) * CUint4(1, 0, 0, 0) +
						(hintMin == 1) * CUint4(0, 1, 0, 0) +
						(hintMin == 2) * CUint4(0, 0, 1, 0);

	if (outPosition != null && hintMin >= 0)
		*outPosition = pointOnAxis;
}

//----------------------------------------------------------------------------

void	CGizmo::_CheckSelectionSphere(const CRay &ray, CUint4 &outSelectionAxis, CFloat3 *outPosition)
{
	const CFloat4x4	frame = m_Transform;

	const CFloat3	position = frame.StrippedTranslations();
	const CFloat3	CamToPos = position - ray.Origin();

	const float		kMargin = 0.04f;

	const float		radius = _GetViewScale() * (m_GeomDesc.m_AxisLength + m_GeomDesc.m_ArrowHeight);
	const float		radius2Sup = PKSquared(radius);
	const float		radius2Inf = radius2Sup * PKSquared(1 - kMargin);

	const float		radiusExt = _GetViewScale() * (m_GeomDesc.m_AxisLength + m_GeomDesc.m_ArrowHeight * 2.f);
	const float		radiusExt2Sup = PKSquared(radiusExt);
	const float		radiusExt2Inf = radiusExt2Sup * PKSquared(1 - kMargin);

	// in fact, it should check the intersection with a torus, but this implies to get the roots of a quartic-equation ...

	PK_ASSERT(m_Type == GizmoRotating);
	const CFloat3	viewDepth = (m_CameraPosition - m_Transform.StrippedTranslations()).Normalized();

	// Project on the sphere

	PK_ASSERT(ray.Direction().IsNormalized(1.e-4f));
	const float		tRayCenter = CamToPos.Dot(ray.Direction());
	const float		tOffsetSquared = tRayCenter * tRayCenter - CamToPos.LengthSquared() + radius * radius;
	const bool		hitSphere = tOffsetSquared >= 0;

	const float		tOffset = hitSphere ? PKSqrt(tOffsetSquared) : 0.f;
	const float		tRayClosestPoint = tRayCenter - tOffset;

	const CFloat3	CamToSphere = tRayClosestPoint * ray.Direction();

	CFloat3			pointOnSpin = ray.Origin() + CamToSphere;

	if (hitSphere && tRayClosestPoint > 0.f) // not behind the camera
	{
		outSelectionAxis = CUint4(1, 1, 1, 0); // full rotation
	}

	// Check if it is close of an axis, for rotation on specific axis (x, y, z or screen-space)

	const CFloat3	PosToSphere = CamToSphere - CamToPos; // we assume that ray.Origin() == m_CameraPosition

	float			distMin = TNumericTraits<float>::kMax;

	// Check ring X-Y-Z
	for (u32 iAxis = 0; iAxis < 3; ++iAxis)
	{
		const CFloat3	planeNormal = frame.Axis(iAxis).xyz();
		const float		tAxisPlane = planeNormal.Dot(CamToPos) / planeNormal.Dot(ray.Direction());
		if (tAxisPlane > 0.f)
		{
			// Try to project directly on the plane
			const CFloat3	posToProj = ray.Direction() * tAxisPlane - CamToPos; // we assume that ray.Origin() == m_CameraPosition
			const float		dist2 = posToProj.LengthSquared();
			if (posToProj.Dot(viewDepth) > 0.f && dist2 >= radius2Inf && dist2 <= radius2Sup && (dist2 - radius2Inf) < distMin)
			{
				distMin = dist2 - radius2Inf;
				outSelectionAxis = CUint4::ZERO;
				outSelectionAxis.Axis(iAxis) = 1;
				pointOnSpin = ray.Origin() + ray.Direction() * tAxisPlane;
			}
			// Try to project on the shpere, then on the plane
			else if (hitSphere)
			{
				const float		tRayAbs = PKAbs(planeNormal.Dot(PosToSphere));
				if (tRayAbs < 0.05f * radius && tRayAbs < distMin)
				{
					distMin = tRayAbs;
					outSelectionAxis = CUint4::ZERO;
					outSelectionAxis.Axis(iAxis) = 1;
					pointOnSpin = ray.Origin() + ray.Direction() * tAxisPlane;
				}
			}
		}
	}

	// Check ring Screen-Space
	{
		const CFloat3	planeNormal = viewDepth;
		const float		tAxisPlane = planeNormal.Dot(CamToPos) / planeNormal.Dot(ray.Direction());
		const CFloat3	posToProj = ray.Direction() * tAxisPlane - CamToPos; // we assume that ray.Origin() == m_CameraPosition
		const float		dist2 = posToProj.LengthSquared();
		if (tAxisPlane > 0.f && dist2 >= radiusExt2Inf && dist2 <= radiusExt2Sup && (dist2 - radiusExt2Inf) < distMin)
		{
			distMin = dist2 - radius2Inf;
			outSelectionAxis = CUint4::WAXIS;
			pointOnSpin = ray.Origin() + ray.Direction() * tAxisPlane;
		}
	}

	if (outPosition != null)
		*outPosition = pointOnSpin;
}

//----------------------------------------------------------------------------

struct	SGizmoConstant
{
	CFloat4x4	m_ModelViewProj;
	CFloat4		m_HoveredColor;
	CFloat4		m_GrabbedColor;
};

//----------------------------------------------------------------------------

struct	SGizmoVertex
{
	CFloat3		m_Position;
	CFloat3		m_Color;
};

//----------------------------------------------------------------------------

bool	CGizmoDrawer::CreateRenderStates(	const RHI::PApiManager &apiManager,
											CShaderLoader &loader,
											const TMemoryView<const RHI::SRenderTargetDesc> &frameBufferLayout,
											const RHI::PRenderPass &bakedRenderPass,
											u32 subPassIdx)
{
	m_Solid = apiManager->CreateRenderState(RHI::SRHIResourceInfos("Gizmo Solid Render State"));
	m_Additive = apiManager->CreateRenderState(RHI::SRHIResourceInfos("Gizmo Additive Render State"));

	if (m_Solid == null || m_Additive == null)
		return false;

	m_Solid->m_RenderState.m_PipelineState.m_DynamicViewport = true;
	m_Solid->m_RenderState.m_PipelineState.m_DynamicScissor = true;
	m_Solid->m_RenderState.m_PipelineState.m_DrawMode = RHI::DrawModeTriangle;

	if (!m_Solid->m_RenderState.m_InputVertexBuffers.PushBack().Valid())
		return false;
	m_Solid->m_RenderState.m_InputVertexBuffers.Last().m_Stride = sizeof(SGizmoVertex);

	FillGizmoShaderBindings(m_Solid->m_RenderState.m_ShaderBindings);
	FillGizmoShaderBindings(m_Additive->m_RenderState.m_ShaderBindings);

	m_Solid->m_RenderState.m_ShaderBindings.m_InputAttributes[0].m_BufferIdx = 0;
	m_Solid->m_RenderState.m_ShaderBindings.m_InputAttributes[0].m_StartOffset = 0;

	m_Solid->m_RenderState.m_ShaderBindings.m_InputAttributes[1].m_BufferIdx = 0;
	m_Solid->m_RenderState.m_ShaderBindings.m_InputAttributes[1].m_StartOffset = sizeof(CFloat3);

	CShaderLoader::SShadersPaths shadersPaths;
	shadersPaths.m_Fragment = GIZMO_FRAGMENT_SHADER_PATH;
	shadersPaths.m_Vertex = GIZMO_VERTEX_SHADER_PATH;

	if (loader.LoadShader(m_Solid->m_RenderState, shadersPaths, apiManager) == false)
		return false;

	m_Additive->m_RenderState = m_Solid->m_RenderState;
	m_Additive->m_RenderState.m_PipelineState.m_Blending = true;
	m_Additive->m_RenderState.m_PipelineState.m_ColorBlendingDst = RHI::BlendOne;
	m_Additive->m_RenderState.m_PipelineState.m_ColorBlendingSrc = RHI::BlendOne;

	if (!apiManager->BakeRenderState(m_Solid, frameBufferLayout, bakedRenderPass, subPassIdx) ||
		!apiManager->BakeRenderState(m_Additive, frameBufferLayout, bakedRenderPass, subPassIdx))
		return false;

	return true;
}

//----------------------------------------------------------------------------

static CFloat4	_srgb_to_linear(const CFloat4 &c)
{
	return CFloat4(	powf(c.x(), 2.2f),
					powf(c.y(), 2.2f),
					powf(c.z(), 2.2f),
					c.w());
}

//----------------------------------------------------------------------------

static const CFloat4	kGizmoAxisColors[4] =
{
#if 1	// Trying to mostly preserve luminance
	_srgb_to_linear(CFloat4(0.97f, 0.15f, 0.1f, 1.0f)),
	_srgb_to_linear(CFloat4(0.50f, 0.85f, 0.0f, 1.0f)),
	_srgb_to_linear(CFloat4(0.20f, 0.56f, 1.0f, 1.0f)),
#else
	CFloat4(1.0f, 0.2f, 0.2f, 1.0f),
	CFloat4(0.2f, 1.0f, 0.1f, 1.0f),
	CFloat4(0.1f, 0.2f, 1.0f, 1.0f),
#endif
	CFloat4(0.8f, 0.8f, 0.8f, 1.0f),
};

//----------------------------------------------------------------------------

bool	CGizmoDrawer::CreateVertexBuffer(const RHI::PApiManager &apiManager)
{
	// 1. Generates the geometry
	CMeshTriangleBatch	batch;

	batch.m_IStream.SetPrimitiveType(CMeshIStream::Triangles);
	batch.m_VStream.Reformat(VertexDeclaration::Position3f16_Color4f16);

	// -> Root

	m_DrawRoot.offset = batch.m_IStream.IndexCount();
	PrimitiveDiscretizers::BuildSphere(batch, CFloat4x4::IDENTITY, m_GeomDesc.m_RootSize, CFloat4(0.25f), 18);
	m_DrawRoot.count = batch.m_IStream.IndexCount() - m_DrawRoot.offset;

	// -> Sphere

	m_DrawSphere.offset = batch.m_IStream.IndexCount();
	PrimitiveDiscretizers::BuildSphere(batch, CFloat4x4::IDENTITY, m_GeomDesc.m_AxisLength + m_GeomDesc.m_ArrowHeight, CFloat4(0.05f), 48);
	m_DrawSphere.count = batch.m_IStream.IndexCount() - m_DrawSphere.offset;

	// -> Square handle

	const u32		up = CCoordinateFrame::AxisIndexer(Axis_Up);
	const CUint3	axisIndexer = CCoordinateFrame::AxesIndexer();

	CFloat4x4	xMatrix = CFloat4x4::IDENTITY;
	xMatrix.Axis(axisIndexer.x()) = CFloat4::ZAXIS;
	xMatrix.Axis(axisIndexer.y()) = CFloat4::XAXIS;
	xMatrix.Axis(axisIndexer.z()) = CFloat4::YAXIS;

	CFloat4x4	yMatrix = CFloat4x4::IDENTITY;
	yMatrix.Axis(axisIndexer.x()) = CFloat4::XAXIS;
	yMatrix.Axis(axisIndexer.y()) = CFloat4::YAXIS;
	yMatrix.Axis(axisIndexer.z()) = CFloat4::ZAXIS;

	CFloat4x4	zMatrix = CFloat4x4::IDENTITY;
	zMatrix.Axis(axisIndexer.x()) = CFloat4::YAXIS;
	zMatrix.Axis(axisIndexer.y()) = CFloat4::ZAXIS;
	zMatrix.Axis(axisIndexer.z()) = CFloat4::XAXIS;

	const CFloat4	kLightGray = CFloat4(0.2f, 0.2f, 0.2f, 1.0f);

	{
		const float		squareSizeIn = m_GeomDesc.m_SquareDist;
		const float		squareSizeOu = squareSizeIn + m_GeomDesc.m_SquareSize;

		const CFloat3	squarePt00 = CFloat3(squareSizeIn, 0.f, squareSizeIn);
		const CFloat3	squarePt01 = CFloat3(squareSizeIn, 0.f, squareSizeOu);
		const CFloat3	squarePt10 = CFloat3(squareSizeOu, 0.f, squareSizeIn);
		const CFloat3	squarePt11 = CFloat3(squareSizeOu, 0.f, squareSizeOu);

		m_DrawSquareX.offset = batch.m_IStream.IndexCount();

		PrimitiveDiscretizers::BuildTriangle(batch, xMatrix, squarePt00, squarePt01, squarePt11, kLightGray);
		PrimitiveDiscretizers::BuildTriangle(batch, xMatrix, squarePt11, squarePt10, squarePt00, kLightGray);

		m_DrawSquareX.count = batch.m_IStream.IndexCount() - m_DrawSquareX.offset;

		PrimitiveDiscretizers::BuildTriangle(batch, yMatrix, squarePt00, squarePt01, squarePt11, kLightGray);
		PrimitiveDiscretizers::BuildTriangle(batch, yMatrix, squarePt11, squarePt10, squarePt00, kLightGray);

		PrimitiveDiscretizers::BuildTriangle(batch, zMatrix, squarePt00, squarePt01, squarePt11, kLightGray);
		PrimitiveDiscretizers::BuildTriangle(batch, zMatrix, squarePt11, squarePt10, squarePt00, kLightGray);

		PK_ASSERT(m_DrawSquareX.offset + m_DrawSquareX.count * 3 == batch.m_IStream.IndexCount());
	}

	// -> Axis for translation
	{
		m_DrawAxisTX.offset = batch.m_IStream.IndexCount();
		{
			CFloat4x4	transf = xMatrix;
			transf.StrippedTranslations() = transf.Axis(up).xyz() * m_GeomDesc.m_AxisLength / 2.0f;
			PrimitiveDiscretizers::BuildCylinder(batch, transf, m_GeomDesc.m_AxisWidth, 0, m_GeomDesc.m_AxisLength, kGizmoAxisColors[0], true, 6);
			transf.StrippedTranslations() = transf.Axis(up).xyz() * m_GeomDesc.m_AxisLength;
			PrimitiveDiscretizers::BuildCone(batch, transf, m_GeomDesc.m_ArrowWidth, m_GeomDesc.m_ArrowHeight, kGizmoAxisColors[0], true, 8);
		}
		m_DrawAxisTX.count = batch.m_IStream.IndexCount() - m_DrawAxisTX.offset;

		{
			CFloat4x4	transf = yMatrix;
			transf.StrippedTranslations() = transf.Axis(up).xyz() * m_GeomDesc.m_AxisLength / 2.0f;
			PrimitiveDiscretizers::BuildCylinder(batch, transf, m_GeomDesc.m_AxisWidth, 0, m_GeomDesc.m_AxisLength, kGizmoAxisColors[1], true, 6);
			transf.StrippedTranslations() = transf.Axis(up).xyz() * m_GeomDesc.m_AxisLength;
			PrimitiveDiscretizers::BuildCone(batch, transf, m_GeomDesc.m_ArrowWidth, m_GeomDesc.m_ArrowHeight, kGizmoAxisColors[1], true, 8);
		}

		{
			CFloat4x4	transf = zMatrix;
			transf.StrippedTranslations() = transf.Axis(up).xyz() * m_GeomDesc.m_AxisLength / 2.0f;
			PrimitiveDiscretizers::BuildCylinder(batch, transf, m_GeomDesc.m_AxisWidth, 0, m_GeomDesc.m_AxisLength, kGizmoAxisColors[2], true, 6);
			transf.StrippedTranslations() = transf.Axis(up).xyz() * m_GeomDesc.m_AxisLength;
			PrimitiveDiscretizers::BuildCone(batch, transf, m_GeomDesc.m_ArrowWidth, m_GeomDesc.m_ArrowHeight, kGizmoAxisColors[2], true, 8);
		}

		PK_ASSERT(m_DrawAxisTX.offset + m_DrawAxisTX.count * 3 == batch.m_IStream.IndexCount());
	}

	// -> Axis for Scale
	{
		const CAABB	box = CAABB(CFloat3(-m_GeomDesc.m_ArrowWidth), CFloat3(m_GeomDesc.m_ArrowWidth));

		m_DrawAxisSX.offset = batch.m_IStream.IndexCount();
		{
			CFloat4x4	transf = xMatrix;
			transf.StrippedTranslations() = transf.Axis(up).xyz() * m_GeomDesc.m_AxisLength / 2.0f;
			PrimitiveDiscretizers::BuildCylinder(batch, transf, m_GeomDesc.m_AxisWidth, 0, m_GeomDesc.m_AxisLength, kGizmoAxisColors[0], true, 6);
			transf.StrippedTranslations() = transf.Axis(up).xyz() * m_GeomDesc.m_AxisLength;
			PrimitiveDiscretizers::BuildBox(batch, transf, box, kGizmoAxisColors[0]);
		}
		m_DrawAxisSX.count = batch.m_IStream.IndexCount() - m_DrawAxisSX.offset;

		{
			CFloat4x4	transf = yMatrix;
			transf.StrippedTranslations() = transf.Axis(up).xyz() * m_GeomDesc.m_AxisLength / 2.0f;
			PrimitiveDiscretizers::BuildCylinder(batch, transf, m_GeomDesc.m_AxisWidth, 0, m_GeomDesc.m_AxisLength, kGizmoAxisColors[1], true, 6);
			transf.StrippedTranslations() = transf.Axis(up).xyz() * m_GeomDesc.m_AxisLength;
			PrimitiveDiscretizers::BuildBox(batch, transf, box, kGizmoAxisColors[1]);
		}

		{
			CFloat4x4	transf = zMatrix;
			transf.StrippedTranslations() = transf.Axis(up).xyz() * m_GeomDesc.m_AxisLength / 2.0f;
			PrimitiveDiscretizers::BuildCylinder(batch, transf, m_GeomDesc.m_AxisWidth, 0, m_GeomDesc.m_AxisLength, kGizmoAxisColors[2], true, 6);
			transf.StrippedTranslations() = transf.Axis(up).xyz() * m_GeomDesc.m_AxisLength;
			PrimitiveDiscretizers::BuildBox(batch, transf, box, kGizmoAxisColors[2]);
		}

		PK_ASSERT(m_DrawAxisSX.offset + m_DrawAxisSX.count * 3 == batch.m_IStream.IndexCount());
	}

	// -> Rotation handle
	{
		m_DrawRotationX.offset = batch.m_IStream.IndexCount();
		PrimitiveDiscretizers::BuildHalfTorus(batch, xMatrix, m_GeomDesc.m_AxisLength + m_GeomDesc.m_ArrowHeight, m_GeomDesc.m_AxisWidth, kGizmoAxisColors[0], 48, 8);
		m_DrawRotationX.count = batch.m_IStream.IndexCount() - m_DrawRotationX.offset;

		PrimitiveDiscretizers::BuildHalfTorus(batch, yMatrix, m_GeomDesc.m_AxisLength + m_GeomDesc.m_ArrowHeight, m_GeomDesc.m_AxisWidth, kGizmoAxisColors[1], 48, 8);
		PrimitiveDiscretizers::BuildHalfTorus(batch, zMatrix, m_GeomDesc.m_AxisLength + m_GeomDesc.m_ArrowHeight, m_GeomDesc.m_AxisWidth, kGizmoAxisColors[2], 48, 8);

		PK_ASSERT(m_DrawRotationX.offset + m_DrawRotationX.count * 3 == batch.m_IStream.IndexCount());

		m_DrawRotationSP.offset = batch.m_IStream.IndexCount();
		PrimitiveDiscretizers::BuildDisk(batch, zMatrix, m_GeomDesc.m_AxisLength + m_GeomDesc.m_ArrowHeight * 2.f, m_GeomDesc.m_AxisLength + m_GeomDesc.m_ArrowHeight, kLightGray, 48);
		m_DrawRotationSP.count = batch.m_IStream.IndexCount() - m_DrawRotationSP.offset;
	}

	// 2. Upload buffers to RHI

	// 2.1 Vertex buffer

	m_GizmoVtx = apiManager->CreateGpuBuffer(RHI::SRHIResourceInfos("Gizmo Vertex Buffer"), RHI::VertexBuffer, batch.m_VStream.VertexCount() * sizeof(SGizmoVertex));
	if (m_GizmoVtx == null)
		return false;

	SGizmoVertex	*vtxData = static_cast<SGizmoVertex*>(apiManager->MapCpuView(m_GizmoVtx));
	if (vtxData == null)
		return false;

	TStridedMemoryView<const CFloat3>	positions = batch.m_VStream.Positions();
	TStridedMemoryView<const CFloat4>	colors = batch.m_VStream.Colors();

	for (u32 i = 0; i < batch.m_VStream.VertexCount(); ++i)
	{
		vtxData[i].m_Position = positions[i];
		vtxData[i].m_Color = colors[i].xyz();
	}

	apiManager->UnmapCpuView(m_GizmoVtx);

	// 2.2 Transfer the index buffer

	m_GizmoIdx = apiManager->CreateGpuBuffer(RHI::SRHIResourceInfos("Gizmo Index Buffer"), RHI::IndexBuffer, batch.m_IStream.IndexCount() * sizeof(u32));
	if (m_GizmoIdx == null)
		return false;

	u32	*idxData = static_cast<u32*>(apiManager->MapCpuView(m_GizmoIdx));
	if (idxData == null)
		return false;

	Mem::Copy(idxData, batch.m_IStream.Stream<u32>(), batch.m_IStream.StreamSize());

	apiManager->UnmapCpuView(m_GizmoIdx);

	return true;
}

//----------------------------------------------------------------------------

#if GIZMODRAWER_USE_IMGUI == 1
static void	_DrawText(const char *imGUIkey, const CInt2 &screenPosition, const CFloat4 &color, const char *format, ...) // colored text, vertical-centered
{
	const ImVec4	kColor = ImVec4(color.x(), color.y(), color.z(), color.w());
	const float		kFontSize = ImGui::GetFontSize();

	const ImVec2	kDispPos = ImVec2(screenPosition.x(), screenPosition.y() - kFontSize/2);
	const ImVec2	kDispSiz = ImVec2(kFontSize * 2, kFontSize * 2);
	ImGui::SetNextWindowPos(kDispPos, ImGuiSetCond_Always);
	ImGui::SetNextWindowSize(kDispSiz, ImGuiSetCond_Always);
	ImGui::SetNextWindowBgAlpha(0.f);
	const u32		kWindowFlags =	ImGuiWindowFlags_NoTitleBar |
									ImGuiWindowFlags_NoResize |
									ImGuiWindowFlags_NoMove |
									ImGuiWindowFlags_NoSavedSettings |
									ImGuiWindowFlags_NoInputs |
									ImGuiWindowFlags_NoScrollbar |
									ImGuiWindowFlags_NoScrollWithMouse |
									ImGuiWindowFlags_AlwaysAutoResize |
									ImGuiWindowFlags_NoFocusOnAppearing |
									ImGuiWindowFlags_NoBringToFrontOnFocus |
									ImGuiWindowFlags_NoNav;

	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 0.f);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);

	ImGui::Begin(imGUIkey, null, kWindowFlags);

	va_list	arglist;
	va_start(arglist, format);
	ImGui::TextColoredV(kColor, format, arglist);
	va_end(arglist);

	ImGui::End();

	ImGui::PopStyleVar(6);
}
#endif

//----------------------------------------------------------------------------

bool	CGizmoDrawer::DrawGizmo(const RHI::PCommandBuffer &cmdBuff, const CGizmo &gizmo) const
{
	if (gizmo.m_Type == GizmoNone)
		return true; // don't draw if the Gizmo mode is not set

	CFloat4x4		MVP = gizmo.m_Transform * gizmo.m_ViewProj;
	const float		scale = gizmo._GetViewScale();
	MVP.Axis(0) *= scale;
	MVP.Axis(1) *= scale;
	MVP.Axis(2) *= scale;

	SGizmoConstant	gizmoConstant;

	const bool	gizmoGrabbedAxisFull = gizmo.m_GrabbedAxis.xyz() == CUint3::ONE;
	const bool	gizmoHoveredAxisFull = gizmo.m_HoveredAxis.xyz() == CUint3::ONE;

	if (!PK_VERIFY(m_Additive != null && m_Solid != null && m_GizmoVtx != null && m_GizmoIdx != null))
		return false;

	const bool	preRenderAdditive = gizmo.m_Type == GizmoRotating || (gizmo.m_Type != GizmoRotating && gizmo.IsGrabbed());
	if (preRenderAdditive)
		cmdBuff->BindRenderState(m_Additive);
	else
		cmdBuff->BindRenderState(m_Solid);

	cmdBuff->BindVertexBuffers(TMemoryView<const RHI::PGpuBuffer>(m_GizmoVtx));
	cmdBuff->BindIndexBuffer(m_GizmoIdx, 0, RHI::IndexBuffer32Bit);

	const CFloat4	kRotate_ShadeFlats = CFloat4(0.01f, 0.01f, 0.01f, 1);
	const CFloat4	kRotate_ShadeHover = CFloat4(0.02f, 0.02f, 0.02f, 1);
	const CFloat4	kDisabled_ShadeFlats = CFloat4(0.1f, 0.1f, 0.1f, 1);
	const CFloat4	kHandle_ShadeFlats = CFloat4(0.1f, 0.1f, 0.1f, 1);
	const CFloat4	kGrabbed_ShadeOld = CFloat4(0.07f, 0.07f, 0.07f, 1);

	if (gizmo.m_Type == GizmoRotating)
	{
		gizmoConstant.m_ModelViewProj = MVP;

		gizmoConstant.m_GrabbedColor = (gizmo.m_AllowedModesMask & GizmoRotating) != 0 ? CFloat4(0.1f, 0.02f, 0.1f, gizmoGrabbedAxisFull ? 1.f : 0.f) : kRotate_ShadeFlats;
		gizmoConstant.m_HoveredColor = gizmo.m_GrabbedAxis.w() == 0 ? CFloat4(0.1f, 0.1f, 0.02f, gizmoHoveredAxisFull ? 1.f : 0.f) : kRotate_ShadeHover;
		cmdBuff->PushConstant(&gizmoConstant, 0);
		// Draw Sphere
		cmdBuff->DrawIndexed(m_DrawSphere.offset, 0, m_DrawSphere.count);
	}
	if (gizmo.m_Type != GizmoRotating && gizmo.IsGrabbed())
	{
		CFloat4x4		MVPpre = gizmo.m_TransformPrev * gizmo.m_ViewProj;
		const float		scalePre = gizmo._GetViewScalePre();
		MVPpre.Axis(0) *= scalePre;
		MVPpre.Axis(1) *= scalePre;
		MVPpre.Axis(2) *= scalePre;
		gizmoConstant.m_ModelViewProj = MVPpre;
		// Origin - Axis
		const SDrawIndexedRange &drawAxis = gizmo.m_Type == GizmoScaling ? m_DrawAxisSX : m_DrawAxisTX;
		gizmoConstant.m_GrabbedColor = kGrabbed_ShadeOld;
		gizmoConstant.m_HoveredColor.w() = 0;
		cmdBuff->PushConstant(&gizmoConstant, 0);
		cmdBuff->DrawIndexed(drawAxis.offset, 0, drawAxis.count * 3);
	}

	const bool	inactive = (gizmo.m_AllowedModesMask & gizmo.m_Type) == 0;

	if (gizmo.m_Type != GizmoRotating)
	{
		cmdBuff->BindRenderState(m_Solid);

		// Draw Axis
		const SDrawIndexedRange &drawAxis = gizmo.m_Type == GizmoScaling ? m_DrawAxisSX : m_DrawAxisTX;

		gizmoConstant.m_ModelViewProj = MVP;
		gizmoConstant.m_GrabbedColor = (!inactive) ? CFloat4(1, 0, 1, 0) : kDisabled_ShadeFlats;
		gizmoConstant.m_HoveredColor = (!inactive) ? CFloat4(1, 1, 0, 0) : kDisabled_ShadeFlats;
		const CUint4	grabbedAxis = (!inactive) ? gizmo.m_GrabbedAxis : CUint4(0);
		const CUint4	hoveredAxis = (!inactive) ? gizmo.m_HoveredAxis : CUint4(1);

		gizmoConstant.m_GrabbedColor.w() = grabbedAxis.x();
		gizmoConstant.m_HoveredColor.w() = hoveredAxis.x();
		if (gizmo.m_Type == GizmoScaling && gizmo.m_Clicked)
		{
			CFloat4x4	transformScale = gizmo.m_Transform;
			transformScale.Axis(0) *= scale;
			transformScale.Axis(1) = scale * gizmo.m_TransformPrev.Axis(1);
			transformScale.Axis(2) = scale * gizmo.m_TransformPrev.Axis(2);
			gizmoConstant.m_ModelViewProj = transformScale * gizmo.m_ViewProj;
		}
		cmdBuff->PushConstant(&gizmoConstant, 0);
		cmdBuff->DrawIndexed(drawAxis.offset, 0, drawAxis.count);

		gizmoConstant.m_GrabbedColor.w() = grabbedAxis.y();
		gizmoConstant.m_HoveredColor.w() = hoveredAxis.y();
		if (gizmo.m_Type == GizmoScaling && gizmo.m_Clicked)
		{
			CFloat4x4 transformScale = gizmo.m_Transform;
			transformScale.Axis(0) = scale * gizmo.m_TransformPrev.Axis(0);
			transformScale.Axis(1) *= scale;
			transformScale.Axis(2) = scale * gizmo.m_TransformPrev.Axis(2);
			gizmoConstant.m_ModelViewProj = transformScale * gizmo.m_ViewProj;
		}
		cmdBuff->PushConstant(&gizmoConstant, 0);
		cmdBuff->DrawIndexed(drawAxis.offset + drawAxis.count, 0, drawAxis.count);

		gizmoConstant.m_GrabbedColor.w() = grabbedAxis.z();
		gizmoConstant.m_HoveredColor.w() = hoveredAxis.z();
		if (gizmo.m_Type == GizmoScaling && gizmo.m_Clicked)
		{
			CFloat4x4	transformScale = gizmo.m_Transform;
			transformScale.Axis(0) = scale * gizmo.m_TransformPrev.Axis(0);
			transformScale.Axis(1) = scale * gizmo.m_TransformPrev.Axis(1);
			transformScale.Axis(2) *= scale;
			gizmoConstant.m_ModelViewProj = transformScale * gizmo.m_ViewProj;
		}
		cmdBuff->PushConstant(&gizmoConstant, 0);
		cmdBuff->DrawIndexed(drawAxis.offset + drawAxis.count * 2, 0, drawAxis.count);

		gizmoConstant.m_ModelViewProj = MVP;

		if (gizmo.m_Type != GizmoShowAxis && gizmo.m_Type != GizmoShowAxisWithoutNames)
		{
			cmdBuff->BindRenderState(m_Additive);

			// Draw Root
			gizmoConstant.m_GrabbedColor = CFloat4(0.1f, 0.02f, 0.1f, gizmoGrabbedAxisFull ? 1.f : 0.f);
			gizmoConstant.m_HoveredColor = CFloat4(0.1f, 0.1f, 0.02f, gizmoHoveredAxisFull ? 1.f : 0.f);
			if (gizmo.m_Type != GizmoScaling || gizmo.m_GrabbedAxis == CUint4::ZERO || gizmoGrabbedAxisFull)
			{
				cmdBuff->PushConstant(&gizmoConstant, 0);
				cmdBuff->DrawIndexed(m_DrawRoot.offset, 0, m_DrawRoot.count);
			}

			// Draw Square handles
			const float		desatFactor = inactive ? 1.0f : 0.5f;
			const CFloat4	pickSquareColor_X = PKLerp(kGizmoAxisColors[0] * CFloat4(0.5f).xyz1(), kHandle_ShadeFlats, desatFactor);
			const CFloat4	pickSquareColor_Y = PKLerp(kGizmoAxisColors[1] * CFloat4(0.5f).xyz1(), kHandle_ShadeFlats, desatFactor);
			const CFloat4	pickSquareColor_Z = PKLerp(kGizmoAxisColors[2] * CFloat4(0.5f).xyz1(), kHandle_ShadeFlats, desatFactor);

			gizmoConstant.m_GrabbedColor = gizmo.m_GrabbedAxis.yz() == CUint2::ONE ? CFloat4(0.50f, 0.02f, 0.50f, 1.f) : pickSquareColor_X;
			gizmoConstant.m_HoveredColor = gizmo.m_HoveredAxis.yz() == CUint2::ONE ? CFloat4(0.50f, 0.50f, 0.02f, 1.f) : CFloat4::ZERO;
			if (gizmo.m_GrabbedAxis == CUint4::ZERO || gizmo.m_GrabbedAxis == CUint4(0, 1, 1, 0))
			{
				cmdBuff->PushConstant(&gizmoConstant, 0);
				cmdBuff->DrawIndexed(m_DrawSquareX.offset, 0, m_DrawSquareX.count);
			}

			gizmoConstant.m_GrabbedColor = gizmo.m_GrabbedAxis.xz() == CUint2::ONE ? CFloat4(0.50f, 0.02f, 0.50f, 1.f) : pickSquareColor_Y;
			gizmoConstant.m_HoveredColor = gizmo.m_HoveredAxis.xz() == CUint2::ONE ? CFloat4(0.50f, 0.50f, 0.02f, 1.f) : CFloat4::ZERO;
			if (gizmo.m_GrabbedAxis == CUint4::ZERO || gizmo.m_GrabbedAxis == CUint4(1, 0, 1, 0))
			{
				cmdBuff->PushConstant(&gizmoConstant, 0);
				cmdBuff->DrawIndexed(m_DrawSquareX.offset + m_DrawSquareX.count, 0, m_DrawSquareX.count);
			}

			gizmoConstant.m_GrabbedColor = gizmo.m_GrabbedAxis.xy() == CUint2::ONE ? CFloat4(0.50f, 0.02f, 0.50f, 1.f) : pickSquareColor_Z;
			gizmoConstant.m_HoveredColor = gizmo.m_HoveredAxis.xy() == CUint2::ONE ? CFloat4(0.50f, 0.50f, 0.02f, 1.f) : CFloat4::ZERO;
			if (gizmo.m_GrabbedAxis == CUint4::ZERO || gizmo.m_GrabbedAxis == CUint4(1, 1, 0, 0))
			{
				cmdBuff->PushConstant(&gizmoConstant, 0);
				cmdBuff->DrawIndexed(m_DrawSquareX.offset + m_DrawSquareX.count * 2, 0, m_DrawSquareX.count);
			}
		}
	}
	else // (gizmo.m_Type == GizmoRotating)
	{
		// Draw Rotation handles
		cmdBuff->BindRenderState(m_Solid);

		gizmoConstant.m_GrabbedColor = CFloat4(1, 0, 1, 0);
		gizmoConstant.m_HoveredColor = CFloat4(1, 1, 0, 0);

		CFloat4x4	MVProtating;

		if (gizmo.m_GrabbedAxis.w() == 0 && !inactive)
		{
			MVProtating = gizmo.m_TransformRotating[0] * gizmo.m_ViewProj;
			MVProtating.Axis(0) *= scale;
			MVProtating.Axis(1) *= scale;
			MVProtating.Axis(2) *= scale;
			gizmoConstant.m_ModelViewProj = MVProtating;

			gizmoConstant.m_GrabbedColor.w() = gizmoGrabbedAxisFull ? 0 : gizmo.m_GrabbedAxis.x();
			gizmoConstant.m_HoveredColor.w() = gizmoHoveredAxisFull ? 0 : gizmo.m_HoveredAxis.x();
			cmdBuff->PushConstant(&gizmoConstant, 0);
			cmdBuff->DrawIndexed(m_DrawRotationX.offset, 0, m_DrawRotationX.count);

			MVProtating = gizmo.m_TransformRotating[1] * gizmo.m_ViewProj;
			MVProtating.Axis(0) *= scale;
			MVProtating.Axis(1) *= scale;
			MVProtating.Axis(2) *= scale;
			gizmoConstant.m_ModelViewProj = MVProtating;

			gizmoConstant.m_GrabbedColor.w() = gizmoGrabbedAxisFull ? 0 : gizmo.m_GrabbedAxis.y();
			gizmoConstant.m_HoveredColor.w() = gizmoHoveredAxisFull ? 0 : gizmo.m_HoveredAxis.y();
			cmdBuff->PushConstant(&gizmoConstant, 0);
			cmdBuff->DrawIndexed(m_DrawRotationX.offset + m_DrawRotationX.count, 0, m_DrawRotationX.count);

			MVProtating = gizmo.m_TransformRotating[2] * gizmo.m_ViewProj;
			MVProtating.Axis(0) *= scale;
			MVProtating.Axis(1) *= scale;
			MVProtating.Axis(2) *= scale;
			gizmoConstant.m_ModelViewProj = MVProtating;

			gizmoConstant.m_GrabbedColor.w() = gizmoGrabbedAxisFull ? 0 : gizmo.m_GrabbedAxis.z();
			gizmoConstant.m_HoveredColor.w() = gizmoHoveredAxisFull ? 0 : gizmo.m_HoveredAxis.z();
			cmdBuff->PushConstant(&gizmoConstant, 0);
			cmdBuff->DrawIndexed(m_DrawRotationX.offset + m_DrawRotationX.count * 2, 0, m_DrawRotationX.count);
		}

		if ((!gizmo.m_Clicked || gizmo.m_GrabbedAxis.w() != 0) && !inactive)
		{
			MVProtating = gizmo.m_TransformRotating[3] * gizmo.m_ViewProj;
			MVProtating.Axis(0) *= scale;
			MVProtating.Axis(1) *= scale;
			MVProtating.Axis(2) *= scale;
			gizmoConstant.m_ModelViewProj = MVProtating;
			gizmoConstant.m_GrabbedColor.w() = gizmo.m_GrabbedAxis.w();
			gizmoConstant.m_HoveredColor.w() = gizmo.m_HoveredAxis.w();
			cmdBuff->PushConstant(&gizmoConstant, 0);
			cmdBuff->DrawIndexed(m_DrawRotationSP.offset, 0, m_DrawRotationSP.count);
		}

		cmdBuff->BindRenderState(m_Additive);

		// Draw Axis
		gizmoConstant.m_GrabbedColor = kHandle_ShadeFlats;
		gizmoConstant.m_HoveredColor.w() = 0;
		gizmoConstant.m_ModelViewProj = MVP;
		cmdBuff->PushConstant(&gizmoConstant, 0);
		cmdBuff->DrawIndexed(m_DrawAxisTX.offset, 0, m_DrawAxisTX.count * 3);
	}

#if GIZMODRAWER_USE_IMGUI == 1
	if (gizmo.m_Type == GizmoShowAxis)
	{
		const CInt3		axisMapper = CCoordinateFrame::TransposeRemapper(Frame_RightHand_Y_Up, CCoordinateFrame::GlobalFrame());

		const char		kAxisNamesXYZ[3] = { 'X', 'Y', 'Z' };
		const char		kAxisNamesSUF[6] = { 'R', 'U', 'B', 'L', 'D', 'F' };

		const float		fontSize = ImGui::GetFontSize();

		for (u32 iAxis = 0; iAxis < 3; ++iAxis)
		{
			const CFloat4	center = gizmo.m_Transform.WAxis() + scale * (gizmo.m_GeomDesc.m_AxisLength + gizmo.m_GeomDesc.m_ArrowHeight) * gizmo.m_Transform.Axis(iAxis);

			const CFloat4	camToGizmo_ClipSpace = gizmo.m_ViewProj.TransformVector(gizmo.m_Transform.WAxis());
			const CFloat4	camToLetter_ClipSpace = gizmo.m_ViewProj.TransformVector(center);
			const CFloat2	gizmoToLetter_ScreenSpace = 0.5f * (camToLetter_ClipSpace.xy() / camToLetter_ClipSpace.w() - camToGizmo_ClipSpace.xy() / camToGizmo_ClipSpace.w()) * gizmo.m_ContextSize;

			const float		lengthSquared_GC = gizmoToLetter_ScreenSpace.LengthSquared();

			if (lengthSquared_GC < 0.25f * fontSize * fontSize)
				continue;

			const CFloat4	axisTip_ClipSpace = gizmo.m_ViewProj.TransformVector(center);
			const CInt2		axisTip_ScreenSpace = (0.5f + 0.5f * axisTip_ClipSpace.xy() / axisTip_ClipSpace.w()) * gizmo.m_ContextSize;

			CInt2			letter1_ScreenSpace = axisTip_ScreenSpace;

			// FIXME: Here, we should instead try to collide the letter bounding rect with the viewport corners,
			// and use that to decide where to push the letters, instead of the hardcoded logic.
			// The code below is a hardcoded behavior for bottom-left placement, now that it can be configured,
			// it breaks down for any other location.

			if (gizmoToLetter_ScreenSpace.x() + PKAbs(gizmoToLetter_ScreenSpace.y()) < 0.f)
			{
				const CInt2		fontOffsetY = CInt2(0, s32(fontSize));
				if (gizmoToLetter_ScreenSpace.y() < 0)
					letter1_ScreenSpace = axisTip_ScreenSpace - fontOffsetY;
				else
					letter1_ScreenSpace = axisTip_ScreenSpace + fontOffsetY;
				letter1_ScreenSpace.x() -= 0.3f * fontSize;
			}
			else if (gizmoToLetter_ScreenSpace.y() > 0.f)
			{
				letter1_ScreenSpace = axisTip_ScreenSpace + CInt2(s32(0.8f * fontSize), 0);
			}
			else
			{
				letter1_ScreenSpace = axisTip_ScreenSpace + 0.8f * fontSize * gizmoToLetter_ScreenSpace.Normalized();
			}

			const CInt2		letter2_ScreenSpace = letter1_ScreenSpace + CInt2(s32(fontSize * 0.7f), 0);

			// Use address of gizmo and axis name to generate a unique name that will be used by ImGui::Begin
			// Note: ImGui uses names as IDs too
			// That way we avoid a bug where ImGui only shows one group of axis names if multiple gizmo use GizmoShowAxis type
			const CString	nameKeyXYZ = CString::Format("%p-%c", &gizmo, kAxisNamesXYZ[iAxis]);
			_DrawText(nameKeyXYZ.Data(), letter1_ScreenSpace, kGizmoAxisColors[iAxis], "%c", kAxisNamesXYZ[iAxis]);

			const s32	axisMap = axisMapper.Axis(iAxis);
			const u32	axisIdx = PKAbs(axisMap) - 1;
			const u32	axisNameIdx = axisIdx + (axisMap > 0 ? 0 : 3);

			const CString	nameKeySUF = CString::Format("%p-%c", &gizmo, kAxisNamesSUF[axisNameIdx]);
			_DrawText(nameKeySUF.Data(), letter2_ScreenSpace, kGizmoAxisColors[3], "%c", kAxisNamesSUF[axisNameIdx]);
		}
	}
#else
	PK_ASSERT_NOT_IMPLEMENTED();

	if (gizmo.m_Type == GizmoShowAxis)
		return false;

#if 0
	// Extract of code (billboarding):

	const CFloat4	axisSide = scale * 0.5f * gizmo.m_GeomDesc.m_LetterSize * gizmo.m_ViewTransposed.Axis(axisIndexer.x()).xyz0();
	const CFloat4	axisUp   = scale * 0.5f * gizmo.m_GeomDesc.m_LetterSize * gizmo.m_ViewTransposed.Axis(axisIndexer.y()).xyz0();
	const CFloat4	xpy = axisSide + axisUp;
	const CFloat4	xmy = axisSide - axisUp;

	for (u32 iAxis = 0; iAxis < 3; ++iAxis)
	{
		// ...

		const CFloat4	center_ClipSpace = gizmo.m_ViewProj.TransformVector(center);
		const CFloat2	center_ScreenSpace = center_ClipSpace.xy() / center_ClipSpace.w();
		const CFloat2	centerPixelSnap_ScreenSpace = PKFloor(gizmo.m_ContextSize * center_ScreenSpace) / gizmo.m_ContextSize;
		const CFloat4	centerPixelSnap_ClipSpace = CFloat4(centerPixelSnap_ScreenSpace * center_ClipSpace.w(), center_ClipSpace.zw());
		const CFloat4	centerPixelSnap = gizmo.m_InvViewProj.TransformVector(centerPixelSnap_ClipSpace).xyz1();

		Positions[letterCount * 6 + 0] = centerPixelSnap - xpy;
		Positions[letterCount * 6 + 1] = centerPixelSnap + xmy;
		Positions[letterCount * 6 + 2] = centerPixelSnap + xpy;
		Positions[letterCount * 6 + 3] = centerPixelSnap + xpy;
		Positions[letterCount * 6 + 4] = centerPixelSnap - xmy;
		Positions[letterCount * 6 + 5] = centerPixelSnap - xpy;

		const u32	iAxisP1 = iAxis + 1;
		UVs[letterCount * 6 + 0].xy() = CFloat2(iAxis, 1.f) / 3.f;
		UVs[letterCount * 6 + 1].xy() = CFloat2(iAxisP1, 1.f) / 3.f;
		UVs[letterCount * 6 + 2].xy() = CFloat2(iAxisP1, 0.f) / 3.f;
		UVs[letterCount * 6 + 3].xy() = CFloat2(iAxisP1, 0.f) / 3.f;
		UVs[letterCount * 6 + 4].xy() = CFloat2(iAxis, 0.f) / 3.f;
		UVs[letterCount * 6 + 5].xy() = CFloat2(iAxis, 1.f) / 3.f;

		// ...
	}
#endif

#endif // GIZMODRAWER_USE_IMGUI == 1

	return true;
}

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
