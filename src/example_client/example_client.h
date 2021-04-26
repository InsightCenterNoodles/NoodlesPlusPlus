#ifndef DACITELINK_H
#define DACITELINK_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class ExampleClient;
}
QT_END_NAMESPACE

namespace Qt3DCore {
class QEntity;
}

namespace nooc {
class ClientConnection;
}

class QTreeWidgetItem;

class ComponentListModel;
class AttachedMethodListModel;
class ExDoc;

class ExampleClient : public QMainWindow {
    Q_OBJECT

    std::unique_ptr<Ui::ExampleClient> ui;
    QTreeWidgetItem*                   m_fake_tree_root = nullptr;

    std::unique_ptr<Qt3DCore::QEntity> m_root_entity;

    AttachedMethodListModel* m_document_methods;

    AttachedMethodListModel* m_object_methods;

    std::shared_ptr<ComponentListModel> m_method_list;
    std::shared_ptr<ComponentListModel> m_signal_list;
    std::shared_ptr<ComponentListModel> m_table_list;
    std::shared_ptr<ComponentListModel> m_buffer_list;
    std::shared_ptr<ComponentListModel> m_texture_list;
    std::shared_ptr<ComponentListModel> m_material_list;
    std::shared_ptr<ComponentListModel> m_light_list;
    std::shared_ptr<ComponentListModel> m_mesh_list;

    std::shared_ptr<ExDoc> m_current_doc;

    std::unique_ptr<nooc::ClientConnection> m_current_connection;

    // transients
    QTreeWidgetItem* m_current_object = nullptr;

    void closeEvent(QCloseEvent* event) override;

public:
    ExampleClient(QWidget* parent = nullptr);
    ~ExampleClient();

private slots:
    void start_connect_to_server();

    void handle_document_updated();
    void handle_doc_methods_clicked(QModelIndex const&);
    void handle_obj_methods_clicked(QModelIndex const&);

    void current_tree_item_changed(QTreeWidgetItem*, QTreeWidgetItem*);

public slots:
    void method_reply(QString);
    void method_error(QString);
};
#endif // DACITELINK_H
