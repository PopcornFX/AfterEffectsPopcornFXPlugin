//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#include <ae_precompiled.h>

#include "Panels/AEGP_GraphicalResourcesTreeModel.h"
#include "AEGP_World.h"
#include "AEGP_Scene.h"
#include "AEGP_LayerHolder.h"
#include "AEGP_AEPKConversion.h"

#include <QPainter>

#include <pk_maths/include/pk_maths_simd_vector.h>

//----------------------------------------------------------------------------

__AEGP_PK_BEGIN

const QSize		kGraphicResourceThumbSize = QSize(35, 35);
const QSize		kResetButtonSize = QSize(16, 16);

//----------------------------------------------------------------------------

CGraphicResetButtonView::CGraphicResetButtonView()
{
	m_Pixmap = QPixmap(":/icons/reset.png");
}

//----------------------------------------------------------------------------

void	CGraphicResetButtonView::paint(QPainter *painter, const QRect &rect, const QPalette &palette, bool hover) const
{
	(void)palette;
	(void)hover;
	const QRect		displayRect = QRect(QPoint(rect.x(), rect.y()), sizeHint()).adjusted(1, 1, -1, -1);
	painter->drawPixmap(displayRect, m_Pixmap);
}

//----------------------------------------------------------------------------

QSize CGraphicResetButtonView::sizeHint() const
{
	return kResetButtonSize + QSize(2, 2);
}

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------

CGraphicResourceView::CGraphicResourceView()
	: m_Type(CGraphicResourceView::ViewType::ViewType_Effect)
{

}

//----------------------------------------------------------------------------

CGraphicResourceView::CGraphicResourceView(CGraphicResourceView::ViewType type)
	: m_Type(type)
{

}

//----------------------------------------------------------------------------

CGraphicResourceView::CGraphicResourceView(CGraphicResourceView::ViewType type, const QPixmap &pixmap)
	: m_Type(type)
	, m_Pixmap(pixmap)
{

}

//----------------------------------------------------------------------------

static s32	_Align(s32 v, s32 alignment)
{
	return v - v % alignment;
}

//----------------------------------------------------------------------------

static void	_DrawCheckerboard(	QPainter &painter, const QRect &displayRect, const QRect &visibleRect,
								const QColor &baseColor, const QColor &alternateColor, s32 patternSize)
{
	PK_ASSERT(patternSize > 0);
	const s32	offsetX = displayRect.left();
	const s32	offsetY = displayRect.top();
	for (s32 py = visibleRect.top(), stopY = visibleRect.bottom(); py <= stopY; )
	{
		const s32	startY = py;
		const s32	endY = PKMin(stopY + 1, _Align(startY - offsetY + patternSize, patternSize) + offsetY);
		for (s32 px = visibleRect.left(), stopX = visibleRect.right(); px <= stopX; )
		{
			const s32		startX = px;
			const s32		endX = PKMin(stopX + 1, _Align(startX - offsetX + patternSize, patternSize) + offsetX);

			const s32		tileIdX = (px - offsetX) / patternSize;
			const s32		tileIdY = (py - offsetY) / patternSize;
			const bool		evenTile = ((tileIdX ^ tileIdY) & 1) != 0;

			const QRect		tileRect(startX, startY, endX - startX, endY - startY);
			const QColor	tileColor = evenTile ? baseColor : alternateColor;
			painter.fillRect(tileRect, tileColor);

			px = endX;
		}
		py = endY;
	}
}

