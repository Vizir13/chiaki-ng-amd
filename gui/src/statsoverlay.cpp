#include "statsoverlay.h"
#include "qmlmainwindow.h"
#include "qmlbackend.h"
#include "streamsession.h"
#include "vkreflex.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>
#include <algorithm>

// Colors of the app theme (gui/src/qml/controls/Theme.qml)
namespace {
const QColor colText(0xE8, 0xED, 0xF4);
const QColor colSecondary(0x9A, 0xA7, 0xB8);
const QColor colAccent(0x4C, 0xC2, 0xFF);
const QColor colWarn(0xF5, 0xA5, 0x24);
const QColor colGood(0x7F, 0xD9, 0x9A);
const QColor colBad(0xFF, 0x8A, 0x8A);
const QColor colLine(0x26, 0x30, 0x41);
const QString fontFamily = QStringLiteral("Inter");
constexpr int chartFrames = 60;

QFont uiFont(int pixel_size, QFont::Weight weight = QFont::Normal)
{
    QFont f(fontFamily);
    f.setPixelSize(pixel_size);
    f.setWeight(weight);
    return f;
}

enum Row { RowStream, RowScreen, RowUpscaling, RowFrameGen, RowEnhancement, RowToScreen, RowPacketLoss, RowLost, RowDropped, RowQueue, RowPending, RowCount };

bool isFrameGenPart(const QString &part)
{
    return part.contains(QStringLiteral("VFG")) || part.contains(QStringLiteral("DLSS")) || part.contains(QStringLiteral("FSR"))
        || part.contains(QStringLiteral("Flow")) || part.contains(QStringLiteral("frame gen"), Qt::CaseInsensitive);
}

QString shortPart(QString part)
{
    part.replace(QStringLiteral("NVIDIA "), QString());
    part.replace(QStringLiteral("RTX VSR"), QStringLiteral("RTX Video"));
    part.replace(QStringLiteral(" (half-rate source)"), QStringLiteral(" ½"));
    static const QRegularExpression times(QStringLiteral("\\b(\\d)x\\b"));
    part.replace(times, QStringLiteral("\\1×"));
    return part;
}
}

// Bars: how many display refreshes each of the last frames stayed on screen.
class PacingChart : public QWidget
{
public:
    explicit PacingChart(QWidget *parent) : QWidget(parent)
    {
        setFixedHeight(80);
        setAttribute(Qt::WA_TranslucentBackground);
    }

    void setData(const QVector<int> &v, int expected_refreshes)
    {
        values = v;
        expected = expected_refreshes;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const int base = height() - 2;
        p.setPen(QPen(colLine, 1));
        p.drawLine(0, base + 1, width(), base + 1);
        if (values.isEmpty())
            return;
        const int top_value = std::max(expected + 2, *std::max_element(values.cbegin(), values.cend()));
        const double gap = 2.0;
        const double bar_w = (width() - gap * (chartFrames - 1)) / chartFrames;
        // Newest bar on the right
        const int offset = chartFrames - values.size();
        p.setPen(Qt::NoPen);
        for (int i = 0; i < values.size(); i++) {
            const int n = values[i];
            const double h = std::max(3.0, (base - 4) * static_cast<double>(n) / top_value);
            const double x = (offset + i) * (bar_w + gap);
            QPainterPath path;
            path.addRoundedRect(QRectF(x, base - h, bar_w, h + 2), 2, 2);
            p.fillPath(path, n == expected ? colAccent : colWarn);
        }
    }

private:
    QVector<int> values;
    int expected = 0;
};

