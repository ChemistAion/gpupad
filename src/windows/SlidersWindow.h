#pragma once

#include "session/SessionModel.h"
#include <QFrame>
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

    void rebuild();
    bool hasSliderBindings() const;

private:
    struct SliderControl
    {
        ItemId bindingId{};
        int valueIndex{};
        QSlider *slider{};
        QDoubleSpinBox *spin{};
    };

    void clearLayout();
    QString formatValue(double value) const;
    void updateBindingValue(ItemId bindingId, int valueIndex, double value);

    SessionModel &mModel;
    QScrollArea *mScrollArea{};
    QWidget *mContainer{};
    QVBoxLayout *mLayout{};
    QLabel *mEmptyLabel{};
    QVector<SliderControl> mControls;
    bool mUpdating{};
};
