#include "example_client.h"
#include "ui_example_client.h"

//#include "../shared/interface_tools.h"
#include "delegates.h"

#include "abstracttreemodel.h"
#include "attachedmethodlistmodel.h"
#include "componentlistmodel.h"
#include "methoddialog.h"

#include "client_interface.h"

#include <QDataWidgetMapper>
#include <QDebug>
#include <QEntity>
#include <QHostInfo>
#include <QLineEdit>
#include <QMessageBox>
#include <QSettings>
#include <QToolButton>
#include <QUrl>
#include <QValidator>

#include <QCamera>
#include <Qt3DExtras/QFirstPersonCameraController>
#include <Qt3DExtras/QOrbitCameraController>
#include <Qt3DExtras/qforwardrenderer.h>
#include <Qt3DExtras/qt3dwindow.h>

// hack
#include <Qt3DCore/QTransform>
#include <Qt3DExtras/QPhongMaterial>
#include <Qt3DExtras/QTorusGeometry>
#include <Qt3DExtras/QTorusMesh>

static auto APPLICATION_NAME         = QStringLiteral("NoodlesExampleClient");
static auto SETTING_GEOMETRY         = QStringLiteral("geometry");
static auto SETTING_WINDOWSIZE       = QStringLiteral("windowState");
static auto SETTING_PREVIOUS_SERVERS = QStringLiteral("previousServers");
static auto SETTING_PREVIOUS_NAME    = QStringLiteral("previousName");


class URLValidator : public QValidator {
public:
    URLValidator(QObject* p) : QValidator(p) { }


    // QValidator interface
public:
    State validate(QString& str, int& /*pos*/) const override {
        QUrl url(str);

        if (str.isEmpty()) { return State::Intermediate; }

        if (url.isValid()) { return State::Acceptable; }

        return State::Invalid;
    }
};


static QDataWidgetMapper* init_view(QTableView*                          v,
                                    std::shared_ptr<ComponentListModel>& model,
                                    QVector<int> const& col_hide) {
    v->setAlternatingRowColors(true);
    v->setSelectionBehavior(QAbstractItemView::SelectRows);
    v->horizontalHeader()->setStretchLastSection(true);

    v->setModel(model.get());

    for (auto i : col_hide) {
        v->hideColumn(i);
    }

    auto* mapper = new QDataWidgetMapper(v);

    mapper->setModel(model.get());

    QObject::connect(v->selectionModel(),
                     &QItemSelectionModel::currentRowChanged,
                     mapper,
                     &QDataWidgetMapper::setCurrentModelIndex);

    return mapper;
}

