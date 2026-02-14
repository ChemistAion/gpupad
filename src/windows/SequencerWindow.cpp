#include "SequencerWindow.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {

class TimelineWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit TimelineWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumHeight(120);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setMouseTracking(true);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const auto w = width();
        const auto h = height();
        const auto rulerH = 24;
        const auto trackH = 28;
        const auto trackCount = std::max(1, (h - rulerH) / trackH);

        // background
        p.fillRect(rect(), palette().base());

        // ruler background
        p.fillRect(0, 0, w, rulerH, palette().alternateBase());

        // ruler ticks and labels
        p.setPen(palette().text().color());
        const auto tickSpacing = 60;
        for (auto x = 0; x < w; x += tickSpacing) {
            const auto sec = (x * mDuration) / std::max(w, 1);
            p.drawLine(x, rulerH - 6, x, rulerH);
            p.drawText(x + 3, rulerH - 8,
                QString::number(sec, 'f', 1) + "s");
        }

        // ruler bottom line
        p.setPen(QPen(palette().mid().color(), 1));
        p.drawLine(0, rulerH, w, rulerH);

        // track lanes
        for (auto i = 0; i < trackCount; ++i) {
            const auto y = rulerH + i * trackH;
            if (i % 2 == 1)
                p.fillRect(0, y, w, trackH, palette().alternateBase());
            p.setPen(QPen(palette().mid().color(), 1));
            p.drawLine(0, y + trackH, w, y + trackH);
        }

        // playhead
        const auto playX =
            static_cast<int>((mPlayhead / mDuration) * w);
        p.setPen(QPen(QColor(220, 60, 60), 2));
        p.drawLine(playX, 0, playX, h);

        // playhead handle
        const QPointF tri[3] = {
            { static_cast<qreal>(playX - 5), 0.0 },
            { static_cast<qreal>(playX + 5), 0.0 },
            { static_cast<qreal>(playX), 6.0 },
        };
        p.setBrush(QColor(220, 60, 60));
        p.setPen(Qt::NoPen);
        p.drawPolygon(tri, 3);
    }

    void mousePressEvent(QMouseEvent *e) override
    {
        updatePlayhead(e->pos().x());
    }

    void mouseMoveEvent(QMouseEvent *e) override
    {
        if (e->buttons() & Qt::LeftButton)
            updatePlayhead(e->pos().x());
    }

private:
    void updatePlayhead(int x)
    {
        mPlayhead = std::clamp(
            (static_cast<double>(x) / std::max(width(), 1)) * mDuration,
            0.0, mDuration);
        update();
    }

    double mDuration{ 10.0 };
    double mPlayhead{ 0.0 };
};

} // namespace

SequencerWindow::SequencerWindow(QWidget *parent)
    : QFrame(parent)
{
    setFrameShape(QFrame::NoFrame);

    auto outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    mScrollArea = new QScrollArea(this);
    mScrollArea->setWidgetResizable(true);

    mContainer = new QWidget(mScrollArea);
    mLayout = new QVBoxLayout(mContainer);
    mLayout->setContentsMargins(4, 4, 4, 4);
    mLayout->setSpacing(4);

    auto timeline = new TimelineWidget(mContainer);
    mLayout->addWidget(timeline, 1);

    mEmptyLabel = new QLabel(tr("Sequencer (no tracks)"), mContainer);
    mEmptyLabel->setAlignment(Qt::AlignLeft | Qt::AlignBottom);
    auto font = mEmptyLabel->font();
    font.setPointSize(font.pointSize() - 1);
    mEmptyLabel->setFont(font);
    mEmptyLabel->setForegroundRole(QPalette::PlaceholderText);
    mLayout->addWidget(mEmptyLabel);

    mScrollArea->setWidget(mContainer);
    outerLayout->addWidget(mScrollArea);
}

#include "SequencerWindow.moc"
