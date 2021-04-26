#ifndef ABSTRACTTREEMODEL_H
#define ABSTRACTTREEMODEL_H

#include <QAbstractItemModel>
#include <QModelIndex>
#include <QVariant>

#include <vector>

class AbstractTreeItem {
    using GFXObjectItemPtr = std::unique_ptr<AbstractTreeItem>;

    std::vector<GFXObjectItemPtr> m_children;
    std::vector<QVariant>         m_item_data;
    AbstractTreeItem*             m_parent_item;

public:
    explicit AbstractTreeItem(std::vector<QVariant> data,
                              AbstractTreeItem*     parent_item = nullptr);
    ~AbstractTreeItem();

    AbstractTreeItem* emplace_back(std::vector<QVariant> data) {
        return m_children
            .emplace_back(
                std::make_unique<AbstractTreeItem>(std::move(data), this))
            .get();
    }

    AbstractTreeItem* child(int row);
    int               child_count() const;
    int               column_count() const;
    QVariant          data(int column) const;
    int               row() const;
    AbstractTreeItem* parent_item();
};

class AbstractTreeModel : public QAbstractItemModel {
    std::unique_ptr<AbstractTreeItem> root_item;

public:
    explicit AbstractTreeModel(QObject* parent = nullptr);
    ~AbstractTreeModel();


    QVariant      data(QModelIndex const& index, int role) const override;
    Qt::ItemFlags flags(QModelIndex const& index) const override;

    QVariant headerData(int             section,
                        Qt::Orientation orientation,
                        int             role = Qt::DisplayRole) const override;

    QModelIndex index(int                row,
                      int                column,
                      const QModelIndex& parent = QModelIndex()) const override;

    QModelIndex parent(QModelIndex const& index) const override;
    int rowCount(QModelIndex const& parent = QModelIndex()) const override;
    int columnCount(QModelIndex const& parent = QModelIndex()) const override;
};

#endif // ABSTRACTTREEMODEL_H
