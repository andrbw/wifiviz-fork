#pragma once

#include <QWidget>
#include <QVector>
#include <QPushButton>
#include <QString>
#include <QHash>
#include <QColor>
#include <QPair>
#include <QSet>

#include <QFileDialog>
#include <QInputDialog>
#include <QRect>

#include <algorithm>

#include "ppdu_visual_item.h"
#include "ppdu_detail_window.h"
#include "ppdu_info_overlay.h"
#include "legend_overlay.h"
#include "utils.h"

class QPainter;
class QScrollBar;

enum class TimelineRowMode
{
    BySender,
    ByReceiver,
    ByChannel,
    ByNodeLink
};

enum class TimelineViewMode
{
    PpduTimeline,
    MloChannelTimeline,
    MloChannelState,
    ChannelState,
    RxTimeline,
    PhyStateTimeline
};

struct TimeRangeStats
{
    uint64_t durationNs = 0;

    uint64_t busyNs = 0;
    uint64_t idleNs = 0;

    uint64_t totalBytes = 0;

    double throughputMbps = 0.0;
    double utilization = 0.0; // [0,1]
};

struct CachedPpduLayoutItem
{
    uint64_t rowKey = 0;
    int row = -1;
    int lane = 0;
    bool overlap = false;
};

struct CachedPpduRow
{
    uint64_t rowKey = 0;
    QString label;
    QVector<int> itemIndices;
    int laneCount = 1;
};

struct CachedPhyStateRow
{
    uint64_t rowKey = 0;
    QString label;
    uint16_t nodeId = 0;
    uint8_t linkId = 0;
    uint8_t channel = 0;
    QVector<int> itemIndices;
};

/* Geometry of the band the timeline rows are drawn in: everything between the toolbar at the
 * top and the time axis at the bottom. Rows shrink to fit that band down to a minimum height;
 * past that the band scrolls, so a run with more stations than fit on screen keeps every row
 * reachable instead of dropping the ones that fall past the bottom edge. */
struct TimelineRowBand
{
    int rowH = 0;      // height of a single row
    int topY = 0;      // y of row 0, scroll already applied (above bandTop when scrolled)
    int bandTop = 0;   // top of the band
    int bandH = 0;     // visible height of the band
    int contentH = 0;  // height of all rows together
    int scroll = 0;    // how far the rows are scrolled up, in pixels
    int maxScroll = 0; // 0 while every row fits

    int bandBottom() const { return bandTop + bandH; }
    // First and last row pixel actually on screen. Anything drawn under the rows -- the time
    // axis, the conflict bar -- hangs off axisY(), which is the bottom of the rows while they
    // all fit and the bottom of the band once they do not.
    int visibleTopY() const { return std::max(topY, bandTop); }
    int axisY() const { return std::min(topY + contentH, bandTop + bandH); }
    int visibleH() const { return std::max(0, axisY() - visibleTopY()); }
    bool containsY(int y) const { return y >= bandTop && y < bandTop + bandH; }
};

struct DeviceNodeInfo
{
    uint16_t nodeId = 0;
    DeviceRole role = DeviceRole::Unknown;
};

class PpduTimelineView : public QWidget,public ResettableBase
{
    Q_OBJECT
public:
    explicit PpduTimelineView(QWidget *parent = nullptr);
    ~PpduTimelineView();
    void setDetailWindow(PpduDetailWindow *detailWindow);
    
    void enableQuitButton(bool enable);  // 控制按钮
    void stopTimeline();                  // 停止 timeline/线程
    
    void append(const PpduVisualItem &ppdu);
    void clear();
    void resetPage() override;

