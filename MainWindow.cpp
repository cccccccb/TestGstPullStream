#include "MainWindow.h"
#include "TestVideoWidget.h"
#include "ui_MainWindow.h"

#include <QHBoxLayout>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);

    TestVideoWidget *testWidget = new TestVideoWidget(this);
    testWidget->createPipeline();
    mainLayout->addWidget(testWidget);
}

MainWindow::~MainWindow()
{
    delete ui;
}

