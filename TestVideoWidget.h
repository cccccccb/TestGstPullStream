#ifndef TESTVIDEOWIDGET_H
#define TESTVIDEOWIDGET_H

#include <QMutex>
#include <QStack>
#include <QWidget>

#include "gst/gst.h"
#include "gst/app/gstappsink.h"
#include "gst/video/gstvideometa.h"
#include "gst/gstsample.h"

class TestVideoWidget : public QWidget
{
    Q_OBJECT
    friend GstFlowReturn new_sample(GstAppSink *, gpointer);

public:
    explicit TestVideoWidget(QWidget *parent = nullptr);
    ~TestVideoWidget();

    bool createPipeline();
    void cleanUp();

protected:
    Q_INVOKABLE void pendingDisplayFrame();
    void paintEvent(QPaintEvent *event) override;
    void timerEvent(QTimerEvent *event) override;

    bool scheduleFrame();

protected:
    GstElement *m_pipeline;
    GstAppSink *m_appsink;

    QMutex m_queueMutex;
    QStack<GstSample *> m_pendingSamples;

    QMutex m_paintMutex;
    QImage *m_displayImage;

    int m_delayTimerId;
};

#endif // TESTVIDEOWIDGET_H
