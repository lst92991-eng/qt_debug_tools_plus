#pragma once

class QApplication;

// Runs a headless CAN FD link test. When move300mm is true, the test performs
// the safety handshake and then commands a closed-loop 300 mm forward move.
int runAgvCanFdTest(QApplication& app, bool move300mm);
