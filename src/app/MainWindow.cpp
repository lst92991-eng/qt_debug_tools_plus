#include "MainWindow.h"

#include "ui_MainWindow.h"

#include <QAction>
#include <QPlainTextEdit>
#include <QStatusBar>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_ui(new Ui::MainWindow)
{
    m_ui->setupUi(this);

    connect(m_ui->actionExit, &QAction::triggered, this, &QWidget::close);

    m_ui->activityLog->appendPlainText(tr("Day 1：主窗口外壳已就绪"));
    statusBar()->showMessage(tr("Day 1：尚未接入设备和插件"));
}

MainWindow::~MainWindow()
{
    delete m_ui;
}