void	CGraphicResourceView::paint(QPainter *painter, const QRect &rect, const QPalette &palette, bool hover) const
{
	(void)palette;

	if (m_Type == ViewType::ViewType_PathResource)
	{
		if (hover)
		{
			QPixmap pix = QPixmap(sizeHint());
			pix.fill(QColor(255, 255, 255, 128));
			const QRect		displayRect = QRect(QPoint(rect.x(), rect.y()), sizeHint());
			painter->drawPixmap(displayRect, pix);
		}

		// Display a checkerboard background
		const QColor	chkColorA = QColor(0x30, 0x30, 0x30, 0xFF);
		const QColor	chkColorB = QColor(0x40, 0x40, 0x40, 0xFF);
		const QRect		displayRect = QRect(QPoint(rect.x(), rect.y()), sizeHint()).adjusted(1, 1, -1, -1);
		const QRect		visibleRect = displayRect & rect;

		_DrawCheckerboard(*painter, displayRect, visibleRect, chkColorA, chkColorB, 5);

		painter->drawPixmap(displayRect, m_Pixmap);
	}
}

//----------------------------------------------------------------------------

QSize CGraphicResourceView::sizeHint() const
{
	if (m_Type == ViewType::ViewType_PathResource)
		return kGraphicResourceThumbSize + QSize(2, 2);

	return 0.1f/*PaintingScaleFactor*/ * QSize(1, 1);
}

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------

void	CGraphicResourceDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	if (index.data().canConvert<CGraphicResourceView>())
	{
		CGraphicResourceView item = qvariant_cast<CGraphicResourceView>(index.data());
		item.paint(painter, option.rect, option.palette, option.state & QStyle::State_MouseOver);
	}
	if (index.data().canConvert<CGraphicResetButtonView>())
	{
		CGraphicResetButtonView item = qvariant_cast<CGraphicResetButtonView>(index.data());
		item.paint(painter, option.rect, option.palette, option.state & QStyle::State_MouseOver);
	}
	else
	{
		//Remove Selected and MouseOver state for visual
		QStyleOptionViewItem	modifiedOption = option;
		modifiedOption.state &= ~QStyle::State_Selected;
		modifiedOption.state &= ~QStyle::State_MouseOver;
		modifiedOption.state &= ~QStyle::State_HasFocus;
		
		QStyledItemDelegate::paint(painter, modifiedOption, index);
	}
}

//----------------------------------------------------------------------------

QSize CGraphicResourceDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	if (index.data().canConvert<CGraphicResourceView>())
	{
		CGraphicResourceView item = qvariant_cast<CGraphicResourceView>(index.data());
		return item.sizeHint();
	}
	return QStyledItemDelegate::sizeHint(option, index);
}

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------

CGraphicalResourcesTreeItem::CGraphicalResourcesTreeItem()
	: m_ParentItem(null)
{
}

//----------------------------------------------------------------------------

CGraphicalResourcesTreeItem::CGraphicalResourcesTreeItem(const QVector<QVariant> &data, CGraphicalResourcesTreeItem *parentItem)
	: m_ItemData(data)
	, m_ParentItem(parentItem)
{
}

//----------------------------------------------------------------------------

CGraphicalResourcesTreeItem::~CGraphicalResourcesTreeItem()
{
	qDeleteAll(m_ChildItems);
}

//----------------------------------------------------------------------------

void	CGraphicalResourcesTreeItem::AppendChild(CGraphicalResourcesTreeItem *child)
{
	m_ChildItems.append(child);
}

//----------------------------------------------------------------------------

void	CGraphicalResourcesTreeItem::InsertChild(CGraphicalResourcesTreeItem *child, int index)
{
	m_ChildItems.insert(index, child);
}

//----------------------------------------------------------------------------

CGraphicalResourcesTreeItem		*CGraphicalResourcesTreeItem::Child(int row)
{
	if (row < 0 || row >= m_ChildItems.size())
		return nullptr;
	return m_ChildItems.at(row);
}

//----------------------------------------------------------------------------

int	CGraphicalResourcesTreeItem::ChildCount() const
{
	return m_ChildItems.count();
}

//----------------------------------------------------------------------------

int	CGraphicalResourcesTreeItem::ColumnCount() const
{
	return m_ItemData.count();
}

//----------------------------------------------------------------------------

QVariant	CGraphicalResourcesTreeItem::Data(int column) const
{
	//Crash on close
	if (column < 0 || column >= m_ItemData.size())
		return QVariant();
	return m_ItemData.at(column);
}

