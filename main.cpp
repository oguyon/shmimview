#include "shmimview.h"
#include <QApplication>
#include <ImageStruct.h>

char imname[200];

int main(int argc, char *argv[])
{
    printf("Entering main\n");

    QApplication app(argc, argv);
    shmimview w;

    w.show();

    return app.exec();
}
