#include "core/newsticker.h"

#include <QPainter>
#include <QFontMetrics>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDesktopServices>
#include <QSettings>
#include <QUrl>

// ── Static helpers ────────────────────────────────────────────────────────
QStringList NewsTicker::availableSubreddits()
{
    return {"programming", "Python", "javascript", "C_Programming", "cpp"};
}

// ── Constructor ───────────────────────────────────────────────────────────
NewsTicker::NewsTicker(QWidget* parent)
    : QWidget(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    setFixedHeight(20);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setCursor(Qt::PointingHandCursor);

    m_scrollTimer = new QTimer(this);
    m_scrollTimer->setInterval(SCROLL_INTERVAL);
    connect(m_scrollTimer, &QTimer::timeout, this, &NewsTicker::onScrollTick);

    m_pollTimer = new QTimer(this);
    connect(m_pollTimer, &QTimer::timeout, this, &NewsTicker::onPollTick);

    connect(m_nam, &QNetworkAccessManager::finished,
            this,  &NewsTicker::onNetworkReply);

    applySettings();
}

// ── Configuration ─────────────────────────────────────────────────────────
void NewsTicker::setSubreddits(const QStringList& subs)
{
    m_subreddits = subs;
}

void NewsTicker::setPollIntervalMinutes(int minutes)
{
    m_pollIntervalMs = minutes * 60 * 1000;
    if (m_pollTimer->isActive()) {
        m_pollTimer->setInterval(m_pollIntervalMs);
    }
}

void NewsTicker::setEnabled(bool enabled)
{
    if (enabled) {
        setVisible(true);
        if (!m_running) {
            m_running = true;
            onPollTick();  // fetch immediately
            m_pollTimer->start(m_pollIntervalMs);
            m_scrollTimer->start();
        }
    } else {
        setVisible(false);
        m_running = false;
        m_scrollTimer->stop();
        m_pollTimer->stop();
    }
}

void NewsTicker::applySettings()
{
    QSettings s("CodeClarity", "CodeClarity");

    // Enabled toggle
    bool enabled = s.value("ticker/enabled", true).toBool();

    // Subreddits
    QStringList available = availableSubreddits();
    QStringList subs;
    for (const QString& sub : available) {
        if (s.value("ticker/sub/" + sub, sub == "programming").toBool())
            subs << sub;
    }
    if (subs.isEmpty()) subs << "programming";
    m_subreddits = subs;

    // Poll interval
    int intervalMin = s.value("ticker/intervalMin", 10).toInt();
    m_pollIntervalMs = intervalMin * 60 * 1000;

    setEnabled(enabled);
}

// ── Fetch ─────────────────────────────────────────────────────────────────
void NewsTicker::onPollTick()
{
    m_items.clear();
    m_pendingRequests = m_subreddits.size();

    if (m_pendingRequests == 0) {
        rebuildScrollItems();
        return;
    }

    for (const QString& sub : m_subreddits)
        fetchSubreddit(sub);
}

void NewsTicker::fetchSubreddit(const QString& sub)
{
    QUrl url(QString("https://www.reddit.com/r/%1/hot.json?limit=5").arg(sub));
    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", "CodeClarity/0.1 (educational IDE)");
    // Store sub name in the request for use in the reply handler
    req.setAttribute(QNetworkRequest::User, sub);
    m_nam->get(req);
}

void NewsTicker::onNetworkReply(QNetworkReply* reply)
{
    reply->deleteLater();

    // Which subreddit was this?
    QString sub = reply->request().attribute(QNetworkRequest::User).toString();

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(data, &err);

        if (err.error == QJsonParseError::NoError && doc.isObject()) {
            QJsonArray children = doc["data"]["children"].toArray();
            for (const QJsonValue& val : children) {
                QJsonObject post = val["data"].toObject();
                QString title    = post["title"].toString().trimmed();
                QString permalink = post["permalink"].toString();
                if (title.isEmpty()) continue;

                TickerItem item;
                item.subreddit = "r/" + sub;
                item.title     = title;
                item.url       = "https://www.reddit.com" + permalink;
                m_items.append(item);
            }
        }
    }
    // (silently ignore network errors — ticker just stays empty)

    --m_pendingRequests;
    if (m_pendingRequests <= 0) {
        m_pendingRequests = 0;
        rebuildScrollItems();
    }
}

