#pragma once

class DebugCore;
class QString;

// Application composition root shared by GUI and command-line modes.
// Add a concrete service here only when it has a real application caller.
class AppContext {
public:
    AppContext();

    DebugCore* debugCore() const;
    QString pluginRoot() const;
    void scanPlugins() const;

private:
    DebugCore* m_core = nullptr;
};