StatsOverlayWidget::StatsOverlayWidget(QmlMainWindow *owner, QmlBackend *backend)
    : QWidget(nullptr)
    , owner(owner)
    , backend(backend)
{
    setObjectName(QStringLiteral("statsOverlayWidget"));
    // Qt::Tool + a transient parent: an owned window, kept above the stream
    // window only (Qt::ToolTip and WindowStaysOnTopHint made it float above
    // every application). It takes clicks (to open the details) but never
    // the focus, so the stream keeps the keyboard and controller.
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFocusPolicy(Qt::NoFocus);
    setAutoFillBackground(false);
    setCursor(Qt::PointingHandCursor);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(10);
    outer->setSizeConstraint(QLayout::SetFixedSize);

    setStyleSheet(QStringLiteral(
        "#statsPill { background: rgba(10, 14, 19, 205); border: 1px solid rgba(255, 255, 255, 31); border-radius: 20px; }"
        "#statsDetails { background: rgba(10, 14, 19, 220); border: 1px solid rgba(255, 255, 255, 26); border-radius: 16px; }"
        "QLabel { color: %1; background: transparent; }").arg(colText.name()));

    // Pill
    pill = new QFrame(this);
    pill->setObjectName(QStringLiteral("statsPill"));
    pill->setFixedHeight(40);
    auto *pillLayout = new QHBoxLayout(pill);
    pillLayout->setContentsMargins(14, 0, 16, 0);
    pillLayout->setSpacing(8);
    pillDot = new QLabel(pill);
    pillDot->setFixedSize(9, 9);
    pillLabel = new QLabel(pill);
    pillLabel->setTextFormat(Qt::RichText);
    pillLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    pillLabel->setFont(uiFont(13, QFont::DemiBold));
    pillLayout->addWidget(pillDot);
    pillLayout->addWidget(pillLabel);
    outer->addWidget(pill);

    // Details
    details = new QFrame(this);
    details->setObjectName(QStringLiteral("statsDetails"));
    details->setFixedWidth(440);
    auto *detailsLayout = new QVBoxLayout(details);
    detailsLayout->setContentsMargins(20, 18, 20, 18);
    detailsLayout->setSpacing(12);

    auto *head = new QHBoxLayout();
    detailsTitle = new QLabel(tr("Frame pacing on screen"), details);
    detailsTitle->setFont(uiFont(14, QFont::DemiBold));
    detailsHint = new QLabel(details);
    detailsHint->setFont(uiFont(12));
    detailsHint->setStyleSheet(QStringLiteral("color: %1;").arg(colSecondary.name()));
    head->addWidget(detailsTitle);
    head->addStretch(1);
    head->addWidget(detailsHint);
    detailsLayout->addLayout(head);

    chart = new PacingChart(details);
    detailsLayout->addWidget(chart);

    legend = new QLabel(details);
    legend->setTextFormat(Qt::RichText);
    legend->setFont(uiFont(12));
    legend->setStyleSheet(QStringLiteral("color: %1;").arg(colSecondary.name()));
    detailsLayout->addWidget(legend);

    grid = new QGridLayout();
    grid->setHorizontalSpacing(20);
    grid->setVerticalSpacing(8);
    const QString titles[RowCount] = {
        tr("Stream"), tr("On screen"), tr("Upscaling"), tr("Frame generation"), tr("Enhancement"),
        tr("Present → on screen"), tr("Packet loss"), tr("Lost frames"), tr("Dropped frames"),
        tr("Queue depth"), tr("Pending frame age"),
    };
    for (int i = 0; i < RowCount; i++) {
        auto *t = new QLabel(titles[i], details);
        t->setFont(uiFont(13));
        t->setStyleSheet(QStringLiteral("color: %1;").arg(colSecondary.name()));
        auto *v = new QLabel(details);
        v->setFont(uiFont(13));
        v->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        v->setTextInteractionFlags(Qt::NoTextInteraction);
        grid->addWidget(t, i, 0);
        grid->addWidget(v, i, 1);
        rowTitles.append(t);
        rowValues.append(v);
    }
    grid->setColumnStretch(1, 1);
    detailsLayout->addLayout(grid);
    outer->addWidget(details);

    chartTimer = new QTimer(this);
    chartTimer->setInterval(250);
    connect(chartTimer, &QTimer::timeout, this, &StatsOverlayWidget::refresh);

    connect(owner, &QmlMainWindow::queueDepthAverageChanged, this, &StatsOverlayWidget::refresh);
    connect(owner, &QmlMainWindow::pendingFrameAgeChanged, this, &StatsOverlayWidget::refresh);
    connect(owner, &QmlMainWindow::droppedFramesChanged, this, &StatsOverlayWidget::refresh);
    connect(owner, &QmlMainWindow::videoEnhancementStatusChanged, this, &StatsOverlayWidget::refresh);
    connect(owner, &QmlMainWindow::fpsChanged, this, &StatsOverlayWidget::refresh);
    if (backend) {
        connect(backend, &QmlBackend::sessionChanged, this, [this](StreamSession *s) {
            setSession(s);
            refresh();
        });
        setSession(backend->qmlSession());
    }
    applyLayout();
    refresh();
}

