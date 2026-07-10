#include "MagNavCsvProtocolPlugin.h"

#include <QCoreApplication>
#include <QTextStream>

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    MagNavCsvProtocolPlugin plugin;

    int frameCount = 0;
    DataFrame lastFrame;
    QObject::connect(&plugin, &IProtocolPlugin::frameParsed, [&](const DataFrame& frame) {
        ++frameCount;
        lastFrame = frame;
    });

    plugin.feedBytes("1,-123,406,39,12,6,0,299,107,3,0,0,0\r\n");
    const QVector<double> expectedFirst = {6, 0, 299, 107, 3, 0, 0, 0};
    bool firstFrameOk = frameCount == 1 && lastFrame.channels.size() == expectedFirst.size();
    for (int index = 0; firstFrameOk && index < expectedFirst.size(); ++index) {
        firstFrameOk = lastFrame.channels.at(index).value == expectedFirst.at(index);
    }
    firstFrameOk = firstFrameOk
        && lastFrame.attributes.value(QStringLiteral("offset")).toDouble() == -123.0
        && lastFrame.attributes.value(QStringLiteral("channel_mask")).toDouble() == 12.0;

    plugin.feedBytes("2,98,82,49,48,0,0,0,9,42,40,14,s\n");
    const bool malformedRejected = frameCount == 1;

    plugin.feedBytes("0,87,119,46,48,0,0,0,21,74,45,9,0\n");
    const QVector<double> expectedLast = {0, 0, 0, 21, 74, 45, 9, 0};
    bool lastFrameOk = frameCount == 2 && lastFrame.channels.size() == expectedLast.size();
    for (int index = 0; lastFrameOk && index < expectedLast.size(); ++index) {
        lastFrameOk = lastFrame.channels.at(index).value == expectedLast.at(index);
    }

    QTextStream out(stdout);
    out << "frames=" << frameCount << '\n';
    out << "first_frame=" << (firstFrameOk ? "ok" : "failed") << '\n';
    out << "malformed_frame=" << (malformedRejected ? "rejected" : "accepted") << '\n';
    out << "last_frame=" << (lastFrameOk ? "ok" : "failed") << '\n';
    return firstFrameOk && malformedRejected && lastFrameOk ? 0 : 1;
}
