#ifndef CHIAKI_GUI_STATSOVERLAY_H
#define CHIAKI_GUI_STATSOVERLAY_H

#include <QWidget>
#include <QPointer>
#include <QVector>

class QmlMainWindow;
class QmlBackend;
class StreamSession;
class QLabel;
class QFrame;
class QGridLayout;
class QTimer;
class PacingChart;

// Stream statistics overlay: a small frameless window owned by the stream
// window (it follows it in the z-order instead of staying above every other
// application), sized to its content and placed at one of nine positions.
// It is a widget of its own, not QML, so that updating it never makes the
// Vulkan stream window present an extra frame.
//
// Collapsed it is a pill: "60 fps · VFG 2× · Reflex · 4 ms". A click opens
// the details: how evenly the last frames stayed on screen (present wait),
// the stream and screen rates, enhancements and network numbers.
//
// Styles (setting): 0 Mini and 1 Compact start as the pill (Compact adds
// Mbps and packet loss), 2 Default and 3 Full start open (Full adds the
// queue numbers).
class StatsOverlayWidget : public QWidget
{
public:
    enum Style { Mini = 0, Compact = 1, Default = 2, Full = 3 };

    StatsOverlayWidget(QmlMainWindow *owner, QmlBackend *backend);

    // style: see Style; position: 0-2 top left/centre/right, 3-5 middle,
    // 6-8 bottom.
    void setLayoutOptions(int new_style, int new_position);
    void setOverlayVisible(bool visible);
    void syncGeometry();
    void refresh();

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    void updateTransientParent();
    void setSession(StreamSession *session);
    void syncGeometryIfVisible();
    void applyLayout();
    void setRow(int row, const QString &value, bool visible);

    QPointer<QmlMainWindow> owner;
    QPointer<QmlBackend> backend;
    QPointer<StreamSession> current_session;
    int style = Default;
    int position = 5; // middle right
    bool expanded = true;

    QFrame *pill = nullptr;
    QLabel *pillDot = nullptr;
    QLabel *pillLabel = nullptr;
    QFrame *details = nullptr;
    QLabel *detailsTitle = nullptr;
    QLabel *detailsHint = nullptr;
    PacingChart *chart = nullptr;
    QLabel *legend = nullptr;
    QGridLayout *grid = nullptr;
    QVector<QLabel *> rowTitles;
    QVector<QLabel *> rowValues;
    QTimer *chartTimer = nullptr;
};

#endif // CHIAKI_GUI_STATSOVERLAY_H
