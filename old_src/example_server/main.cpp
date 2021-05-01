#include "plotty.h"

#include <QCoreApplication>


int main(int argc, char* argv[]) {
    auto app = QCoreApplication(argc, argv);

    Plotty plotty(50000);

    return app.exec();
}
