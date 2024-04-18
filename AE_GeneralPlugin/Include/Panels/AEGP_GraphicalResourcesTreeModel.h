//----------------------------------------------------------------------------
// Copyright Persistant Studios, SARL. All Rights Reserved. https://www.popcornfx.com/terms-and-conditions/
//----------------------------------------------------------------------------
#pragma once

#ifndef	__AEGP_GRAPHICALRESOURCESTREEMODEL_H__
#define	__AEGP_GRAPHICALRESOURCESTREEMODEL_H__

#include "AEGP_Define.h"

#include "pk_kernel/include/kr_string_id.h"

#include <QAbstractItemModel>
#include <QModelIndex>
#include <QVariant>
#include <QVector>
#include <QStyledItemDelegate>
#include <QPixmap>

//----------------------------------------------------------------------------

__AEGP_PK_BEGIN

struct SRendererProperties;

//----------------------------------------------------------------------------

class CGraphicResetButtonView
{
public:
	explicit	CGraphicResetButtonView();

	void		paint(QPainter *painter, const QRect &rect, const QPalette &palette, bool hover) const;
	QSize		sizeHint() const;

	QPixmap		m_Pixmap;
};

//----------------------------------------------------------------------------

class CGraphicResourceView
{
public:
	enum class ViewType { ViewType_Effect, ViewType_PathResource, ViewType_Layer };

	explicit CGraphicResourceView();
	explicit CGraphicResourceView(ViewType type);
	explicit CGraphicResourceView(ViewType type, const QPixmap &pixmap);

	void paint(QPainter *painter, const QRect &rect, const QPalette &palette, bool hover) const;
	QSize sizeHint() const;

	ViewType	Type() const { return m_Type; }
private:
	ViewType m_Type;
	QPixmap m_Pixmap;
};

//----------------------------------------------------------------------------

class CGraphicResourceDelegate : public QStyledItemDelegate
{
	Q_OBJECT
public:
	using QStyledItemDelegate::QStyledItemDelegate;

	void		paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
	QSize		sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

//----------------------------------------------------------------------------

class CGraphicalResourcesTreeItem
{
public:
	explicit CGraphicalResourcesTreeItem();
	explicit CGraphicalResourcesTreeItem(const QVector<QVariant> &data, CGraphicalResourcesTreeItem *parentItem = nullptr);
	~CGraphicalResourcesTreeItem();

	void							AppendChild(CGraphicalResourcesTreeItem *child);
	void							InsertChild(CGraphicalResourcesTreeItem *child, int index);
	CGraphicalResourcesTreeItem		*Child(int row);
	int								ChildCount() const;
	int								ColumnCount() const;
	QVariant						Data(int column) const;
	int								Row() const;
	CGraphicalResourcesTreeItem		*ParentItem();
	void							ClearChildren();
	void							RemoveChild(int index);
	void							RemoveChildren(int from, int count);

	void							SetData(int column, const QVariant &data);

	u32								GetID() const { return m_UID; }
	bool							GetUpdated() { return m_Updated; }
	void							SetID(u32 id) { m_UID = id; }
	void							SetUpdated(bool updated) { m_Updated = updated; }

	CStringId						GetLayerID() const { return m_LayerUID; }
	u32								GetRendererID() const { return m_RendererUID; }
	void							SetLayerID(CStringId id) { m_LayerUID = id; }
	void							SetRendererID(u32 id) { m_RendererUID = id; }
private:
	QVector<CGraphicalResourcesTreeItem*>	m_ChildItems;
	QVector<QVariant>						m_ItemData;
	CGraphicalResourcesTreeItem				*m_ParentItem = null;

	u32										m_UID;
	bool									m_Updated;

	CStringId								m_LayerUID;
	u32										m_RendererUID;
};

//----------------------------------------------------------------------------

class CGraphicalResourcesTreeModel : public QAbstractItemModel
{
	Q_OBJECT

public:
	explicit CGraphicalResourcesTreeModel(QObject *parent = nullptr);
	~CGraphicalResourcesTreeModel();

	QVariant		data(const QModelIndex &index, int role) const override;
	Qt::ItemFlags	flags(const QModelIndex &index) const override;
	QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
	QModelIndex		index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
	QModelIndex		parent(const QModelIndex &index) const override;
	int				rowCount(const QModelIndex &parent = QModelIndex()) const override;
	int				columnCount(const QModelIndex &parent = QModelIndex()) const override;

	CGraphicalResourcesTreeItem		*Item(const QModelIndex &index) const;
	QModelIndex						Index(CGraphicalResourcesTreeItem *item) const;

	void			UpdateModel();
private:
	void			_UpdateModel();
	void			_ResetModel();

	CGraphicalResourcesTreeItem	*_CreateEffect(const QString &layerName, const QString &emitterName, u32 uid);
	CGraphicalResourcesTreeItem	*_CreateEffectLayer(const QString &layerName, u32 uid, CGraphicalResourcesTreeItem *effect);
	CGraphicalResourcesTreeItem	*_CreateRenderer(SRendererProperties *renderer, CGraphicalResourcesTreeItem *layer);

	CGraphicalResourcesTreeItem	*_FindChild(CGraphicalResourcesTreeItem *parent, u32 childId);
	void						_UnflagUpdated(CGraphicalResourcesTreeItem *parent);
	void						_RemoveOldItems(CGraphicalResourcesTreeItem *parent);

	bool						_LoadImageThumbnail(const CString &path, QPixmap *outThumbnail);

	CGraphicalResourcesTreeItem		*m_RootItem = null;
	CString							m_CompositionName;
};

//----------------------------------------------------------------------------

__AEGP_PK_END

Q_DECLARE_METATYPE(AEGPPk::CGraphicResetButtonView)
Q_DECLARE_METATYPE(AEGPPk::CGraphicResourceView)
Q_DECLARE_METATYPE(AEGPPk::CGraphicalResourcesTreeItem)

#endif //!__AEGP_GRAPHICALRESOURCESTREEMODEL_H__