ExampleClient::ExampleClient(QWidget* parent)
    : QMainWindow(parent), ui(new Ui::ExampleClient) {
    ui->setupUi(this);

    // set up 3d
    {
        Qt3DExtras::Qt3DWindow* view = new Qt3DExtras::Qt3DWindow();
        view->defaultFrameGraph()->setClearColor(QColor(200, 200, 200));
        QWidget* container = QWidget::createWindowContainer(view);

        ui->viewHolder->layout()->addWidget(container);

        m_root_entity = std::make_unique<Qt3DCore::QEntity>();

        view->setRootEntity(m_root_entity.get());

        auto* camera = view->camera();

        // camera->lens()->setPerspectiveProjection(45, 1, .1f, 1000.f);
        camera->setPosition(QVector3D(0, 0, 10));
        camera->setUpVector(QVector3D(0, 1, 0));
        camera->setViewCenter(QVector3D(0, 0, 0));

        auto* controller =
            new Qt3DExtras::QOrbitCameraController(m_root_entity.get());

        // auto* controller =
        //    new Qt3DExtras::QFirstPersonCameraController(m_root_entity.get());

        controller->setCamera(camera);

        if (0) {
            auto m_torus = new Qt3DExtras::QTorusMesh();
            m_torus->setRadius(1.0f);
            m_torus->setMinorRadius(0.4f);
            m_torus->setRings(100);
            m_torus->setSlices(20);

            Qt3DCore::QTransform* torusTransform = new Qt3DCore::QTransform();
            torusTransform->setScale(2.0f);
            torusTransform->setRotation(QQuaternion::fromAxisAndAngle(
                QVector3D(0.0f, 1.0f, 0.0f), 25.0f));
            torusTransform->setTranslation(QVector3D(5.0f, 4.0f, 0.0f));

            Qt3DExtras::QPhongMaterial* torusMaterial =
                new Qt3DExtras::QPhongMaterial();
            torusMaterial->setDiffuse(QColor(QRgb(0xbeb32b)));

            auto m_torusEntity = new Qt3DCore::QEntity(m_root_entity.get());
            m_torusEntity->addComponent(m_torus);
            m_torusEntity->addComponent(torusMaterial);
            m_torusEntity->addComponent(torusTransform);
        }
    }

    ui->stackedWidget->setCurrentIndex(0);
    ui->connectionGroup->setCurrentIndex(0);

    // set up models

    m_method_list  = std::make_shared<ComponentListModel>(ExMethod::header());
    m_signal_list  = std::make_shared<ComponentListModel>(ExSignal::header());
    m_table_list   = std::make_shared<ComponentListModel>(ExTable::header());
    m_buffer_list  = std::make_shared<ComponentListModel>(ExBuffer::header());
    m_texture_list = std::make_shared<ComponentListModel>(ExTexture::header());
    m_material_list =
        std::make_shared<ComponentListModel>(ExMaterial::header());
    m_light_list = std::make_shared<ComponentListModel>(ExLight::header());
    m_mesh_list  = std::make_shared<ComponentListModel>(ExMesh::header());


    m_current_connection = std::make_unique<nooc::ClientConnection>();


    // set up connection
    QSettings settings(APPLICATION_NAME, APPLICATION_NAME);
    restoreGeometry(settings.value(SETTING_GEOMETRY).toByteArray());
    restoreState(settings.value(SETTING_WINDOWSIZE).toByteArray());

    {
        auto prev_server_list =
            settings.value(SETTING_PREVIOUS_SERVERS).toStringList();

        if (prev_server_list.size()) {
            ui->connectionBox->addItems(prev_server_list);
            ui->connectionBox->setCurrentIndex(0);
        } else {
            ui->connectionBox->addItem("ws://localhost:50000");
        }

        auto prev_user =
            settings.value(SETTING_PREVIOUS_NAME, QHostInfo::localHostName())
                .toString();

        if (prev_user.size()) { ui->clientNameEdit->setText(prev_user); }
    }

    ui->connectionBox->setValidator(new URLValidator(this));

    connect(ui->connectionBox->lineEdit(),
            &QLineEdit::returnPressed,
            this,
            &ExampleClient::start_connect_to_server);

    // init tabs

    {

        m_document_methods = new AttachedMethodListModel(this);

        ui->documentMethodView->setModel(m_document_methods);

        connect(ui->documentMethodView,
                &QListView::doubleClicked,
                this,
                &ExampleClient::handle_doc_methods_clicked);
    }

    {
        auto* mapper = init_view(ui->methodsView, m_method_list, { 0, 3, 4 });

        mapper->addMapping(ui->methodNameLabel, 1, "text");
        mapper->addMapping(ui->methodDescBox, 2);
    }

    { auto* mapper = init_view(ui->signalsView, m_signal_list, { 0, 2, 3 }); }

    { auto* mapper = init_view(ui->buffersView, m_buffer_list, { 0 }); }

    { auto* mapper = init_view(ui->texturesView, m_texture_list, { 0 }); }

    { auto* mapper = init_view(ui->materialsView, m_material_list, { 0 }); }

    { auto* mapper = init_view(ui->lightsView, m_light_list, { 0 }); }

    { auto* mapper = init_view(ui->meshesView, m_mesh_list, { 0 }); }

    { auto* mapper = init_view(ui->tableView, m_table_list, { 0 }); }

    // set up object tree

    {
        m_fake_tree_root =
            new QTreeWidgetItem(ui->objectTreeWidget->invisibleRootItem());
        m_fake_tree_root->setText(0, "Object Tree");

        m_object_methods = new AttachedMethodListModel(this);

        ui->objAttMethodsList->setModel(m_object_methods);

        connect(ui->objAttMethodsList,
                &QListView::doubleClicked,
                this,
                &ExampleClient::handle_obj_methods_clicked);

        connect(ui->objectTreeWidget,
                &QTreeWidget::currentItemChanged,
                this,
                &ExampleClient::current_tree_item_changed);
    }

    connect(ui->connectServer,
            &QPushButton::clicked,
            this,
            &ExampleClient::start_connect_to_server);
}

ExampleClient::~ExampleClient() { }

void ExampleClient::closeEvent(QCloseEvent* event) {
    QSettings settings(APPLICATION_NAME, APPLICATION_NAME);
    settings.setValue(SETTING_GEOMETRY, saveGeometry());
    settings.setValue(SETTING_WINDOWSIZE, saveState());

    {
        auto&       cbm = *(ui->connectionBox->model());
        QStringList list;
        for (int i = 0; i < cbm.rowCount(); i++) {
            list << cbm.data(cbm.index(i, 0)).toString();
        }

        // put the last used on top
        if (list.size() > 1 and ui->connectionBox->currentIndex() > 0) {
            std::swap(list[0], list[ui->connectionBox->currentIndex()]);
        }

        settings.setValue(SETTING_PREVIOUS_SERVERS, list);
    }

    settings.setValue(SETTING_PREVIOUS_NAME, ui->clientNameEdit->text());

    QMainWindow::closeEvent(event);
}

