#pragma once

#include "session/SessionModel.h"
#include <QFrame>
#include <QVector>

class QScrollArea;
class QVBoxLayout;
class QLabel;
class QTimer;

class SequencerWindow final : public QFrame
{
    Q_OBJECT

public:
    explicit SequencerWindow(QWidget *parent = nullptr);

    void rebuild();

    struct TrackInfo
    {
        ItemId bindingId{};
        QString name;
    };

private:
    void clearLayout();

    SessionModel &mModel;
    QScrollArea *mScrollArea{};
    QWidget *mContainer{};
    QVBoxLayout *mLayout{};
    QWidget *mTimeline{};
    QLabel *mTimerLabel{};
    QLabel *mEmptyLabel{};
    QVector<TrackInfo> mTracks;
    QTimer *mUITimer{};
    bool mUpdating{};
};

