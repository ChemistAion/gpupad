#pragma once

#include <QFrame>

class QScrollArea;
class QVBoxLayout;
class QLabel;

class SequencerWindow final : public QFrame
{
    Q_OBJECT

public:
    explicit SequencerWindow(QWidget *parent = nullptr);

private:
    QScrollArea *mScrollArea{};
    QWidget *mContainer{};
    QVBoxLayout *mLayout{};
    QLabel *mEmptyLabel{};
};
