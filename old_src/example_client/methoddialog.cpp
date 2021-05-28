#include "methoddialog.h"
#include "ui_methoddialog.h"

#include "../shared/variant_tools.h"
#include "delegates.h"
#include "example_client.h"

#include <QDataWidgetMapper>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>


StringTranslator::StringTranslator(nooc::MethodDelegate*     d,
                                   nooc::MethodContext       c,
                                   std::shared_ptr<ExMethod> m)
    : nooc::PendingMethodReply(d, c), m_method(m) { }

void StringTranslator::interpret() {
    emit recv(QString::fromStdString(m_var.dump_string()));
}

// Forward
static noo::AnyVar from_json(QJsonValue const& s);

static noo::AnyVar from_json(QJsonArray const& arr) {
    // TODO: we can do better
    bool all_reals = std::all_of(
        arr.begin(), arr.end(), [](auto const& v) { return v.isDouble(); });

    if (all_reals) {
        std::vector<double> reals;
        reals.reserve(arr.size());

        for (auto const& v : arr) {
            reals.push_back(v.toDouble());
        }

        return noo::AnyVar(std::move(reals));
    }

    noo::AnyVarList ret;

    ret.reserve(arr.size());

    for (auto const& v : arr) {
        ret.push_back(from_json(v));
    }

    return std::move(ret);
}

static noo::AnyVar from_json(QJsonObject const& obj) {
    noo::AnyVarMap ret;

    for (auto iter = obj.begin(); iter != obj.end(); ++iter) {
        ret.try_emplace(iter.key().toStdString(), from_json(iter.value()));
    }

    return ret;
}

static noo::AnyVar from_json(QJsonValue const& s) {
    switch (s.type()) {
    case QJsonValue::Null: return std::monostate();
    case QJsonValue::Bool: return (int64_t)s.toBool();
    case QJsonValue::Double: return s.toDouble();
    case QJsonValue::String: return s.toString().toStdString();
    case QJsonValue::Array: return from_json(s.toArray());
    case QJsonValue::Object: return from_json(s.toObject());
    case QJsonValue::Undefined: return std::monostate();
    }

    return std::monostate();
}

static std::variant<noo::AnyVar, QString> from_raw_string(QString const& s) {

    auto ls = QString("[ %1 ]").arg(s);

    // to save us effort, we are going to hijack the document reader

    QJsonParseError error;

    auto doc = QJsonDocument::fromJson(ls.toLocal8Bit(), &error);

    if (error.error == QJsonParseError::NoError) {
        return from_json(doc.array().at(0));
    }

    return error.errorString();
}

// =============================================================================

