#pragma once

#include <QWidget>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>
#include <QStringList>
#include <QList>
#include <QPaintEvent>
#include <QMouseEvent>

// ── TickerItem ────────────────────────────────────────────────────────────
struct TickerItem {
    QString subreddit;   // e.g. "r/programming"
    QString title;
    QString url;         // full Reddit URL to open on click
};

// ── NewsTicker ────────────────────────────────────────────────────────────
// A thin horizontal strip that scrolls post titles from Reddit.
//
// Visual: dark background (#181825), 20px tall, 1px border top+bottom.
// Text color: #a6adc8.  Subreddit prefix: #89b4fa (accent).
//
// Scrolling: QTimer moves pixel offset left by 2px every 30ms.
// When an item scrolls fully off the left edge it wraps to the right.
//
// Clicking anywhere within an item's bounding rectangle opens the Reddit
// post via QDesktopServices::openUrl.
//
// The widget fetches from the Reddit JSON API (no auth required):
//   https://www.reddit.com/r/<sub>/hot.json?limit=5
// One request per subreddit per poll cycle.
class NewsTicker : public QWidget {
    Q_OBJECT

public:
    explicit NewsTicker(QWidget* parent = nullptr);

    // Configuration — called from Settings
    void setSubreddits(const QStringList& subs);
    void setPollIntervalMinutes(int minutes);
    void setEnabled(bool enabled);   // show/hide + start/stop

    // Subreddits supported
    static QStringList availableSubreddits();

    // Reload config from QSettings and restart
    void applySettings();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onScrollTick();
    void onPollTick();
    void onNetworkReply(QNetworkReply* reply);

private:
    void fetchSubreddit(const QString& sub);
    void rebuildScrollItems();

    // Each rendered segment: text + its current x position + click target
    struct Segment {
        QString prefix;     // "[r/programming] "
        QString title;
        QString url;
        int     x = 0;     // current left edge in widget coordinates
        int     width = 0; // cached pixel width
    };

    QNetworkAccessManager* m_nam        = nullptr;
    QTimer*                m_scrollTimer = nullptr;
    QTimer*                m_pollTimer   = nullptr;

    QList<TickerItem>  m_items;
    QList<Segment>     m_segments;

    QStringList m_subreddits;
    int         m_pollIntervalMs = 10 * 60 * 1000;  // 10 min default
    int         m_pendingRequests = 0;

    // Scroll state
    int  m_scrollOffset = 0;  // how many px the whole track has moved left
    int  m_totalWidth   = 0;  // sum of all segment widths + spacing
    bool m_running      = false;

    static constexpr int SEGMENT_GAP   = 60;   // px gap between items
    static constexpr int SCROLL_STEP   = 2;    // px per tick
    static constexpr int SCROLL_INTERVAL = 30; // ms per tick
};
