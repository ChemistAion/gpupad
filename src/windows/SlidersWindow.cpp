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
    constexpr int SliderResolution = 1000;

    int toSliderValue(double value, double rangeMin, double rangeMax)
    {
        const auto clamped = std::clamp(value, rangeMin, rangeMax);
        const auto t = (clamped - rangeMin) / (rangeMax - rangeMin);
        return static_cast<int>(std::round(t * SliderResolution));
    }

    double fromSliderValue(int value, double rangeMin, double rangeMax)
    {
        const auto t = static_cast<double>(value) / SliderResolution;
        return rangeMin + (rangeMax - rangeMin) * t;
    }

    std::pair<double, double> computeRange(const QStringList &values)
    {
        auto lo = 0.0;
        auto hi = 1.0;
        for (const auto &v : values) {
            auto ok = false;
            const auto d = v.toDouble(&ok);
            if (ok) {
                lo = std::min(lo, d);
                hi = std::max(hi, d);
            }
        }
        // add headroom so the slider isn't pinned at the extremes
        const auto margin = std::max(0.5, (hi - lo) * 0.25);
        return { lo - margin, hi + margin };
    }

    QString getBindingTitle(const SessionModel &model, ItemId bindingId)
    {
        return model.getItemName(bindingId);
    }

    QString getValueLabel(Binding::Editor editor, int index)
    {
        switch (editor) {
        case Binding::Editor::Expression:
            return QStringLiteral("Value");

        case Binding::Editor::Expression2:
        case Binding::Editor::Expression3:
        case Binding::Editor::Expression4:
            return QStringLiteral("Value[%1]").arg(index);

        case Binding::Editor::Expression2x2:
        case Binding::Editor::Expression2x3:
        case Binding::Editor::Expression2x4:
        case Binding::Editor::Expression3x2:
        case Binding::Editor::Expression3x3:
        case Binding::Editor::Expression3x4:
        case Binding::Editor::Expression4x2:
        case Binding::Editor::Expression4x3:
        case Binding::Editor::Expression4x4: {
            // AxB = A columns x B rows, column-major storage
            auto cols = 0;
            auto rows = 0;
            switch (editor) {
            case Binding::Editor::Expression2x2: cols = 2; rows = 2; break;
            case Binding::Editor::Expression2x3: cols = 2; rows = 3; break;
            case Binding::Editor::Expression2x4: cols = 2; rows = 4; break;
            case Binding::Editor::Expression3x2: cols = 3; rows = 2; break;
            case Binding::Editor::Expression3x3: cols = 3; rows = 3; break;
            case Binding::Editor::Expression3x4: cols = 3; rows = 4; break;
            case Binding::Editor::Expression4x2: cols = 4; rows = 2; break;
            case Binding::Editor::Expression4x3: cols = 4; rows = 3; break;
            case Binding::Editor::Expression4x4: cols = 4; rows = 4; break;
            default: break;
            }
            const auto col = index / rows;
            const auto row = index % rows;
            return QStringLiteral("Value[%1][%2]").arg(col).arg(row);
        }

        default:
            return QStringLiteral("Value[%1]").arg(index);
        }
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

        const auto [rangeMin, rangeMax] = computeRange(values);
        const auto step = (rangeMax - rangeMin) / SliderResolution;

        for (auto i = 0; i < values.size(); ++i) {
            auto rowWidget = new QWidget(group);
            auto rowLayout = new QHBoxLayout(rowWidget);
            rowLayout->setContentsMargins(0, 0, 0, 0);

            auto slider = new QSlider(Qt::Horizontal, rowWidget);
            slider->setRange(0, SliderResolution);
            slider->setSingleStep(1);

            auto spin = new QDoubleSpinBox(rowWidget);
            spin->setRange(rangeMin, rangeMax);
            spin->setSingleStep(step);
            spin->setDecimals(4);

            auto ok = false;
            const auto value = values[i].toDouble(&ok);
            if (ok) {
                slider->setValue(toSliderValue(value, rangeMin, rangeMax));
                spin->setValue(value);
            } else {
                slider->setEnabled(false);
                spin->setEnabled(false);
            }

            rowLayout->addWidget(slider, 1);
            rowLayout->addWidget(spin);

            formLayout->addRow(getValueLabel(binding.editor, i), rowWidget);

            connect(slider, &QSlider::valueChanged, this,
                [this, bindingId = binding.id, valueIndex = i, spin,
                    rangeMin, rangeMax](int v) {
                    const auto value = fromSliderValue(v, rangeMin, rangeMax);
                    if (!spin->hasFocus()) {
                        const QSignalBlocker blocker(spin);
                        spin->setValue(value);
                    }
                    updateBindingValue(bindingId, valueIndex, value);
                });
            connect(spin, &QDoubleSpinBox::valueChanged, this,
                [this, bindingId = binding.id, valueIndex = i, slider,
                    rangeMin, rangeMax](double value) {
                    const auto sliderValue =
                        toSliderValue(value, rangeMin, rangeMax);
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
