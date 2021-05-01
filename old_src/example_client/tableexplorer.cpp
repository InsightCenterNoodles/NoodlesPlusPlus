#include "tableexplorer.h"
#include "ui_tableexplorer.h"

TableExplorer::TableExplorer(QWidget* parent)
    : QMainWindow(parent), ui(new Ui::TableExplorer) {
    ui->setupUi(this);
}

TableExplorer::~TableExplorer() {
    delete ui;
}
