#include "SlidersEditor.h"
#include "FileDialog.h"
#include "Singletons.h"
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSet>
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

SlidersEditor::SlidersEditor(QWidget *parent)
    : QFrame(parent)
    , mModel(Singletons::sessionModel())
    , mFileName(FileDialog::generateNextUntitledFileName(tr("Sliders")))
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

    mEmptyLabel = new QLabel(tr("Select a slider binding to edit."), mContainer);
    mEmptyLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    mLayout->addWidget(mEmptyLabel);
    mLayout->addStretch();

    mScrollArea->setWidget(mContainer);
    layout->addWidget(mScrollArea);

    connect(&mModel, &QAbstractItemModel::dataChanged, this,
        [this](const QModelIndex &, const QModelIndex &) {
            if (!mUpdating)
                rebuild();
        });
}

QList<QMetaObject::Connection> SlidersEditor::connectEditActions(
    const EditActions &actions)
{
    actions.undo->setEnabled(false);
    actions.redo->setEnabled(false);
    actions.cut->setEnabled(false);
    actions.copy->setEnabled(false);
    actions.paste->setEnabled(false);
    actions.delete_->setEnabled(false);
    actions.selectAll->setEnabled(false);
    actions.rename->setEnabled(false);
    actions.findReplace->setEnabled(false);

    actions.windowFileName->setText(fileName());
    actions.windowFileName->setEnabled(false);
    return {};
}

QString SlidersEditor::fileName() const
{
    return mFileName;
}

void SlidersEditor::setFileName(QString fileName)
{
    if (!fileName.isEmpty())
        mFileName = fileName;
}

bool SlidersEditor::load()
{
    return true;
}

bool SlidersEditor::save()
{
    return true;
}

void SlidersEditor::setModified() { }

void SlidersEditor::setSelection(const QModelIndexList &selection)
{
    mSelection = selection;
    rebuild();
}

void SlidersEditor::rebuild()
{
    mUpdating = true;
    clearLayout();
    mControls.clear();

    auto hasSliders = false;
    auto processed = QSet<ItemId>();

    for (const auto &index : mSelection) {
        const auto binding = mModel.item<Binding>(index);
        if (!binding)
            continue;
        if (binding->bindingType != Binding::BindingType::Uniform)
            continue;
        if (binding->editor != Binding::Editor::Slider)
            continue;
        if (processed.contains(binding->id))
            continue;
        processed.insert(binding->id);

        hasSliders = true;

        auto group = new QGroupBox(getBindingTitle(mModel, binding->id),
            mContainer);
        auto formLayout = new QFormLayout(group);
        formLayout->setContentsMargins(8, 8, 8, 8);

        auto values = binding->values;
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
                [this, bindingId = binding->id, valueIndex = i, spin](int v) {
                    const auto value = fromSliderValue(v);
                    if (!spin->hasFocus()) {
                        const QSignalBlocker blocker(spin);
                        spin->setValue(value);
                    }
                    updateBindingValue(bindingId, valueIndex, value);
                });
            connect(spin, &QDoubleSpinBox::valueChanged, this,
                [this, bindingId = binding->id, valueIndex = i, slider](
                    double value) {
                    const auto sliderValue = toSliderValue(value);
                    if (!slider->hasFocus()) {
                        const QSignalBlocker blocker(slider);
                        slider->setValue(sliderValue);
                    }
                    updateBindingValue(bindingId, valueIndex, value);
                });

            mControls.push_back(
                SliderControl{ binding->id, i, slider, spin });
        }

        mLayout->addWidget(group);
    }

    if (!hasSliders) {
        mEmptyLabel = new QLabel(tr("Select a slider binding to edit."),
            mContainer);
        mEmptyLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        mLayout->addWidget(mEmptyLabel);
    }

    mLayout->addStretch();
    mUpdating = false;
}

void SlidersEditor::clearLayout()
{
    while (auto item = mLayout->takeAt(0)) {
        if (auto widget = item->widget())
            widget->deleteLater();
        delete item;
    }
    mEmptyLabel = nullptr;
}

QString SlidersEditor::formatValue(double value) const
{
    static const auto regex = QRegularExpression("\\.?0+$");
    auto string = QString::number(value, 'f', 6);
    string.remove(regex);
    return string;
}

void SlidersEditor::updateBindingValue(ItemId bindingId, int valueIndex,
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
