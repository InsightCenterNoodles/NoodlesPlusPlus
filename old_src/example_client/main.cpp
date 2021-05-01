#include "example_client.h"

#include <QApplication>
#include <QStyle>
#include <QStyleFactory>

int main(int argc, char* argv[]) {
    QApplication a(argc, argv);

    a.setStyle(QStyleFactory::create("Fusion"));

    ExampleClient w;
    w.show();
    return a.exec();
}
