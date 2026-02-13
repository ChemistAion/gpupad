#include "SlidersWindow.h"
#include "Singletons.h"
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace {
    constexpr double SliderMin = 0.0;
    constexpr double SliderMax = 1.0;
    constexpr double SliderStep = 0.01;
    constexpr int SliderResolution = 1000;

    int toSliderValue(double value)
    {
        const auto clamped = std::clamp(value, SliderMin, SliderMax);
        const auto t = (clamped - SliderMin) / (SliderMax - SliderMin);
        return static_cast<int>(std::round(t * SliderResolution));
    }

    double fromSliderValue(int value)
    {
        const auto t = static_cast<double>(value) / SliderResolution;
        return SliderMin + (SliderMax - SliderMin) * t;
    }

    QString getBindingTitle(const SessionModel &model, ItemId bindingId)
    {
        return model.getFullItemName(bindingId);
    }
} // namespace

SlidersWindow::SlidersWindow(QWidget *parent)
    : QFrame(parent)
    , mModel(Singletons::sessionModel())
{
    setFrameShape(QFrame::NoFrame);

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    mScrollArea = new QScrollArea(this);
    mScrollArea->setWidgetResizable(true);

    mContainer = new QWidget(mScrollArea);
    mLayout = new QVBoxLayout(mContainer);
    mLayout->setContentsMargins(8, 8, 8, 8);
    mLayout->setSpacing(8);

    mEmptyLabel = new QLabel(tr("No slider bindings in session."), mContainer);
    mEmptyLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    mLayout->addWidget(mEmptyLabel);
    mLayout->addStretch();

    mScrollArea->setWidget(mContainer);
    layout->addWidget(mScrollArea);

    const auto rebuildGuarded = [this]() {
        if (!mUpdating)
            rebuild();
    };
    connect(&mModel, &QAbstractItemModel::dataChanged, this, rebuildGuarded);
    connect(&mModel, &QAbstractItemModel::rowsInserted, this, rebuildGuarded);
    connect(&mModel, &QAbstractItemModel::rowsRemoved, this, rebuildGuarded);
}

bool SlidersWindow::hasSliderBindings() const
{
    auto found = false;
    mModel.forEachItem<Binding>([&](const Binding &binding) {
        if (binding.bindingType != Binding::BindingType::Uniform)
            return;
        if (!binding.sliders)
            return;
        if (binding.editor == Binding::Editor::Color)
            return;
        found = true;
    });
    return found;
}

void SlidersWindow::rebuild()
{
    mUpdating = true;
    clearLayout();
    mControls.clear();

    auto hasSliders = false;

    mModel.forEachItem<Binding>([&](const Binding &binding) {
        if (binding.bindingType != Binding::BindingType::Uniform)
            return;
        if (!binding.sliders)
            return;
        if (binding.editor == Binding::Editor::Color)
            return;

        hasSliders = true;
        auto group = new QGroupBox(getBindingTitle(mModel, binding.id),
            mContainer);
        auto formLayout = new QFormLayout(group);
        formLayout->setContentsMargins(8, 8, 8, 8);

        auto values = binding.values;
        if (values.isEmpty())
            values.append("0");

        for (auto i = 0; i < values.size(); ++i) {
            auto rowWidget = new QWidget(group);
            auto rowLayout = new QHBoxLayout(rowWidget);
            rowLayout->setContentsMargins(0, 0, 0, 0);

            auto slider = new QSlider(Qt::Horizontal, rowWidget);
            slider->setRange(0, SliderResolution);
            slider->setSingleStep(1);

            auto spin = new QDoubleSpinBox(rowWidget);
            spin->setRange(SliderMin, SliderMax);
            spin->setSingleStep(SliderStep);
            spin->setDecimals(4);

            auto ok = false;
            const auto value = values[i].toDouble(&ok);
            if (ok) {
                slider->setValue(toSliderValue(value));
                spin->setValue(value);
            } else {
                slider->setEnabled(false);
                spin->setEnabled(false);
            }

            rowLayout->addWidget(slider, 1);
            rowLayout->addWidget(spin);

            formLayout->addRow(tr("Value %1").arg(i + 1), rowWidget);

            connect(slider, &QSlider::valueChanged, this,
                [this, bindingId = binding.id, valueIndex = i, spin](int v) {
                    const auto value = fromSliderValue(v);
                    if (!spin->hasFocus()) {
                        const QSignalBlocker blocker(spin);
                        spin->setValue(value);
                    }
                    updateBindingValue(bindingId, valueIndex, value);
                });
            connect(spin, &QDoubleSpinBox::valueChanged, this,
                [this, bindingId = binding.id, valueIndex = i, slider](
                    double value) {
                    const auto sliderValue = toSliderValue(value);
                    if (!slider->hasFocus()) {
                        const QSignalBlocker blocker(slider);
                        slider->setValue(sliderValue);
                    }
                    updateBindingValue(bindingId, valueIndex, value);
                });

            mControls.push_back(
                SliderControl{ binding.id, i, slider, spin });
        }

        mLayout->addWidget(group);
    });

    if (!hasSliders) {
        mEmptyLabel = new QLabel(tr("No slider bindings in session."),
            mContainer);
        mEmptyLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        mLayout->addWidget(mEmptyLabel);
    }

    mLayout->addStretch();
    mUpdating = false;
}

void SlidersWindow::clearLayout()
{
    while (auto item = mLayout->takeAt(0)) {
        if (auto widget = item->widget())
            widget->deleteLater();
        delete item;
    }
    mEmptyLabel = nullptr;
}

QString SlidersWindow::formatValue(double value) const
{
    static const auto regex = QRegularExpression("\\.?0+$");
    auto string = QString::number(value, 'f', 6);
    string.remove(regex);
    return string;
}

void SlidersWindow::updateBindingValue(ItemId bindingId, int valueIndex,
    double value)
{
    if (mUpdating)
        return;

    auto binding = mModel.findItem<Binding>(bindingId);
    if (!binding)
        return;

    auto values = binding->values;
    while (values.size() <= valueIndex)
        values.append("0");

    const auto newValue = formatValue(value);
    if (values[valueIndex] == newValue)
        return;

    values[valueIndex] = newValue;
    mUpdating = true;
    mModel.setData(mModel.getIndex(binding, SessionModel::BindingValues),
        values);
    mUpdating = false;
}
