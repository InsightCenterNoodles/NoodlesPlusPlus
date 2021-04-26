#ifndef TABLEEXPLORER_H
#define TABLEEXPLORER_H

#include <QMainWindow>

namespace Ui {
class TableExplorer;
}

class TableExplorer : public QMainWindow {
    Q_OBJECT

public:
    explicit TableExplorer(QWidget* parent = nullptr);
    ~TableExplorer();

private:
    Ui::TableExplorer* ui;
};

#endif // TABLEEXPLORER_H
