#pragma once

#include "session/SessionModel.h"
#include <QFrame>
#include <QModelIndexList>
#include <QVector>

class QScrollArea;
class QVBoxLayout;
class QLabel;
class QSlider;
class QDoubleSpinBox;

class SlidersWindow final : public QFrame
{
    Q_OBJECT

public:
    explicit SlidersWindow(QWidget *parent = nullptr);

    void setSelection(const QModelIndexList &selection);
    bool hasSliderBindings() const;

private:
    struct SliderControl
    {
        ItemId bindingId{};
        int valueIndex{};
        QSlider *slider{};
        QDoubleSpinBox *spin{};
    };

    void rebuild();
    void clearLayout();
    QString formatValue(double value) const;
    void updateBindingValue(ItemId bindingId, int valueIndex, double value);

    SessionModel &mModel;
    QScrollArea *mScrollArea{};
    QWidget *mContainer{};
    QVBoxLayout *mLayout{};
    QLabel *mEmptyLabel{};
    QModelIndexList mSelection;
    QVector<SliderControl> mControls;
    bool mUpdating{};
};