//----------------------------------------------------------------------------

int	CGraphicalResourcesTreeItem::Row() const
{
	if (m_ParentItem)
		return m_ParentItem->m_ChildItems.indexOf(const_cast<CGraphicalResourcesTreeItem*>(this)); // yuck, ugly

	return 0;
}

//----------------------------------------------------------------------------

CGraphicalResourcesTreeItem	*CGraphicalResourcesTreeItem::ParentItem()
{
	return m_ParentItem;
}

//----------------------------------------------------------------------------

void	CGraphicalResourcesTreeItem::ClearChildren()
{
	qDeleteAll(m_ChildItems);
	m_ChildItems.clear();
}

//----------------------------------------------------------------------------

void	CGraphicalResourcesTreeItem::RemoveChild(int index)
{
	delete m_ChildItems[index];
	m_ChildItems.remove(index);
}

//----------------------------------------------------------------------------

void	CGraphicalResourcesTreeItem::RemoveChildren(int from, int count)
{
	for (int i = from; i < from + count; ++i)
		delete m_ChildItems[i];
	m_ChildItems.remove(from, count);
}

//----------------------------------------------------------------------------

void	CGraphicalResourcesTreeItem::SetData(int column, const QVariant &data)
{
	if (column < 0 || column >= m_ItemData.size())
		return;
	m_ItemData[column] = data;
}

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------

CGraphicalResourcesTreeModel::CGraphicalResourcesTreeModel(QObject *parent)
	: QAbstractItemModel(parent)
	, m_RootItem(null)
{
	m_RootItem = new CGraphicalResourcesTreeItem({ tr("Layer"), tr("Path"), QVariant::fromValue(CGraphicResourceView(CGraphicResourceView::ViewType::ViewType_Effect)), tr("") });
}

//----------------------------------------------------------------------------

CGraphicalResourcesTreeModel::~CGraphicalResourcesTreeModel()
{
	if (m_RootItem)
		delete m_RootItem;
}

//----------------------------------------------------------------------------

QVariant		CGraphicalResourcesTreeModel::data(const QModelIndex &index, int role) const
{
	if (!index.isValid())
		return QVariant();

	if (role != Qt::DisplayRole)
		return QVariant();

	CGraphicalResourcesTreeItem *item = static_cast<CGraphicalResourcesTreeItem*>(index.internalPointer());

	return item->Data(index.column());
}

//----------------------------------------------------------------------------

CGraphicalResourcesTreeItem		*CGraphicalResourcesTreeModel::Item(const QModelIndex &index) const
{
	if (!index.isValid())
		return null;

	CGraphicalResourcesTreeItem *item = static_cast<CGraphicalResourcesTreeItem*>(index.internalPointer());

	return item;
}

//----------------------------------------------------------------------------

QModelIndex		CGraphicalResourcesTreeModel::Index(CGraphicalResourcesTreeItem *item) const
{
	if (item == m_RootItem)
		return QModelIndex();

	return index(item->Row(), 0, Index(item->ParentItem()));
}

//----------------------------------------------------------------------------

Qt::ItemFlags	CGraphicalResourcesTreeModel::flags(const QModelIndex &index) const
{
	if (!index.isValid())
		return Qt::NoItemFlags;

	return QAbstractItemModel::flags(index);
}

//----------------------------------------------------------------------------

QVariant		CGraphicalResourcesTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
		return m_RootItem->Data(section);

	return QVariant();
}

//----------------------------------------------------------------------------

QModelIndex		CGraphicalResourcesTreeModel::index(int row, int column, const QModelIndex &parent) const
{
	if (!hasIndex(row, column, parent))
		return QModelIndex();

	CGraphicalResourcesTreeItem *parentItem;

	if (!parent.isValid())
		parentItem = m_RootItem;
	else
		parentItem = static_cast<CGraphicalResourcesTreeItem*>(parent.internalPointer());

	CGraphicalResourcesTreeItem *childItem = parentItem->Child(row);
	if (childItem)
		return createIndex(row, column, childItem);
	return QModelIndex();
}