void ExampleClient::start_connect_to_server() {
    ui->connectionGroup->setCurrentIndex(1);

    QUrl url(ui->connectionBox->currentText());

    if (!url.isValid()) return;

    qDebug() << "Starting connection to" << url;

    nooc::ClientDelegates delegates;

    delegates.client_name = ui->clientNameEdit->text();

    delegates.tex_maker = [this](noo::TextureID           id,
                                 nooc::TextureData const& md) {
        return std::make_shared<ExTexture>(id, md, m_texture_list);
    };
    delegates.buffer_maker = [this](noo::BufferID           id,
                                    nooc::BufferData const& md) {
        return std::make_shared<ExBuffer>(
            id, md, m_buffer_list, m_root_entity.get());
    };
    delegates.table_maker = [this](noo::TableID id, nooc::TableData const& md) {
        return std::make_shared<ExTable>(id, md, m_table_list);
    };
    delegates.light_maker = [this](noo::LightID id, nooc::LightData const& md) {
        return std::make_shared<ExLight>(
            id, md, m_light_list, m_root_entity.get());
    };
    delegates.mat_maker = [this](noo::MaterialID           id,
                                 nooc::MaterialData const& md) {
        return std::make_shared<ExMaterial>(
            id, md, m_material_list, m_root_entity.get());
    };
    delegates.mesh_maker = [this](noo::MeshID id, nooc::MeshData const& md) {
        return std::make_shared<ExMesh>(
            id, md, m_mesh_list, m_root_entity.get());
    };
    delegates.object_maker = [this](noo::ObjectID                 id,
                                    nooc::ObjectUpdateData const& md) {
        return std::make_shared<ExObject>(
            id, md, m_fake_tree_root, m_root_entity.get());
    };
    delegates.sig_maker = [this](noo::SignalID id, nooc::SignalData const& md) {
        return std::make_shared<ExSignal>(id, md, m_signal_list);
    };
    delegates.method_maker = [this](noo::MethodID           id,
                                    nooc::MethodData const& md) {
        return std::make_shared<ExMethod>(id, md, m_method_list);
    };
    delegates.doc_maker = [this]() {
        auto p = std::make_shared<ExDoc>();

        m_current_doc = p;

        connect(p.get(),
                &ExDoc::updated,
                this,
                &ExampleClient::handle_document_updated);

        return p;
    };


    m_current_connection->open(url, std::move(delegates));

    connect(m_current_connection.get(),
            &nooc::ClientConnection::connected,
            [this]() { ui->stackedWidget->setCurrentIndex(1); });

    //    connect(m_current_connection,
    //            &nooc::ClientConnection::disconnected,
    //            [this]() { ui->stackedWidget->setCurrentIndex(0); });

    connect(m_current_connection.get(),
            &nooc::ClientConnection::socket_error,
            [this](QString err) {
                QMessageBox::critical(this,
                                      "Connection Error",
                                      "A connection error was encountered: " +
                                          err);
            });
}

void ExampleClient::handle_document_updated() {
    ExDoc* d = qobject_cast<ExDoc*>(sender());

    if (!d) return;

    m_document_methods->set(d->attached_methods().list());
}

void ExampleClient::handle_doc_methods_clicked(QModelIndex const& index) {
    auto method = m_document_methods->get_method_row(index.row());

    if (!method) return;

    auto* md = new MethodDialog(m_current_doc, method, this);

    md->show();
    md->raise();
    md->activateWindow();
}

std::shared_ptr<ExObject> item_to_object(nooc::ClientConnection& c,
                                         QTreeWidgetItem* current_object) {
    if (!current_object) return nullptr;

    bool ok          = false;
    auto object_slot = current_object->text(1).toInt(&ok);
    auto object_gen  = current_object->text(2).toInt(&ok);

    if (!ok) return nullptr;

    auto object = c.get(noo::ObjectID(object_slot, object_gen));

    if (!object) return nullptr;

    return std::dynamic_pointer_cast<ExObject>(object);
}

void ExampleClient::handle_obj_methods_clicked(QModelIndex const& index) {

    auto object = item_to_object(*m_current_connection, m_current_object);
    auto method = m_object_methods->get_method_row(index.row());

    if (!method or !object) return;

    auto* md = new MethodDialog(object, method, this);

    md->show();
    md->raise();
    md->activateWindow();
}

void ExampleClient::current_tree_item_changed(QTreeWidgetItem* c,
                                              QTreeWidgetItem* /*p*/) {
    qDebug() << Q_FUNC_INFO << c;
    m_current_object = c;

    if (!m_current_object) {
        m_object_methods->clear();
        return;
    }

    auto object = item_to_object(*m_current_connection, m_current_object);

    if (!object) {
        m_object_methods->clear();
        return;
    }

    m_object_methods->set(object->attached_methods().list());
}

void ExampleClient::method_reply(QString text) {
    // QMessageBox::information(this, "Method Reply", text);
}

void ExampleClient::method_error(QString text) {
    // QMessageBox::warning(this, "Method Reply", text);
}
