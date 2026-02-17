#include "SequencerWindow.h"
#include "Singletons.h"
#include "SynchronizeLogic.h"
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {

constexpr int RulerHeight = 24;
constexpr int TrackHeight = 28;
constexpr int LabelWidth = 120;

class TimelineWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit TimelineWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumHeight(80);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setMouseTracking(true);
    }

    void setTracks(const QVector<SequencerWindow::TrackInfo> &tracks)
    {
        mTracks = tracks;
        setMinimumHeight(RulerHeight + std::max(1, static_cast<int>(mTracks.size())) * TrackHeight);
        update();
    }

    void setPlayhead(double time)
    {
        if (!mDragging) {
            mPlayhead = time;
            update();
        }
    }

Q_SIGNALS:
    void playheadDragged(double time);

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const auto w = width();
        const auto h = height();
        const auto trackCount = std::max(1, static_cast<int>(mTracks.size()));

        // background
        p.fillRect(rect(), palette().base());

        // ruler background
        p.fillRect(LabelWidth, 0, w - LabelWidth, RulerHeight,
            palette().alternateBase());

        // ruler ticks
        p.setPen(palette().text().color());
        const auto tickSpacing = 60;
        for (auto x = LabelWidth; x < w; x += tickSpacing) {
            const auto sec = ((x - LabelWidth) * mDuration)
                / std::max(w - LabelWidth, 1);
            p.drawLine(x, RulerHeight - 6, x, RulerHeight);
            p.drawText(x + 3, RulerHeight - 8,
                QString::number(sec, 'f', 1) + "s");
        }

        // ruler bottom line
        p.setPen(QPen(palette().mid().color(), 1));
        p.drawLine(0, RulerHeight, w, RulerHeight);

        // track lanes
        for (auto i = 0; i < trackCount; ++i) {
            const auto y = RulerHeight + i * TrackHeight;
            if (i % 2 == 1)
                p.fillRect(0, y, w, TrackHeight,
                    palette().alternateBase());

            // track label
            if (i < mTracks.size()) {
                p.setPen(palette().text().color());
                const auto labelRect =
                    QRect(4, y, LabelWidth - 8, TrackHeight);
                p.drawText(labelRect,
                    Qt::AlignLeft | Qt::AlignVCenter,
                    mTracks[i].name);
            }

            // lane separator
            p.setPen(QPen(palette().mid().color(), 1));
            p.drawLine(0, y + TrackHeight, w, y + TrackHeight);
        }

        // label / timeline divider
        p.setPen(QPen(palette().mid().color(), 1));
        p.drawLine(LabelWidth, 0, LabelWidth, h);

        // playhead
        const auto timeW = std::max(w - LabelWidth, 1);
        const auto playX = LabelWidth
            + static_cast<int>((mPlayhead / mDuration) * timeW);
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
        mDragging = true;
        updatePlayhead(e->pos().x());
    }

    void mouseMoveEvent(QMouseEvent *e) override
    {
        if (e->buttons() & Qt::LeftButton)
            updatePlayhead(e->pos().x());
    }

    void mouseReleaseEvent(QMouseEvent *) override
    {
        mDragging = false;
    }

private:
    void updatePlayhead(int x)
    {
        const auto timeW = std::max(width() - LabelWidth, 1);
        mPlayhead = std::clamp(
            (static_cast<double>(x - LabelWidth) / timeW) * mDuration,
            0.0, mDuration);
        update();
        Q_EMIT playheadDragged(mPlayhead);
    }

    double mDuration{ 10.0 };
    double mPlayhead{ 0.0 };
    bool mDragging{};
    QVector<SequencerWindow::TrackInfo> mTracks;
};

} // namespace

SequencerWindow::SequencerWindow(QWidget *parent)
    : QFrame(parent)
    , mModel(Singletons::sessionModel())
{
    setFrameShape(QFrame::NoFrame);

    auto outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    mScrollArea = new QScrollArea(this);
    mScrollArea->setWidgetResizable(true);

    mContainer = new QWidget(mScrollArea);
    mLayout = new QVBoxLayout(mContainer);
    mLayout->setContentsMargins(0, 0, 0, 0);
    mLayout->setSpacing(0);

    mLayout->addStretch();

    mScrollArea->setWidget(mContainer);
    outerLayout->addWidget(mScrollArea);

    const auto rebuildGuarded = [this]() {
        if (!mUpdating)
            rebuild();
    };
    connect(&mModel, &QAbstractItemModel::dataChanged, this, rebuildGuarded);
    connect(&mModel, &QAbstractItemModel::rowsInserted, this, rebuildGuarded);
    connect(&mModel, &QAbstractItemModel::rowsRemoved, this, rebuildGuarded);

    auto &sync = Singletons::synchronizeLogic();
    connect(&sync, &SynchronizeLogic::timeChanged, this, [this](double t) {
        if (auto tl = static_cast<TimelineWidget *>(mTimeline))
            tl->setPlayhead(t);
    });
}

void SequencerWindow::rebuild()
{
    mUpdating = true;
    clearLayout();
    mTracks.clear();

    mModel.forEachItem<Binding>([&](const Binding &binding) {
        if (binding.bindingType != Binding::BindingType::Uniform)
            return;
        if (!binding.sliders)
            return;
        if (binding.editor == Binding::Editor::Color)
            return;

        mTracks.push_back(TrackInfo{
            binding.id, mModel.getItemName(binding.id) });
    });

    auto timeline = new TimelineWidget(mContainer);
    timeline->setTracks(mTracks);
    mTimeline = timeline;
    mLayout->insertWidget(0, timeline, 1);

    connect(timeline, &TimelineWidget::playheadDragged,
        this, [](double t) {
            Singletons::synchronizeLogic().setTime(t);
        });

    if (mTracks.isEmpty()) {
        mEmptyLabel = new QLabel(tr("No sequencer tracks in session."),
            mContainer);
        mEmptyLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        mLayout->insertWidget(1, mEmptyLabel);
    }

    mUpdating = false;
}

void SequencerWindow::clearLayout()
{
    while (auto item = mLayout->takeAt(0)) {
        if (auto widget = item->widget())
            widget->deleteLater();
        delete item;
    }
    mTimeline = nullptr;
    mEmptyLabel = nullptr;
    mLayout->addStretch();
}

#include "SequencerWindow.moc"