//----------------------------------------------------------------------------

QModelIndex		CGraphicalResourcesTreeModel::parent(const QModelIndex &index) const
{
	if (!index.isValid())
		return QModelIndex();

	CGraphicalResourcesTreeItem	*childItem = static_cast<CGraphicalResourcesTreeItem*>(index.internalPointer());
	if (childItem == m_RootItem)
		return QModelIndex();

	CGraphicalResourcesTreeItem	*parentItem = childItem->ParentItem();

	if (parentItem == m_RootItem)
		return QModelIndex();

	return createIndex(parentItem->Row(), 0, parentItem);
}

//----------------------------------------------------------------------------

int				CGraphicalResourcesTreeModel::rowCount(const QModelIndex &parent) const
{
	CGraphicalResourcesTreeItem *parentItem;
	if (parent.column() > 0)
		return 0;

	if (!parent.isValid())
		parentItem = m_RootItem;
	else
		parentItem = static_cast<CGraphicalResourcesTreeItem*>(parent.internalPointer());

	return parentItem->ChildCount();
}

//----------------------------------------------------------------------------

int				CGraphicalResourcesTreeModel::columnCount(const QModelIndex &parent) const
{
	if (parent.isValid())
		return static_cast<CGraphicalResourcesTreeItem*>(parent.internalPointer())->ColumnCount();
	return m_RootItem->ColumnCount();
}

//----------------------------------------------------------------------------

void	CGraphicalResourcesTreeModel::UpdateModel()
{
	if (m_RootItem == null)
		return;

	CPopcornFXWorld		&instance = AEGPPk::CPopcornFXWorld::Instance();
	CString				compositionName = "";

	if (!instance.GetMostRecentCompName(compositionName))
		return;

	const bool			reset = compositionName != m_CompositionName;

	m_CompositionName = compositionName;
	if (!reset)
		_UpdateModel();
	else
		_ResetModel();
}

//----------------------------------------------------------------------------