// ── Build scroll segments ─────────────────────────────────────────────────
void NewsTicker::rebuildScrollItems()
{
    m_segments.clear();

    if (m_items.isEmpty()) {
        // Show placeholder while loading
        Segment s;
        s.prefix = "";
        s.title  = "Loading news...";
        s.url    = "";
        s.x      = 0;
        m_segments.append(s);
    } else {
        for (const TickerItem& item : m_items) {
            Segment seg;
            seg.prefix = "[" + item.subreddit + "]  ";
            seg.title  = item.title + "     ";   // trailing space before next item
            seg.url    = item.url;
            m_segments.append(seg);
        }
    }

    // Measure widths using the font we'll paint with
    QFont f = font();
    f.setPointSize(9);
    QFontMetrics fm(f);

    int x = 0;
    for (Segment& seg : m_segments) {
        seg.x     = x;
        seg.width = fm.horizontalAdvance(seg.prefix + seg.title) + SEGMENT_GAP;
        x        += seg.width;
    }
    m_totalWidth = x;
    m_scrollOffset = 0;

    update();
}

// ── Scroll tick ───────────────────────────────────────────────────────────
void NewsTicker::onScrollTick()
{
    if (m_segments.isEmpty() || m_totalWidth <= 0) return;

    m_scrollOffset += SCROLL_STEP;

    // Wrap: once scrollOffset >= totalWidth, reset to 0 (seamless loop)
    if (m_scrollOffset >= m_totalWidth)
        m_scrollOffset -= m_totalWidth;

    update();
}

// ── Paint ─────────────────────────────────────────────────────────────────
void NewsTicker::paintEvent(QPaintEvent*)
{
    QPainter p(this);

    // Background
    p.fillRect(rect(), QColor("#181825"));

    // Border lines (top + bottom, 1px each)
    p.setPen(QColor("#313244"));
    p.drawLine(0, 0, width(), 0);
    p.drawLine(0, height() - 1, width(), height() - 1);

    if (m_segments.isEmpty()) return;

    QFont f = font();
    f.setPointSize(9);
    p.setFont(f);
    QFontMetrics fm(f);
    int baseline = (height() + fm.ascent() - fm.descent()) / 2;

    // Draw two copies of the track so wrap is seamless
    for (int pass = 0; pass < 2; ++pass) {
        int trackOffset = pass * m_totalWidth;

        for (const Segment& seg : m_segments) {
            int drawX = seg.x + trackOffset - m_scrollOffset;

            // Clip: skip if entirely off screen
            if (drawX + seg.width < 0 || drawX > width()) continue;

            // Subreddit prefix in accent color
            p.setPen(QColor("#89b4fa"));
            p.drawText(drawX, baseline, seg.prefix);

            // Title in muted color
            int prefixW = fm.horizontalAdvance(seg.prefix);
            p.setPen(QColor("#a6adc8"));
            p.drawText(drawX + prefixW, baseline, seg.title);
        }
    }
}

// ── Mouse click ───────────────────────────────────────────────────────────
void NewsTicker::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || m_segments.isEmpty()) return;

    QFont f = font();
    f.setPointSize(9);
    QFontMetrics fm(f);

    int clickX = event->pos().x();

    // Check both passes (same as paintEvent) to find which segment was clicked
    for (int pass = 0; pass < 2; ++pass) {
        int trackOffset = pass * m_totalWidth;
        for (const Segment& seg : m_segments) {
            int drawX = seg.x + trackOffset - m_scrollOffset;
            if (clickX >= drawX && clickX < drawX + seg.width) {
                if (!seg.url.isEmpty())
                    QDesktopServices::openUrl(QUrl(seg.url));
                return;
            }
        }
    }
}

void NewsTicker::resizeEvent(QResizeEvent*)
{
    // Nothing special needed — painting uses widget width dynamically
}
