#include "MagNavPanel.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QPixmap>
#include <QTimer>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    const QString output = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QStringLiteral("mag_nav_viewer.png");

    MagNavPanel panel;
    panel.resize(980, 660);
    panel.show();

    QTimer::singleShot(100, [&panel, output]() {
        QPixmap pixmap(panel.size());
        panel.render(&pixmap);
        QDir().mkpath(QFileInfo(output).absolutePath());
        pixmap.save(output);
        QApplication::quit();
    });

    return app.exec();
}