void	CGraphicalResourcesTreeModel::_UpdateModel()
{
	_UnflagUpdated(m_RootItem);

	CPopcornFXWorld							&instance = AEGPPk::CPopcornFXWorld::Instance();
	TArray<SLayerHolder*>					&layers = instance.GetLayers();
	u32										effectCount = 0;

	for (u32 i = 0; i < layers.Count(); ++i)
	{
		PK_SCOPEDLOCK(layers[i]->m_LayerLock);

		if (layers[i]->m_Deleted == true)
			continue;
		if (layers[i]->m_SpawnedEmitter.m_Desc != null)
		{
			if (layers[i]->m_CompositionName != m_CompositionName)
				continue;

			const QString				layerName(layers[i]->m_LayerName.Data());
			const QString				emitterName(layers[i]->m_SpawnedEmitter.m_Desc->m_Name.c_str());
			const CStringId				emitterUID = CStringId(layers[i]->m_SpawnedEmitter.m_Desc->m_UUID.c_str());

			if (emitterName.isEmpty())
				continue;

			// Find effect or create it
			CGraphicalResourcesTreeItem	*effect = _FindChild(m_RootItem, emitterUID.Id());

			if (effect == null)
			{
				beginInsertRows(Index(m_RootItem), effectCount, effectCount);
				effect = _CreateEffect(layerName, emitterName, emitterUID.Id());
				m_RootItem->InsertChild(effect, effectCount);
				endInsertRows();
			}
			else 
			{
				// Update effect data
				effect->SetData(0, layerName);
				effect->SetData(1, emitterName);
			}
			effect->SetUpdated(true);
			effectCount++;

			// Add properties
			TArray<SRendererProperties*>				&renderers = layers[i]->m_Scene->GetRenderers();
			THashMap<u32, CGraphicalResourcesTreeItem*>	effectLayers;

			for (u32 iRenderer = 0; iRenderer < renderers.Count(); ++iRenderer)
			{
				// Find effect layer or create it
				const QString				effectLayerName = renderers[iRenderer]->m_EffectLayerName.Data();
				const u32					effectLayerUID = renderers[iRenderer]->m_EffectLayerUID;
				CGraphicalResourcesTreeItem	*effectLayer = _FindChild(effect, effectLayerUID);
			
				if (effectLayer == null)
				{
					beginInsertRows(Index(effect), effectLayers.Count(), effectLayers.Count());
					effectLayer = _CreateEffectLayer(effectLayerName, effectLayerUID, effect);
					effect->InsertChild(effectLayer, effectLayers.Count());
					endInsertRows();
				}
				else
				{
					// Update effect layer data
					effectLayer->SetData(0, effectLayerName);
				}
				effectLayer->SetUpdated(true);

				u32*	propertyCount = effectLayers.Find(effectLayer);
				if (propertyCount == null)
					propertyCount = effectLayers.Insert(effectLayer, u32(0));

				// Find renderer property or create it
				const u32					propertyUID = renderers[iRenderer]->m_PropertyUID;
				CGraphicalResourcesTreeItem	*property = _FindChild(effectLayer, propertyUID);

				if (property == null)
				{
					beginInsertRows(Index(effectLayer), *propertyCount, *propertyCount);
					property = _CreateRenderer(renderers[iRenderer], effectLayer);
					effectLayer->InsertChild(property, *propertyCount);
					endInsertRows();
				}
				else
				{
					// Update property data
					property->SetData(0, QString(renderers[iRenderer]->m_Name.Data()));

					const QString	newPath = renderers[iRenderer]->m_Value.Data();
					if (property->Data(1) != newPath)
					{
						QPixmap	pix;
						if (!_LoadImageThumbnail(renderers[iRenderer]->m_Value, &pix))
						{
							pix = QPixmap(kGraphicResourceThumbSize);
							pix.fill(QColor(255, 0, 0, 128));

						}

						property->SetData(1, newPath);
						property->SetData(2, QVariant::fromValue(CGraphicResourceView(CGraphicResourceView::ViewType::ViewType_PathResource, pix)));
					}
					property->SetLayerID(renderers[iRenderer]->m_LayerID);
					property->SetRendererID(renderers[iRenderer]->m_RendererUID);
				}
				property->SetUpdated(true);
				*propertyCount++;
			}
		}
	}

	_RemoveOldItems(m_RootItem);
}

//----------------------------------------------------------------------------

void	CGraphicalResourcesTreeModel::_ResetModel()
{
	CPopcornFXWorld			&instance = AEGPPk::CPopcornFXWorld::Instance();
	TArray<SLayerHolder*>	&layers = instance.GetLayers();

	beginResetModel();

	if (m_RootItem->ChildCount() != 0)
		m_RootItem->ClearChildren();

	for (u32 i = 0; i < layers.Count(); ++i)
	{
		PK_SCOPEDLOCK(layers[i]->m_LayerLock);

		if (layers[i]->m_SpawnedEmitter.m_Desc != null)
		{
			if (layers[i]->m_CompositionName != m_CompositionName)
				continue;

			const QString					layerName(layers[i]->m_LayerName.Data());
			const QString					emitterName(layers[i]->m_SpawnedEmitter.m_Desc->m_Name.c_str());
			const CStringId					emitterUID = CStringId(layers[i]->m_SpawnedEmitter.m_Desc->m_UUID.c_str());

			if (emitterName.isEmpty())
				continue;

			CGraphicalResourcesTreeItem		*effect = _CreateEffect(layerName, emitterName, emitterUID.Id());

			m_RootItem->AppendChild(effect);

			TArray<SRendererProperties*>	&renderers = layers[i]->m_Scene->GetRenderers();
			for (u32 iRenderer = 0; iRenderer < renderers.Count(); ++iRenderer)
			{
				const QString				effectLayerName = renderers[iRenderer]->m_EffectLayerName.Data();
				const u32					effectLayerUID = renderers[iRenderer]->m_EffectLayerUID;
				CGraphicalResourcesTreeItem	*effectLayer = _FindChild(effect, effectLayerUID);

				if (effectLayer == null)
				{
					effectLayer = _CreateEffectLayer(effectLayerName, effectLayerUID, effect);
					effect->AppendChild(effectLayer);
				}

				CGraphicalResourcesTreeItem	*rdr = _CreateRenderer(renderers[iRenderer], effectLayer);
				effectLayer->AppendChild(rdr);
			}
		}
	}
	endResetModel();

}

