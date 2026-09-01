#include "result_receiver_window.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    ResultReceiverWindow window;
    window.show();
    return application.exec();
}
