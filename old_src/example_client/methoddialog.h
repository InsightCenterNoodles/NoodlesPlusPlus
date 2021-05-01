#ifndef METHODDIALOG_H
#define METHODDIALOG_H

#include <client_interface.h>

#include <QDialog>
#include <QPointer>

class ExMethod;
class ExampleClient;

namespace Ui {
class MethodDialog;
}

class StringTranslator : public nooc::PendingMethodReply {
    Q_OBJECT

    std::shared_ptr<ExMethod> m_method;

public:
    StringTranslator(nooc::MethodDelegate*,
                     nooc::MethodContext,
                     std::shared_ptr<ExMethod>);

public slots:
    void interpret() override;

signals:
    void recv(QString);
};

class MethodDialog : public QDialog {
    Q_OBJECT

    using CTX = std::variant<nooc::DocumentDelegatePtr,
                             nooc::ObjectDelegatePtr,
                             nooc::TableDelegatePtr>;

    Ui::MethodDialog* ui;

    CTX                       m_ctx;
    std::shared_ptr<ExMethod> m_method;

    QPointer<ExampleClient> m_host;

public:
    MethodDialog(CTX c, std::shared_ptr<ExMethod>, ExampleClient* parent);
    ~MethodDialog();

private slots:
    void do_invoke();
    void do_cancel();

    void check_cell(int row, int column);
};

#endif // METHODDIALOG_H
