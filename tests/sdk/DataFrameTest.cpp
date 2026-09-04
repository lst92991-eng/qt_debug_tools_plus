#include "sdk/DataFrame.h"

#include <cmath>
#include <iostream>

namespace {

int fail(const char* message)
{
    std::cerr << "FAILED: " << message << '\n';
    return 1;
}

} // namespace

int main()
{
    registerMcuDebugMetaTypes();

    ChannelSample emptySample;
    if (emptySample.index != 0) {
        return fail("default channel index must be zero");
    }
    if (!std::isnan(emptySample.value)) {
        return fail("default channel value must be NaN");
    }

    DataFrame frame;
    if (frame.direction != FrameDirection::Receive) {
        return fail("default direction must be Receive");
    }

    frame.timestamp_us = currentTimestampMicros();
    frame.rawPayload = QByteArray::fromHex("0102A0");
    frame.channels.append(ChannelSample{1, 12.5, QStringLiteral("voltage"),
                                        QStringLiteral("V")});
    frame.attributes.insert(QStringLiteral("source"), QStringLiteral("day02-test"));

    if (frame.timestamp_us <= 0) {
        return fail("timestamp must be positive");
    }
    if (frame.rawPayload.toHex().toUpper() != QByteArray("0102A0")) {
        return fail("raw payload changed unexpectedly");
    }
    if (frame.channels.size() != 1 || frame.channels.first().value != 12.5) {
        return fail("channel sample was not stored");
    }
    if (frame.attributes.value(QStringLiteral("source")).toString()
        != QStringLiteral("day02-test")) {
        return fail("attribute was not stored");
    }

    std::cout << "DataFrame test passed\n";
    return 0;
}
