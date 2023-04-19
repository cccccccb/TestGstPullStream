#include "TestVideoWidget.h"

#include <QDebug>
#include <QPainter>
#include <QPointer>
#include <QTimerEvent>

#define MAX_DECODED_FRAMES 2

GstFlowReturn new_sample(GstAppSink *gstappsink, gpointer data)
{
    Q_UNUSED(gstappsink);
    QPointer<TestVideoWidget> widget = static_cast<TestVideoWidget *>(data);

    if (!widget)
        return GST_FLOW_OK;

    widget->m_queueMutex.lock();
    GstSample *sample = gst_app_sink_pull_sample(widget->m_appsink);
    if (sample) {
        widget->m_pendingSamples.push(sample);
    } else {
        widget->m_pendingSamples.clear();
    }
    widget->m_queueMutex.unlock();

    QMetaObject::invokeMethod(widget, "pendingDisplayFrame", Qt::QueuedConnection);
    return GST_FLOW_OK;
}

static gboolean handle_pipeline_message(GstBus *bus, GstMessage *msg, gpointer data)
{
    Q_UNUSED(bus);
    Q_UNUSED(data);

    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_ERROR: {
        GError *error = nullptr;
        gchar *debug_info = nullptr;
        gst_message_parse_error(msg, &error, &debug_info);
        qWarning() << QString("Gstreamer error from element %1: %2").arg(GST_OBJECT_NAME(msg->src), error->message);

        if (debug_info) {
            qDebug() << "debug information: %1" << debug_info;
            g_free(debug_info);
        }

        g_clear_error(&error);
        break;
    }
    default:
        break;
    }

    return true;
}

TestVideoWidget::TestVideoWidget(QWidget *parent)
    : QWidget(parent)
    , m_displayImage(nullptr)
    , m_delayTimerId(0)
{

}

TestVideoWidget::~TestVideoWidget()
{
    cleanUp();
}

bool TestVideoWidget::createPipeline()
{
    GError *error = nullptr;
    if (!gst_init_check(nullptr, nullptr, &error)) {
        qWarning() << "Disabling GStreamer video support: " << error->message;
        g_clear_error(&error);
        return false;
    }

    QString parseLaunchString("udpsrc uri=udp://224.0.0.1:8888 ! "
                              "application/x-rtp,media=video,clock-rate=90000,encoding-name=H264,payload=96 ! "
                              "rtph264depay ! h264parse ! queue ! avdec_h264 ! videoconvert ! "
                              "appsink name=sink caps=video/x-raw,format=BGRx sync=false drop=false");

    qDebug() << parseLaunchString;
    m_pipeline = gst_parse_launch(parseLaunchString.toLocal8Bit().data(), &error);

    if (!m_pipeline) {
        qWarning() << "Parse launch pipeline failed, "<< error->message;
        g_clear_error(&error);
        return false;
    }

    m_appsink = GST_APP_SINK(gst_bin_get_by_name(GST_BIN(m_pipeline), "sink"));
    if (!m_appsink) {
        qWarning("Init appsink failed.");
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
        return false;
    }

    GstAppSinkCallbacks appsink_cbs;
    appsink_cbs.new_sample = new_sample;
    appsink_cbs.eos = nullptr;
    appsink_cbs.new_preroll = nullptr;
    appsink_cbs.new_event = nullptr;

    gst_app_sink_set_callbacks(m_appsink, &appsink_cbs, this, nullptr);
    gst_app_sink_set_max_buffers(m_appsink, MAX_DECODED_FRAMES);

    GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(m_pipeline));
    gst_bus_add_watch(bus, handle_pipeline_message, this);
    gst_object_unref(bus);

    if (gst_element_set_state(m_pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        qDebug() << "GStreamer error: Unable to set the pipeline to the playing state.";

        if (m_appsink) {
            gst_object_unref(m_appsink);
            m_appsink = nullptr;
        }

        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
        return false;
    }

    return true;
}

void TestVideoWidget::cleanUp()
{
    m_paintMutex.lock();
    delete m_displayImage;
    m_displayImage = nullptr;
    m_paintMutex.unlock();

    m_queueMutex.lock();
    while(!m_pendingSamples.isEmpty()) {
        GstSample *sample = m_pendingSamples.pop();
        g_clear_pointer(&sample, gst_sample_unref);
    }
    m_queueMutex.unlock();

    gst_object_unref(m_appsink);
    gst_object_unref(m_pipeline);
    m_appsink = nullptr;
    m_pipeline = nullptr;
}

void TestVideoWidget::pendingDisplayFrame()
{
    if (m_delayTimerId)
        return;

    m_delayTimerId = this->startTimer(20);
}

void TestVideoWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QMutexLocker locker(&m_paintMutex);
    QPainter painter(this);

    if (!m_displayImage) {
        painter.fillRect(this->rect(), Qt::black);
        return;
    }

    painter.drawImage(this->rect(), *m_displayImage);
}

void TestVideoWidget::timerEvent(QTimerEvent *event)
{
    if (m_delayTimerId == event->timerId()) {
        killTimer(m_delayTimerId);
        m_delayTimerId = 0;

        if (scheduleFrame()) {
            this->update();
        }
    }
}

bool TestVideoWidget::scheduleFrame()
{
    m_paintMutex.lock();
    if (m_displayImage) {
        delete m_displayImage;
        m_displayImage = nullptr;
    }
    m_paintMutex.unlock();

    QMutexLocker locker(&m_queueMutex);
    if (m_pendingSamples.isEmpty())
        return false;

    GstSample *gstSample = m_pendingSamples.pop();
    locker.unlock();

    bool ret = false;
    while (gstSample) {
        GstCaps *caps = nullptr;
        GstBuffer *buffer = nullptr;

        do {
            caps = gst_sample_get_caps(gstSample);

            if (!caps)
                break;

            GstStructure *s = gst_caps_get_structure(caps, 0);
            if (!s)
                break;

            gint width = 0;
            gint height = 0;

            if (!gst_structure_get_int(s, "width", &width) ||
                    !gst_structure_get_int(s, "height", &height)) {
                break;
            }

            buffer = gst_sample_get_buffer(gstSample);

            GstMapInfo mapInfo;
            if (!gst_buffer_map(buffer, &mapInfo, GST_MAP_READ))
                break;

            m_paintMutex.lock();
            m_displayImage = new QImage(mapInfo.data, width, height, QImage::Format_ARGB32);
            m_paintMutex.unlock();

            gst_buffer_unmap(buffer, &mapInfo);
            ret = true;
        } while (false);

        g_clear_pointer(&gstSample, gst_sample_unref);

        if (!ret) {
            locker.relock();
            if (!m_pendingSamples.isEmpty())
                gstSample = m_pendingSamples.pop();


            locker.unlock();
        }
    }

    return ret;
}
