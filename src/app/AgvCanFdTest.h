#pragma once

class QApplication;
class AppContext;

// Runs a headless CAN FD link test. When move300mm is true, the test performs
// the safety handshake and then commands a closed-loop 300 mm forward move.
int runAgvCanFdTest(QApplication& app, AppContext& context, bool move300mm);