//----------------------------------------------------------------------------

CGraphicalResourcesTreeItem	*CGraphicalResourcesTreeModel::_CreateEffect(const QString &layerName, const QString &emitterName, u32 uid)
{
	QVector<QVariant>	effectData;
	effectData.append(layerName);
	effectData.append(emitterName);
	effectData.append(QVariant::fromValue(CGraphicResourceView(CGraphicResourceView::ViewType::ViewType_Effect)));
	effectData.append(QString(""));

	CGraphicalResourcesTreeItem	*effect = new CGraphicalResourcesTreeItem(effectData, m_RootItem);

	effect->SetID(uid);

	return effect;
}

//----------------------------------------------------------------------------

CGraphicalResourcesTreeItem	*CGraphicalResourcesTreeModel::_CreateEffectLayer(const QString &layerName, u32 uid, CGraphicalResourcesTreeItem *effect)
{
	QVector<QVariant>	effectData;
	effectData.append(layerName);
	effectData.append(QString(""));
	effectData.append(QVariant::fromValue(CGraphicResourceView(CGraphicResourceView::ViewType::ViewType_Layer)));
	effectData.append(QString(""));

	CGraphicalResourcesTreeItem	*layer = new CGraphicalResourcesTreeItem(effectData, effect);

	layer->SetID(uid);

	return layer;
}

//----------------------------------------------------------------------------

CGraphicalResourcesTreeItem	*CGraphicalResourcesTreeModel::_CreateRenderer(SRendererProperties *renderer, CGraphicalResourcesTreeItem *layer)
{
	QPixmap	pix;

	if (!_LoadImageThumbnail(renderer->m_Value, &pix))
	{
		pix = QPixmap(kGraphicResourceThumbSize);
		pix.fill(QColor(255, 0, 0, 128));
	}

	QVector<QVariant>			rdrData;
	rdrData << QString(renderer->m_Name.Data());
	rdrData << QString(renderer->m_Value.Data());
	rdrData << QVariant::fromValue(CGraphicResourceView(CGraphicResourceView::ViewType::ViewType_PathResource, pix));
	rdrData << QVariant::fromValue(CGraphicResetButtonView());

	CGraphicalResourcesTreeItem	*rdr = new CGraphicalResourcesTreeItem(rdrData, layer);

	rdr->SetLayerID(renderer->m_LayerID);
	rdr->SetRendererID(renderer->m_RendererUID);
	rdr->SetID(renderer->m_PropertyUID);

	return rdr;
}

//----------------------------------------------------------------------------

CGraphicalResourcesTreeItem	*CGraphicalResourcesTreeModel::_FindChild(CGraphicalResourcesTreeItem *parent, u32 childId)
{
	for (int i = 0; i < parent->ChildCount(); ++i)
	{
		CGraphicalResourcesTreeItem	*item = parent->Child(i);
		if (item->GetID() == childId)
			return item;
	}
	return null;
}

//----------------------------------------------------------------------------

void	CGraphicalResourcesTreeModel::_UnflagUpdated(CGraphicalResourcesTreeItem* parent)
{

	for (int i = 0; i < parent->ChildCount(); ++i)
	{
		CGraphicalResourcesTreeItem	*item = parent->Child(i);
		item->SetUpdated(false);
		_UnflagUpdated(item);
	}
}