void StatsOverlayWidget::setLayoutOptions(int new_style, int new_position)
{
    new_style = std::clamp(new_style, 0, 3);
    new_position = std::clamp(new_position, 0, 8);
    if (new_style == style && new_position == position)
        return;
    if (new_style != style)
        expanded = new_style >= Default;
    style = new_style;
    position = new_position;
    applyLayout();
    refresh();
}

void StatsOverlayWidget::setOverlayVisible(bool visible)
{
    if (visible) {
        // The owner must be known before the native window is shown, so
        // that it is created as an owned window.
        if (!windowHandle())
            create();
        updateTransientParent();
        refresh();
        if (!isVisible())
            show();
        updateTransientParent();
        syncGeometry();
        if (expanded)
            chartTimer->start();
        if (owner)
            owner->requestActivate();
    } else {
        chartTimer->stop();
        if (QWindow *overlay_window = windowHandle())
            overlay_window->setTransientParent(nullptr);
        hide();
    }
}

void StatsOverlayWidget::syncGeometry()
{
    if (!owner)
        return;
    const QRect area = owner->geometry();
    if (layout())
        layout()->activate();
    const QSize size = this->size();
    const int margin = 24;
    const int col = position % 3;
    const int row = position / 3;
    int x = area.x() + margin;
    if (col == 1)
        x = area.x() + (area.width() - size.width()) / 2;
    else if (col == 2)
        x = area.x() + area.width() - size.width() - margin;
    int y = area.y() + margin;
    if (row == 1)
        y = area.y() + (area.height() - size.height()) / 2;
    else if (row == 2)
        y = area.y() + area.height() - size.height() - margin;
    const QRect g(QPoint(x, y), size);
    if (geometry() != g)
        setGeometry(g);
}

void StatsOverlayWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    expanded = !expanded;
    applyLayout();
    refresh();
    if (expanded && isVisible())
        chartTimer->start();
    else
        chartTimer->stop();
    // The stream window keeps the input
    if (owner)
        owner->requestActivate();
    event->accept();
}

void StatsOverlayWidget::updateTransientParent()
{
    if (!owner)
        return;
    if (QWindow *overlay_window = windowHandle())
        overlay_window->setTransientParent(owner);
}

void StatsOverlayWidget::setSession(StreamSession *session)
{
    if (session == current_session)
        return;
    if (current_session)
        QObject::disconnect(current_session, nullptr, this, nullptr);
    current_session = session;
    if (current_session) {
        connect(current_session, &StreamSession::MeasuredBitrateChanged, this, &StatsOverlayWidget::refresh);
        connect(current_session, &StreamSession::AveragePacketLossChanged, this, &StatsOverlayWidget::refresh);
        connect(current_session, &StreamSession::FramesLostChanged, this, &StatsOverlayWidget::refresh);
        connect(current_session, &StreamSession::ConnectedChanged, this, &StatsOverlayWidget::refresh);
    }
}

