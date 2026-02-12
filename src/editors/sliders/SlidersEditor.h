#pragma once

#include "editors/IEditor.h"
#include "session/SessionModel.h"
#include <QFrame>
#include <QModelIndexList>
#include <QVector>

class QScrollArea;
class QVBoxLayout;
class QLabel;
class QSlider;
class QDoubleSpinBox;

class SlidersEditor final : public QFrame, public IEditor
{
    Q_OBJECT

public:
    explicit SlidersEditor(QWidget *parent = nullptr);

    QList<QMetaObject::Connection> connectEditActions(
        const EditActions &actions) override;
    QString fileName() const override;
    void setFileName(QString fileName) override;
    bool load() override;
    bool save() override;
    void setModified() override;
    int tabifyGroup() const override { return 1; }

    void setSelection(const QModelIndexList &selection);

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
    QString mFileName;
    QScrollArea *mScrollArea{};
    QWidget *mContainer{};
    QVBoxLayout *mLayout{};
    QLabel *mEmptyLabel{};
    QModelIndexList mSelection;
    QVector<SliderControl> mControls;
    bool mUpdating{};
};