//----------------------------------------------------------------------------

void	CGraphicalResourcesTreeModel::_RemoveOldItems(CGraphicalResourcesTreeItem* parent)
{
	QModelIndex	parentIndex = Index(parent);

	for (int i = 0; i < parent->ChildCount(); )
	{
		CGraphicalResourcesTreeItem	*item = parent->Child(i);
		if (!item->GetUpdated())
		{
			beginRemoveRows(parentIndex, i, i);
			parent->RemoveChild(i);
			endRemoveRows();
		}
		else
		{
			_RemoveOldItems(item);
			++i;
		}
	}
}

//----------------------------------------------------------------------------

bool	CGraphicalResourcesTreeModel::_LoadImageThumbnail(const CString &path, QPixmap *outThumbnail)
{
	const float			dpr = 1.0f;// : MaxDevicePixelRatio();
	const QSize			size = kGraphicResourceThumbSize * dpr;

	CResourceManager	*resourceManager = Resource::DefaultManager();

	if (!PK_VERIFY(outThumbnail != null))
		return false;

	if (!resourceManager->FileController()->Exists(path, false))
		return false;

	PImage	resource;
	const TResourcePtr<CImage>	resourcePtr = resourceManager->Load<CImage>(path, false, SResourceLoadCtl(false, true));
	if (resourcePtr == null || resourcePtr->Empty())
		return false;
	resource = resourcePtr.operator->();

	if (resource == null)
		return false;

	PK_ASSERT(!resource->m_Frames.Empty());
	const CImageFrame	&frame = resource->m_Frames.First();
	PK_ASSERT(!frame.m_Mipmaps.Empty());
	const CImageMap		&map = frame.m_Mipmaps.First();

	CImageSurface		surface(map, resource->m_Format);
	QImage::Format		targetFormat;

	switch (surface.m_Format)
	{
	case CImage::Format_BGR8:
		targetFormat = QImage::Format_RGB888;
		break;
	case CImage::Format_BGRA8:
		targetFormat = QImage::Format_RGBA8888;
		break;
	default:
		targetFormat = QImage::Format_RGBA8888;
		if (!PK_VERIFY(surface.Convert(CImage::Format_BGRA8)))
			return false;
		break;
	}

	// Note: we do this analysis & alpha-patching before downscaling, as Qt will
	// trash the RGB colors when downscaling with an alpha set to zero, for some reason...
	if (targetFormat == QImage::Format_RGBA8888)
	{
		bool	isFullyTransparent = false;
		{
			CUbyte4			*texels = surface.m_RawBuffer->Data<CUbyte4>();
			const CUbyte4	*texelsStop = texels + surface.m_Dimensions.x() * surface.m_Dimensions.y();

			SIMD::Float4	texelORx4 = SIMD::Float4::Zero();
			texelsStop -= 4;
			while (texels <= texelsStop)
			{
				texelORx4 |= SIMD::Float4::LoadAligned16(texels);
				texels += 4;
			}
			texelsStop += 4;

			const u32	texelOR32 = texelORx4.x().AsUint() | texelORx4.y().AsUint() | texelORx4.z().AsUint() | texelORx4.w().AsUint();
			CUbyte4		texelOR = *reinterpret_cast<const CUbyte4*>(&texelOR32);
			while (texels < texelsStop)
			{
				texelOR |= *texels;
				texels++;
			}

			isFullyTransparent = (texelOR.w() == 0);
		}

		// Fix #3945: Editor propertygrid: Some cubemaps do not show-up in asset-picker preview
		// Fix #4133: Content browser: Some cubemap thumbnails do not appear
		if (isFullyTransparent ||
			(resource->m_Flags & CImage::Flag_Cubemap))
		{
			// When it's a cubemap, we're only displaying part of it, and in perhaps 99+% of cases, even if the
			// image format has alpha, we don't want to display alpha. (envmaps, cubemap backdrops)
			// And in practise there are quite a bit of cubemaps grabbed from the web that have a BGRA8 format
			// with zero alpha. Here, for preview purposes & practical reasons, force the alpha channel to 0xFF:

			CUbyte4			*texels = surface.m_RawBuffer->Data<CUbyte4>();
			const CUbyte4	*texelsStop = texels + surface.m_Dimensions.x() * surface.m_Dimensions.y();

			const SIMD::Float4	kFullAlpha = SIMD::Float4::FromConstInt<0xFF000000>();
			texelsStop -= 4;
			while (texels <= texelsStop)
			{
				(SIMD::Float4::LoadAligned16(texels) | kFullAlpha).StoreAligned16(texels);
				texels += 4;
			}
			texelsStop += 4;
			while (texels < texelsStop)
			{
				texels->w() = 0xFF;
				texels++;
			}
		}
	}

	const CUint3	&dim = surface.m_Dimensions;
	const u32		channelCount = CImage::GetFormatChannelCount(surface.m_Format);
	if (!PK_VERIFY(QImage::toPixelFormat(targetFormat).channelCount() == channelCount) ||
		!PK_VERIFY(dim.AxialProduct() * channelCount * sizeof(uchar) <= surface.m_RawBuffer->DataSizeInBytes()))
		return false;

	QImage			dstImage;
	if (PK_VERIFY(All(dim > CUint3(0))))
	{
		const u32	bytesPerLine = dim.x() * channelCount;
		QImage		srcImage = QImage(surface.m_RawBuffer->Data<uchar>(), dim.x(), dim.y(), bytesPerLine, targetFormat).rgbSwapped();

		const u64	fixedPoint = dim.y() * u64(size.height());
		const u64	fixedPointRatioSrc = (dim.x() * fixedPoint) / dim.y();
		const u64	fixedPointRatioDst = (size.width() * fixedPoint) / size.height();

		if (fixedPointRatioSrc == fixedPointRatioDst)
		{
			dstImage = srcImage.scaled(size.width(), size.height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
		}
		else if (fixedPointRatioSrc != fixedPointRatioDst)
		{
			if (dim.y() == 1)
			{
				// We have a 1D texture, probably a gradient
				dstImage = srcImage.scaled(size.width(), size.height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
			}
			else
			{
				u32	dstPadW = 0;
				u32	dstPadH = 0;
				u32	dstW = 0;
				u32	dstH = 0;

				// fixedPointRatioSrc > fixedPointRatioDst : pad along y (top & bottom)
				// fixedPointRatioSrc < fixedPointRatioDst : pad along x (left & right)

				if (fixedPointRatioSrc > fixedPointRatioDst)
				{
					dstW = size.width();	// dim.x() * size.width() / dim.x();
					dstH = PKMax(1U, dim.y() * size.height() / dim.x());
					dstPadH = (size.height() - dstH) / 2;
				}
				else
				{
					dstW = PKMax(1U, dim.x() * size.width() / dim.y());
					dstH = size.height();	// dim.y() * size.height() / dim.y();
					dstPadW = (size.width() - dstW) / 2;
				}

				// First do a smooth downscale. QPainter::drawImage() does a fast downscale.
				// We want the higher quality box-scale for proper thumbnail display:
				QImage	scaledImage = srcImage.scaled(dstW, dstH, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

				dstImage = QImage(size.width(), size.height(), srcImage.format());
				dstImage.fill(Qt::transparent);

				QPainter	p(&dstImage);
				p.setCompositionMode(QPainter::CompositionMode_Source);
				p.drawImage(QRect(dstPadW, dstPadH, dstW, dstH), scaledImage);
			}
		}
	}

	*outThumbnail = QPixmap::fromImage(dstImage);
	outThumbnail->setDevicePixelRatio(dpr);

	return true;
}

//----------------------------------------------------------------------------

__AEGP_PK_END