    size_t Num_ap;
    size_t Num_sta;

signals:
    void timelineClosed();
    void quit_simulation();

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void wheelEvent(QWheelEvent *) override;

    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void leaveEvent(QEvent *) override;
    void closeEvent(QCloseEvent *) override;

private slots:
    void onSaveImage();
    void onToggleLegend();
    void onSetTimeRange();
    void onToggleChannelView();
    TimeRangeStats computeStats(uint64_t startNs, uint64_t endNs) const;
    void showStatsOverlay(const TimeRangeStats &stats, const QPoint &globalPos);

private:
    /* ===== geometry ===== */
    int apCount() const;
    TimelineRowBand rowBand(int rowCount, int minRowH, int maxRowH) const;
    TimelineRowBand ppduRowBand() const;
    QRect rowClipRect(const TimelineRowBand &band) const;
    void syncVScrollBar(const TimelineRowBand &band);
    void scrollRowsBy(int deltaPx);
    int effectiveRowHeight() const;
    int timelineTopY() const;
    void paintPpduTimeline(QPainter &painter);
    void paintRxTimeline(QPainter &painter);
    void paintMloChannelStateView(QPainter &painter);
    void paintChannelStateView(QPainter &painter);
    void paintPhyStateTimeline(QPainter &painter);
    bool showMloChannelStateHover(const QPoint &pos);
    bool showChannelStateHover(const QPoint &pos);
    bool showPhyStateHover(const QPoint &pos);
    void updateModeButton();
    std::pair<uint64_t, uint64_t> currentTimeBounds() const;
    int usableTimelineWidth() const;
    int64_t clampViewStartNs(double requestedStartNs, double nsToPixel) const;
    void syncRangeSliderToView();
    void scheduleDataUpdate();
    void schedulePhyStateDataUpdate();
    void rebuildPpduLayoutCache();
    void ensurePpduLayoutCache() const;
    void markPpduLayoutDirty();
    void rebuildPhyStateCache();
    void ensurePhyStateCache() const;
    void markPhyStateCacheDirty();
    void updateDetailWindow(int idx);
    QString roleLabel(uint64_t mac) const;
    QString roleTooltip(uint64_t mac) const;
    bool showRowLabelTooltip(const QPoint &pos);
    void hideHoverUi();
    DeviceRole deviceRole(uint64_t mac) const;
    DeviceRole deviceRoleForNode(uint16_t nodeId) const;
    QString nodeRoleLabel(uint16_t nodeId) const;
    uint16_t nodeIdForMac(uint64_t mac) const;
    QColor senderColor(uint64_t mac) const;

    /* ===== logic ===== */
    bool hasOverlap(int idx) const;
    int hitTest(const QPoint &pos) const;
    uint64_t rowKey(const PpduVisualItem &ppdu) const;

private:
    QVector<PpduVisualItem> m_ppduItems;
    QVector<PpduVisualItem> m_phyStateItems;
    mutable QVector<CachedPpduLayoutItem> m_cachedPpduLayout;
    mutable QVector<CachedPpduRow> m_cachedPpduRows;
    mutable bool m_ppduLayoutDirty = true;
    mutable QVector<CachedPhyStateRow> m_cachedPhyStateRows;
    mutable uint64_t m_phyStateStartNs = 0;
    mutable uint64_t m_phyStateEndNs = 0;
    mutable bool m_phyStateCacheDirty = true;
    bool m_dataUpdateQueued = false;
    bool m_phyStateUpdateQueued = false;

    /* view */
    int64_t m_viewStartNs = 0;
    double m_nsToPixel = 1e-6;

    /* hover */
    int m_hoverIndex = -1;
    int m_selectedIndex = -1;
    int m_pressedIndex = -1;
    bool m_pressMoved = false;
    QHash<uint64_t, DeviceNodeInfo> m_deviceInfoMap;
    QHash<uint16_t, uint64_t> m_nodeToMacMap;
    QSet<quint64> m_seenPpduKeys;

    /* row layout (IMPORTANT: shared!) */
    int m_rowHeight = 30;
    mutable int m_rowScroll = 0;
    bool m_bandSynced = false;
    QPoint m_mousePos;


    /* margins */
    int m_leftMargin = 120;
    int m_topMargin = 20;

    /* drag */
    bool m_dragging = false;
    QPoint m_lastMousePos;
    QPoint m_pressPos;

    /* selection */
    bool m_selecting = false;
    QPoint m_selectStart;
    QPoint m_selectEnd;

    /* UI */
    QPushButton *m_btnSave = nullptr;
    QPushButton *m_btnLegend = nullptr;
    QPushButton *m_btnSetTimeRange = nullptr;
    QPushButton *quitButton = nullptr;
    QPushButton *m_btnChannel = nullptr;
    QScrollBar *m_vScroll = nullptr;

    PpduInfoOverlay *m_overlay = nullptr;
    LegendOverlay *m_legendOverlay = nullptr;
    PpduDetailWindow *m_detailWindow = nullptr;

    bool m_hasSelection = false;
    uint64_t m_selStartNs = 0;
    uint64_t m_selEndNs = 0;
    bool m_showingStats = false;

    /* ===== Time Range Slider ===== */
    bool m_rangeDragging = false;
    bool m_dragLeftHandle = false;
    bool m_dragRightHandle = false;
    bool m_dragRangeBody = false;

    double m_rangeStart = 0.0; // [0,1]
    double m_rangeEnd = 1.0;   // [0,1]

    int m_lastRangeX = 0;

    /* ===== view mode ===== */
    TimelineRowMode m_rowMode;
    TimelineViewMode m_viewMode;
};