MethodDialog::MethodDialog(CTX                       c,
                           std::shared_ptr<ExMethod> m,
                           ExampleClient*            parent)
    : QDialog(parent), ui(new Ui::MethodDialog) {
    qDebug() << Q_FUNC_INFO;

    ui->setupUi(this);

    m_host = parent;

    // note that we might have a race condition here...
    // someone could delete the method id and replace it with something else,
    // and we would accidentally call that
    // TODO: we need generation slots again

    m_ctx    = c;
    m_method = m;

    // methods are immutable so set up everything

    ui->methodDesc->setPlainText(m_method->documentation());

    QString context_name = VMATCH(
        c,
        VCASE(nooc::DocumentDelegatePtr const&) { return QString("Document"); },
        VCASE(nooc::ObjectDelegatePtr const& ptr) {
            return ptr->id().to_qstring();
        },
        VCASE(nooc::TableDelegatePtr const& ptr) {
            return ptr->id().to_qstring();
        });

    ui->contextLabel->setText(context_name);

    ui->methodNameLabel->setText(m->get_name());

    ui->methodArgEditor->verticalHeader()->setSectionResizeMode(
        QHeaderView::Stretch);

    ui->methodArgEditor->setRowCount(m_method->argument_names().size());

    auto arg_name_list = m_method->argument_names();
    auto arg_doc_list  = m_method->argument_details();

    Q_ASSERT(arg_doc_list.size() == arg_doc_list.size());

    for (int row = 0; row < arg_name_list.size(); row++) {
        auto* new_item_label = new QTableWidgetItem(arg_name_list.value(row));
        auto* new_item_desc  = new QTableWidgetItem(arg_doc_list.value(row));
        auto* new_item_well  = new QTableWidgetItem("null");

        new_item_well->setFlags(new_item_well->flags() |
                                Qt::ItemFlag::ItemIsEnabled);

        ui->methodArgEditor->setItem(row, 0, new_item_label);
        ui->methodArgEditor->setItem(row, 1, new_item_desc);
        ui->methodArgEditor->setItem(row, 2, new_item_well);

        connect(ui->methodArgEditor,
                &QTableWidget::cellChanged,
                this,
                &MethodDialog::check_cell);

        check_cell(row, 2);
    }

    auto* mapper = new QDataWidgetMapper(this);

    mapper->setModel(ui->methodArgEditor->model());

    mapper->addMapping(ui->methodArgDesc, 1);
    mapper->addMapping(ui->currentArgumentEditor, 2);

    if (ui->methodArgEditor->model()->rowCount() > 0) { mapper->toFirst(); }

    {
        auto adv = QFontMetricsF(ui->currentArgumentEditor->font())
                       .horizontalAdvance(' ');

        ui->currentArgumentEditor->setTabStopDistance(adv * 4);
    }

    connect(ui->updateButton,
            &QPushButton::clicked,
            mapper,
            &QDataWidgetMapper::submit);

    connect(ui->methodArgEditor->selectionModel(),
            &QItemSelectionModel::currentRowChanged,
            mapper,
            &QDataWidgetMapper::setCurrentModelIndex);


    connect(ui->invokeButton,
            &QPushButton::clicked,
            this,
            &MethodDialog::do_invoke);

    connect(ui->cancelButton,
            &QPushButton::clicked,
            this,
            &MethodDialog::do_cancel);


    connect(this, &QDialog::accepted, this, &QDialog::deleteLater);

    connect(this, &QDialog::rejected, this, &QDialog::deleteLater);
}

MethodDialog::~MethodDialog() {
    delete ui;
    qDebug() << Q_FUNC_INFO;
}

void MethodDialog::do_invoke() {

    nooc::AttachedMethodList const& mlist = std::visit(
        [](auto const& ptr) { return ptr->attached_methods(); }, m_ctx);

    auto const num_rows = ui->methodArgEditor->model()->rowCount();

    noo::AnyVarList arguments;

    for (auto row_i = 0; row_i < num_rows; row_i++) {
        auto* p = ui->methodArgEditor->item(row_i, 2);
        if (!p) continue;

        auto parsed_any = from_raw_string(p->data(Qt::DisplayRole).toString());

        VMATCH(
            parsed_any,
            VCASE(noo::AnyVar & a) { arguments.push_back(std::move(a)); },
            VCASE(QString) { arguments.push_back({}); });
    }

    auto* reply =
        mlist.new_call_by_delegate<StringTranslator>(m_method, m_method);

    if (!reply) return reject();

    connect(
        reply, &StringTranslator::recv, m_host, &ExampleClient::method_reply);

    connect(reply,
            &StringTranslator::recv_fail,
            m_host,
            &ExampleClient::method_error);

    reply->call_direct(std::move(arguments));

    accept();
}

void MethodDialog::do_cancel() {
    reject();
}

void MethodDialog::check_cell(int row, int column) {
    qDebug() << Q_FUNC_INFO << row << column;

    if (column != 2) return;

    auto* p = ui->methodArgEditor->item(row, column);

    if (!p) return;

    auto c = from_raw_string(p->data(Qt::DisplayRole).toString());

    VMATCH(
        c,
        VCASE(noo::AnyVar const&) {
            p->setBackground(QColor(0, 100, 0));
            p->setToolTip({});
        },
        VCASE(QString err) {
            p->setBackground(QColor(100, 0, 0));
            p->setToolTip(err);
        });
}