void StatsOverlayWidget::applyLayout()
{
    details->setVisible(expanded);
    const int col = position % 3;
    const Qt::Alignment align = col == 0 ? Qt::AlignLeft : (col == 1 ? Qt::AlignHCenter : Qt::AlignRight);
    if (auto *outer = qobject_cast<QVBoxLayout *>(layout())) {
        outer->setAlignment(pill, align);
        outer->setAlignment(details, align);
        // The pill sits on the side nearest the screen edge
        const bool bottom = position / 3 == 2;
        outer->removeWidget(pill);
        outer->removeWidget(details);
        if (bottom) {
            outer->addWidget(details, 0, align);
            outer->addWidget(pill, 0, align);
        } else {
            outer->addWidget(pill, 0, align);
            outer->addWidget(details, 0, align);
        }
    }
    const bool full = style == Full;
    rowTitles[RowQueue]->setVisible(full);
    rowValues[RowQueue]->setVisible(full);
    rowTitles[RowPending]->setVisible(full);
    rowValues[RowPending]->setVisible(full);
}

void StatsOverlayWidget::setRow(int row, const QString &value, bool visible)
{
    rowValues[row]->setText(value);
    rowTitles[row]->setVisible(visible);
    rowValues[row]->setVisible(visible);
}

void StatsOverlayWidget::refresh()
{
    StreamSession *session = current_session;
    const bool hasSession = session != nullptr;
    const QString enhancement = owner ? owner->videoEnhancementStatus() : QString();
    const double bitrate = hasSession ? session->GetMeasuredBitrate() : 0.0;
    const double packetLoss = hasSession ? session->GetAveragePacketLoss() : 0.0;
    const int lostFrames = hasSession ? static_cast<int>(session->GetFramesLost()) : 0;
    const int droppedFrames = owner ? owner->droppedFrames() : 0;
    const double streamFps = owner ? owner->streamFps() : 0.0;
    const double displayFps = owner ? owner->displayFps() : 0.0;

    // Frames on screen (present wait)
    int raw[chartFrames];
    double screenLatency = 0.0, hz = 0.0;
    const int count = VkReflex::recentPacing(raw, chartFrames, &screenLatency, &hz);
    QVector<int> pacing(raw, raw + count);
    int expected = 0;
    {
        int hist[7] = {};
        for (int n : pacing)
            hist[std::clamp(n, 0, 6)]++;
        for (int n = 1; n <= 6; n++) {
            if (hist[n] > hist[expected])
                expected = n;
        }
    }
    const int even = static_cast<int>(std::count(pacing.cbegin(), pacing.cend(), expected));
    const double evenShare = count ? static_cast<double>(even) / count : 1.0;

    // Enhancements by kind
    QStringList upscaling, frameGen, other;
    QString frameGenShort;
    for (const QString &part : enhancement.split(QStringLiteral(" · "), Qt::SkipEmptyParts)) {
        if (part.contains(QStringLiteral("VSR")))
            upscaling << shortPart(part);
        else if (isFrameGenPart(part)) {
            frameGen << shortPart(part);
            if (frameGenShort.isEmpty() && !part.contains(QLatin1Char(':'))) {
                // "VFG 2× 12.6 ms" -> "VFG 2×"
                static const QRegularExpression multiplier(QStringLiteral("^(.*?\\d×)"));
                const QString s = shortPart(part);
                const auto m = multiplier.match(s);
                frameGenShort = m.hasMatch() ? m.captured(1) : s.section(QLatin1Char(' '), 0, 0);
            }
        } else
            other << shortPart(part);
    }

    // Pill
    QColor dot = colGood;
    if (packetLoss > 0.05 || evenShare < 0.7)
        dot = colBad;
    else if (packetLoss > 0.01 || evenShare < 0.95)
        dot = colWarn;
    pillDot->setStyleSheet(QStringLiteral("background: %1; border-radius: 4px;").arg(dot.name()));
    QStringList items;
    const QString secondary = QStringLiteral("<span style=\"color:%1\">%2</span>").arg(colSecondary.name());
    if (hasSession) {
        items << tr("%1 fps").arg(displayFps, 0, 'f', 0).toHtmlEscaped();
        if (!frameGenShort.isEmpty())
            items << secondary.arg(frameGenShort.toHtmlEscaped());
        if (VkReflex::lowLatencyActive())
            items << secondary.arg(tr("Reflex"));
        if (count > 0)
            items << secondary.arg(tr("%1 ms").arg(screenLatency, 0, 'f', screenLatency < 10.0 ? 1 : 0));
        if (style == Compact) {
            items << secondary.arg(tr("%1 Mbps").arg(bitrate, 0, 'f', 1));
            if (packetLoss > 0.0)
                items << secondary.arg(tr("%1% loss").arg(packetLoss * 100.0, 0, 'f', 1));
        }
    } else {
        items << secondary.arg(tr("Connecting…"));
    }
    pillLabel->setText(items.join(QStringLiteral("&nbsp;<span style=\"color:#5B6778\">·</span>&nbsp;")));

    // Details
    if (expanded) {
        const bool havePacing = count > 0;
        chart->setVisible(havePacing);
        legend->setVisible(true);
        chart->setData(pacing, expected);
        detailsHint->setText(havePacing ? tr("last %1 frames").arg(count) : QString());
        if (havePacing) {
            const QString swatch = QStringLiteral("<span style=\"color:%1\">■</span>&nbsp;%2");
            legend->setText(swatch.arg(colAccent.name(), tr("Even (%1 refreshes)").arg(expected))
                + QStringLiteral("&nbsp;&nbsp;&nbsp;&nbsp;")
                + swatch.arg(colWarn.name(), tr("Uneven"))
                + QStringLiteral("&nbsp;&nbsp;&nbsp;&nbsp;")
                + tr("%1% even").arg(evenShare * 100.0, 0, 'f', 0));
        } else {
            legend->setText(tr("Needs the Vulkan renderer with present wait."));
        }

        setRow(RowStream, tr("%1 fps · %2 Mbps").arg(streamFps, 0, 'f', 0).arg(bitrate, 0, 'f', 1), hasSession);
        setRow(RowScreen, hz > 0.0 ? tr("%1 fps · %2 Hz").arg(displayFps, 0, 'f', 0).arg(hz, 0, 'f', 0) : tr("%1 fps").arg(displayFps, 0, 'f', 0), hasSession);
        setRow(RowUpscaling, upscaling.join(QLatin1Char('\n')), !upscaling.isEmpty());
        setRow(RowFrameGen, frameGen.join(QLatin1Char('\n')), !frameGen.isEmpty());
        setRow(RowEnhancement, other.join(QLatin1Char('\n')), !other.isEmpty());
        setRow(RowToScreen, tr("%1 ms").arg(screenLatency, 0, 'f', 1), havePacing);
        setRow(RowPacketLoss, QStringLiteral("%1%").arg(packetLoss * 100.0, 0, 'f', 1), hasSession);
        setRow(RowLost, QString::number(lostFrames), hasSession && (style == Full || lostFrames > 0));
        setRow(RowDropped, QString::number(droppedFrames), droppedFrames > 0);
        const bool full = style == Full;
        setRow(RowQueue, QString::number(owner ? owner->queueDepthAverage() : 0.0, 'f', 1), full);
        setRow(RowPending, tr("%1 ms").arg((owner ? owner->pendingFrameAge() : 0.0) * 1000.0, 0, 'f', 0), full);
        rowValues[RowPacketLoss]->setStyleSheet(QStringLiteral("color: %1;").arg((packetLoss > 0.01 ? colWarn : colText).name()));
    }
    syncGeometryIfVisible();
}

void StatsOverlayWidget::syncGeometryIfVisible()
{
    if (!isVisible())
        return;
    // Size changes with the text; let the layout settle first.
    pill->adjustSize();
    details->adjustSize();
    adjustSize();
    syncGeometry();
}
