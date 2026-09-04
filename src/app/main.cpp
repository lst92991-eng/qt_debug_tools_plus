#include "app/AgvCanFdTest.h"
#include "app/AppContext.h"
#include "app/MainWindow.h"

#include "core/DebugCore.h"

#include <QApplication>
#include <QCoreApplication>
#include <QTextStream>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    AppContext context;

    if (QCoreApplication::arguments().contains(QStringLiteral("--distance-test-300"))) {
        return runAgvCanFdTest(app, context, true);
    }

    if (QCoreApplication::arguments().contains(QStringLiteral("--canfd-link-test"))) {
        return runAgvCanFdTest(app, context, false);
    }

    if (QCoreApplication::arguments().contains(QStringLiteral("--smoke-test"))) {
        DebugCore* core = context.debugCore();
        context.scanPlugins();

        QTextStream out(stdout);
        out << "pluginRoot=" << context.pluginRoot() << '\n';
        out << "physical=" << core->pluginManager()->physicalPlugins().size() << '\n';
        out << "protocol=" << core->pluginManager()->protocolPlugins().size() << '\n';
        out << "visual=" << core->pluginManager()->visualPlugins().size() << '\n';
        out << "control=" << core->pluginManager()->controlPlugins().size() << '\n';

        const bool ok = !core->pluginManager()->physicalPlugins().isEmpty()
            && !core->pluginManager()->protocolPlugins().isEmpty()
            && !core->pluginManager()->visualPlugins().isEmpty()
            && !core->pluginManager()->controlPlugins().isEmpty();
        return ok ? 0 : 2;
    }

    MainWindow window(context);
    window.show();

    return app.exec();
}
