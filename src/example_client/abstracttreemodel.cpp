#include "abstracttreemodel.h"

#include <memory>
#include <utility>

AbstractTreeItem::AbstractTreeItem(std::vector<QVariant> data,
                                   AbstractTreeItem*     parent_item)
    : m_item_data(std::move(data)), m_parent_item(parent_item) { }
AbstractTreeItem::~AbstractTreeItem() = default;

AbstractTreeItem* AbstractTreeItem::child(int row) {
    if (row >= 0 and row < m_children.size()) { return m_children[row].get(); }
    return nullptr;
}

int AbstractTreeItem::child_count() const {
    return m_children.size();
}

int AbstractTreeItem::column_count() const {
    return m_item_data.size();
}

QVariant AbstractTreeItem::data(int column) const {
    if (column >= 0 and column < m_item_data.size()) return m_item_data[column];
    return {};
}

static int index_of(AbstractTreeItem const*                               p,
                    std::vector<std::unique_ptr<AbstractTreeItem>> const& v) {
    for (size_t i = 0; i < v.size(); i++) {
        if (v[i].get() == p) { return i; }
    }

    return -1;
}

int AbstractTreeItem::row() const {
    if (m_parent_item) return index_of(this, m_parent_item->m_children);

    return 0;
}

AbstractTreeItem* AbstractTreeItem::parent_item() {
    return m_parent_item;
}

// =====================================

AbstractTreeModel::AbstractTreeModel(QObject* parent)
    : QAbstractItemModel(parent) {

    root_item = std::make_unique<AbstractTreeItem>(
        std::vector<QVariant> { tr("Object"), tr("Contents") });

    /*
    root_item->emplace_back(
        std::vector<QVariant> { tr("Test 1"), tr("A summary") });

    auto* p = root_item->emplace_back(
        std::vector<QVariant> { tr("Test 2"), tr("A summary 2") });

    p->emplace_back(std::vector<QVariant> { tr("Test 3"), tr("A summary 3") });
    */
}
AbstractTreeModel::~AbstractTreeModel() = default;

QVariant AbstractTreeModel::data(QModelIndex const& index, int role) const {
    if (!index.isValid()) return QVariant();

    if (role != Qt::DisplayRole) return QVariant();

    auto* item = static_cast<AbstractTreeItem*>(index.internalPointer());

    return item->data(index.column());
}
Qt::ItemFlags AbstractTreeModel::flags(QModelIndex const& index) const {
    if (!index.isValid()) return Qt::NoItemFlags;

    return QAbstractItemModel::flags(index);
}

QVariant AbstractTreeModel::headerData(int             section,
                                       Qt::Orientation orientation,
                                       int             role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
        return root_item->data(section);

    return QVariant();
}

QModelIndex
AbstractTreeModel::index(int row, int column, QModelIndex const& parent) const {

    if (!hasIndex(row, column, parent)) return QModelIndex();

    AbstractTreeItem* parent_item;

    if (!parent.isValid()) {
        parent_item = root_item.get();
    } else
        parent_item = static_cast<AbstractTreeItem*>(parent.internalPointer());

    AbstractTreeItem* child_item = parent_item->child(row);

    if (child_item) return createIndex(row, column, child_item);

    return QModelIndex();
}

QModelIndex AbstractTreeModel::parent(QModelIndex const& index) const {
    if (!index.isValid()) return QModelIndex();

    AbstractTreeItem* child_item =
        static_cast<AbstractTreeItem*>(index.internalPointer());
    AbstractTreeItem* parent_item = child_item->parent_item();

    if (parent_item == root_item.get()) return QModelIndex();

    return createIndex(parent_item->row(), 0, parent_item);
}

int AbstractTreeModel::rowCount(QModelIndex const& parent) const {
    AbstractTreeItem* parent_item;
    if (parent.column() > 0) return 0;

    if (!parent.isValid()) {
        parent_item = root_item.get();
    } else {
        parent_item = static_cast<AbstractTreeItem*>(parent.internalPointer());
    }

    return parent_item->child_count();
}

int AbstractTreeModel::columnCount(QModelIndex const& parent) const {
    if (parent.isValid()) {
        return static_cast<AbstractTreeItem*>(parent.internalPointer())
            ->column_count();
    }
    return root_item->column_count();
}
