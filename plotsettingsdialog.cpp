#include "plotsettingsdialog.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QDoubleValidator>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QComboBox>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>

#include <cmath>
#include <limits>

#include "network.h"

namespace {

QLineEdit *makeLineEdit(QWidget *parent)
{
    auto *edit = new QLineEdit(parent);
    auto *validator = new QDoubleValidator(edit);
    validator->setNotation(QDoubleValidator::ScientificNotation);
    validator->setLocale(QLocale::c());
    validator->setBottom(-std::numeric_limits<double>::max());
    validator->setTop(std::numeric_limits<double>::max());
    edit->setValidator(validator);
    edit->setAlignment(Qt::AlignRight);
    return edit;
}

QString makeAxisLabelText(const QString &base, const QString &axisLabel)
{
    if (axisLabel.isEmpty())
        return base;
    return QStringLiteral("%1 (%2)").arg(base, axisLabel);
}

} // namespace

PlotSettingsDialog::PlotSettingsDialog(QWidget *parent)
    : QDialog(parent)
    , m_xMinLabel(new QLabel(this))
    , m_xMaxLabel(new QLabel(this))
    , m_yMinLabel(new QLabel(this))
    , m_yMaxLabel(new QLabel(this))
    , m_markerALabel(new QLabel(this))
    , m_markerBLabel(new QLabel(this))
    , m_xMinEdit(makeLineEdit(this))
    , m_xMaxEdit(makeLineEdit(this))
    , m_yMinEdit(makeLineEdit(this))
    , m_yMaxEdit(makeLineEdit(this))
    , m_markerAEdit(makeLineEdit(this))
    , m_markerBEdit(makeLineEdit(this))
    , m_gridStyleCombo(new QComboBox(this))
    , m_gridColorButton(new QToolButton(this))
    , m_gridColor(Qt::gray)
    , m_subGridStyleCombo(new QComboBox(this))
    , m_subGridColorButton(new QToolButton(this))
    , m_subGridColor(Qt::gray)
    , m_xTickSpacingEdit(makeLineEdit(this))
    , m_xTickAutoCheck(new QCheckBox(tr("Auto"), this))
    , m_xTickManualText()
    , m_xTickAutoValue(0.0)
    , m_xTickManualEnabled(true)
    , m_yTickSpacingEdit(makeLineEdit(this))
    , m_yTickAutoCheck(new QCheckBox(tr("Auto"), this))
    , m_yTickManualText()
    , m_yTickAutoValue(0.0)
    , m_yTickManualEnabled(true)
{
    setWindowTitle(tr("Plot Settings"));
    setModal(true);

    populatePenStyleCombo(m_gridStyleCombo);
    populatePenStyleCombo(m_subGridStyleCombo);

    m_gridColorButton->setAutoRaise(true);
    m_gridColorButton->setToolTip(tr("Select grid color"));
    connect(m_gridColorButton, &QToolButton::clicked, this, &PlotSettingsDialog::pickGridColor);

    m_subGridColorButton->setAutoRaise(true);
    m_subGridColorButton->setToolTip(tr("Select subgrid color"));
    connect(m_subGridColorButton, &QToolButton::clicked, this, &PlotSettingsDialog::pickSubGridColor);

    updateColorIcon(m_gridColorButton, m_gridColor);
    updateColorIcon(m_subGridColorButton, m_subGridColor);

    auto *formLayout = new QFormLayout;
    formLayout->addRow(m_xMinLabel, m_xMinEdit);
    formLayout->addRow(m_xMaxLabel, m_xMaxEdit);
    formLayout->addRow(m_yMinLabel, m_yMinEdit);
    formLayout->addRow(m_yMaxLabel, m_yMaxEdit);
    formLayout->addRow(m_markerALabel, m_markerAEdit);
    formLayout->addRow(m_markerBLabel, m_markerBEdit);

    auto *gridWidget = new QWidget(this);
    auto *gridLayout = new QHBoxLayout(gridWidget);
    gridLayout->setContentsMargins(0, 0, 0, 0);
    gridLayout->addWidget(m_gridStyleCombo, /*stretch=*/1);
    gridLayout->addWidget(m_gridColorButton, /*stretch=*/0);
    formLayout->addRow(tr("Grid"), gridWidget);

    auto *subGridWidget = new QWidget(this);
    auto *subGridLayout = new QHBoxLayout(subGridWidget);
    subGridLayout->setContentsMargins(0, 0, 0, 0);
    subGridLayout->addWidget(m_subGridStyleCombo, /*stretch=*/1);
    subGridLayout->addWidget(m_subGridColorButton, /*stretch=*/0);
    formLayout->addRow(tr("Subgrid"), subGridWidget);

    auto *xTickWidget = new QWidget(this);
    auto *xTickLayout = new QHBoxLayout(xTickWidget);
    xTickLayout->setContentsMargins(0, 0, 0, 0);
    xTickLayout->addWidget(m_xTickSpacingEdit, /*stretch=*/1);
    xTickLayout->addWidget(m_xTickAutoCheck, /*stretch=*/0);
    formLayout->addRow(tr("X tick spacing"), xTickWidget);

    auto *yTickWidget = new QWidget(this);
    auto *yTickLayout = new QHBoxLayout(yTickWidget);
    yTickLayout->setContentsMargins(0, 0, 0, 0);
    yTickLayout->addWidget(m_yTickSpacingEdit, /*stretch=*/1);
    yTickLayout->addWidget(m_yTickAutoCheck, /*stretch=*/0);
    formLayout->addRow(tr("Y tick spacing"), yTickWidget);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(formLayout);
    layout->addWidget(buttonBox);

    setLayout(layout);

    connect(m_xTickAutoCheck, &QCheckBox::toggled, this, &PlotSettingsDialog::handleXAxisAutoToggled);
    connect(m_yTickAutoCheck, &QCheckBox::toggled, this, &PlotSettingsDialog::handleYAxisAutoToggled);

    setAxisLabels(QString(), QString());
    setMarkerLabelTexts(tr("Marker A"), tr("Marker B"));
}

void PlotSettingsDialog::setAxisLabels(const QString &xLabel, const QString &yLabel)
{
    m_xMinLabel->setText(makeAxisLabelText(tr("X axis minimum"), xLabel));
    m_xMaxLabel->setText(makeAxisLabelText(tr("X axis maximum"), xLabel));
    m_yMinLabel->setText(makeAxisLabelText(tr("Y axis minimum"), yLabel));
    m_yMaxLabel->setText(makeAxisLabelText(tr("Y axis maximum"), yLabel));
}

void PlotSettingsDialog::setMarkerLabelTexts(const QString &labelA, const QString &labelB)
{
    m_markerALabel->setText(labelA);
    m_markerBLabel->setText(labelB);
}

void PlotSettingsDialog::setXAxisRange(double minimum, double maximum)
{
    m_xMinEdit->setText(formatValue(minimum));
    m_xMaxEdit->setText(formatValue(maximum));
}

void PlotSettingsDialog::setYAxisRange(double minimum, double maximum)
{
    m_yMinEdit->setText(formatValue(minimum));
    m_yMaxEdit->setText(formatValue(maximum));
}

void PlotSettingsDialog::setMarkerValues(double markerA, bool markerAEnabled,
                                         double markerB, bool markerBEnabled)
{
    if (std::isfinite(markerA))
        m_markerAEdit->setText(formatValue(markerA));
    else
        m_markerAEdit->clear();
    if (std::isfinite(markerB))
        m_markerBEdit->setText(formatValue(markerB));
    else
        m_markerBEdit->clear();

    m_markerAEdit->setEnabled(markerAEnabled);
    m_markerBEdit->setEnabled(markerBEnabled);
    m_markerALabel->setEnabled(markerAEnabled);
    m_markerBLabel->setEnabled(markerBEnabled);
}

void PlotSettingsDialog::setGridSettings(const QPen &gridPen, bool gridVisible,
                                         const QPen &subGridPen, bool subGridVisible)
{
    Qt::PenStyle gridStyle = gridVisible ? gridPen.style() : Qt::NoPen;
    if (!gridVisible)
        gridStyle = Qt::NoPen;
    if (gridStyle == Qt::NoPen && gridVisible)
        gridStyle = Qt::SolidLine;

    Qt::PenStyle subGridStyle = subGridVisible ? subGridPen.style() : Qt::NoPen;
    if (!subGridVisible)
        subGridStyle = Qt::NoPen;
    if (subGridStyle == Qt::NoPen && subGridVisible)
        subGridStyle = Qt::DotLine;

    m_gridColor = gridPen.color().isValid() ? gridPen.color() : QColor(Qt::gray);
    m_subGridColor = subGridPen.color().isValid() ? subGridPen.color() : QColor(Qt::gray);

    {
        QSignalBlocker blocker(m_gridStyleCombo);
        int index = indexForPenStyle(m_gridStyleCombo, gridStyle);
        if (index < 0)
            index = indexForPenStyle(m_gridStyleCombo, Qt::DotLine);
        if (index >= 0)
            m_gridStyleCombo->setCurrentIndex(index);
    }
    {
        QSignalBlocker blocker(m_subGridStyleCombo);
        int index = indexForPenStyle(m_subGridStyleCombo, subGridStyle);
        if (index < 0)
            index = indexForPenStyle(m_subGridStyleCombo, Qt::DotLine);
        if (index >= 0)
            m_subGridStyleCombo->setCurrentIndex(index);
    }

    updateColorIcon(m_gridColorButton, m_gridColor);
    updateColorIcon(m_subGridColorButton, m_subGridColor);
}

Qt::PenStyle PlotSettingsDialog::gridPenStyle() const
{
    Qt::PenStyle style = penStyleFromCombo(m_gridStyleCombo);
    return style;
}

QColor PlotSettingsDialog::gridColor() const
{
    return m_gridColor;
}

Qt::PenStyle PlotSettingsDialog::subGridPenStyle() const
{
    return penStyleFromCombo(m_subGridStyleCombo);
}

QColor PlotSettingsDialog::subGridColor() const
{
    return m_subGridColor;
}

void PlotSettingsDialog::setXAxisTickSpacing(double manualSpacing, bool automatic,
                                             double autoSpacingDisplayValue, bool manualControlsEnabled)
{
    m_xTickAutoValue = autoSpacingDisplayValue;
    m_xTickManualEnabled = manualControlsEnabled;
    if (std::isfinite(manualSpacing) && manualSpacing > 0.0)
        m_xTickManualText = formatValue(manualSpacing);
    else
        m_xTickManualText.clear();

    {
        QSignalBlocker blocker(m_xTickAutoCheck);
        bool shouldCheck = automatic || !manualControlsEnabled;
        m_xTickAutoCheck->setChecked(shouldCheck);
    }
    m_xTickAutoCheck->setEnabled(manualControlsEnabled);
    updateTickSpacingEdit(m_xTickSpacingEdit, m_xTickAutoCheck->isChecked(), manualControlsEnabled,
                          m_xTickManualText, m_xTickAutoValue);
}

void PlotSettingsDialog::setYAxisTickSpacing(double manualSpacing, bool automatic,
                                             double autoSpacingDisplayValue, bool manualControlsEnabled)
{
    m_yTickAutoValue = autoSpacingDisplayValue;
    m_yTickManualEnabled = manualControlsEnabled;
    if (std::isfinite(manualSpacing) && manualSpacing > 0.0)
        m_yTickManualText = formatValue(manualSpacing);
    else
        m_yTickManualText.clear();

    {
        QSignalBlocker blocker(m_yTickAutoCheck);
        bool shouldCheck = automatic || !manualControlsEnabled;
        m_yTickAutoCheck->setChecked(shouldCheck);
    }
    m_yTickAutoCheck->setEnabled(manualControlsEnabled);
    updateTickSpacingEdit(m_yTickSpacingEdit, m_yTickAutoCheck->isChecked(), manualControlsEnabled,
                          m_yTickManualText, m_yTickAutoValue);
}

bool PlotSettingsDialog::xTickSpacingIsAutomatic() const
{
    if (!m_xTickManualEnabled)
        return true;
    return m_xTickAutoCheck->isChecked();
}

bool PlotSettingsDialog::yTickSpacingIsAutomatic() const
{
    if (!m_yTickManualEnabled)
        return true;
    return m_yTickAutoCheck->isChecked();
}

double PlotSettingsDialog::xTickSpacing() const
{
    return parseValue(m_xTickSpacingEdit, std::numeric_limits<double>::quiet_NaN());
}

double PlotSettingsDialog::yTickSpacing() const
{
    return parseValue(m_yTickSpacingEdit, std::numeric_limits<double>::quiet_NaN());
}

double PlotSettingsDialog::xMinimum() const
{
    return parseValue(m_xMinEdit, 0.0);
}

double PlotSettingsDialog::xMaximum() const
{
    return parseValue(m_xMaxEdit, 0.0);
}

double PlotSettingsDialog::yMinimum() const
{
    return parseValue(m_yMinEdit, 0.0);
}

double PlotSettingsDialog::yMaximum() const
{
    return parseValue(m_yMaxEdit, 0.0);
}

double PlotSettingsDialog::markerAValue() const
{
    return parseValue(m_markerAEdit, 0.0);
}

double PlotSettingsDialog::markerBValue() const
{
    return parseValue(m_markerBEdit, 0.0);
}

bool PlotSettingsDialog::markerAIsEnabled() const
{
    return m_markerAEdit->isEnabled();
}

bool PlotSettingsDialog::markerBIsEnabled() const
{
    return m_markerBEdit->isEnabled();
}

QString PlotSettingsDialog::formatValue(double value)
{
    if (!std::isfinite(value))
        return QString();
    return Network::formatEngineering(value);
}

double PlotSettingsDialog::parseValue(const QLineEdit *edit, double fallback)
{
    if (!edit || !edit->isEnabled())
        return fallback;
    bool ok = false;
    double value = edit->text().trimmed().toDouble(&ok);
    return ok ? value : fallback;
}

void PlotSettingsDialog::populatePenStyleCombo(QComboBox *combo)
{
    if (!combo)
        return;

    combo->clear();
    combo->addItem(QObject::tr("None"), static_cast<int>(Qt::NoPen));
    combo->addItem(QObject::tr("Solid"), static_cast<int>(Qt::SolidLine));
    combo->addItem(QObject::tr("Dash"), static_cast<int>(Qt::DashLine));
    combo->addItem(QObject::tr("Dot"), static_cast<int>(Qt::DotLine));
    combo->addItem(QObject::tr("Dash Dot"), static_cast<int>(Qt::DashDotLine));
    combo->addItem(QObject::tr("Dash Dot Dot"), static_cast<int>(Qt::DashDotDotLine));
}

int PlotSettingsDialog::indexForPenStyle(const QComboBox *combo, Qt::PenStyle style)
{
    if (!combo)
        return -1;
    for (int i = 0; i < combo->count(); ++i)
    {
        if (static_cast<Qt::PenStyle>(combo->itemData(i).toInt()) == style)
            return i;
    }
    return -1;
}

Qt::PenStyle PlotSettingsDialog::penStyleFromCombo(const QComboBox *combo)
{
    if (!combo || combo->currentIndex() < 0)
        return Qt::SolidLine;
    return static_cast<Qt::PenStyle>(combo->currentData().toInt());
}

void PlotSettingsDialog::updateColorIcon(QToolButton *button, const QColor &color)
{
    if (!button)
        return;

    QColor effectiveColor = color.isValid() ? color : QColor(Qt::gray);
    QPixmap pixmap(18, 18);
    pixmap.fill(effectiveColor);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(QPen(Qt::black));
    painter.drawRect(pixmap.rect().adjusted(0, 0, -1, -1));
    painter.end();
    button->setIcon(QIcon(pixmap));
    button->setIconSize(pixmap.size());
}

QString PlotSettingsDialog::effectiveManualText(const QString &manualText, double autoValue)
{
    if (!manualText.isEmpty())
        return manualText;
    return formatValue(autoValue);
}

void PlotSettingsDialog::updateTickSpacingEdit(QLineEdit *edit, bool isAuto, bool manualEnabled,
                                               const QString &manualText, double autoValue)
{
    if (!edit)
        return;

    if (!manualEnabled)
    {
        edit->setEnabled(false);
        edit->setText(formatValue(autoValue));
        return;
    }

    edit->setEnabled(!isAuto);
    if (isAuto)
    {
        edit->setText(formatValue(autoValue));
    }
    else
    {
        edit->setText(effectiveManualText(manualText, autoValue));
    }
}

void PlotSettingsDialog::handleXAxisAutoToggled(bool checked)
{
    if (!m_xTickManualEnabled)
    {
        updateTickSpacingEdit(m_xTickSpacingEdit, true, false, m_xTickManualText, m_xTickAutoValue);
        return;
    }
    if (checked)
        m_xTickManualText = m_xTickSpacingEdit->text();
    updateTickSpacingEdit(m_xTickSpacingEdit, checked, m_xTickManualEnabled,
                          m_xTickManualText, m_xTickAutoValue);
}

void PlotSettingsDialog::handleYAxisAutoToggled(bool checked)
{
    if (!m_yTickManualEnabled)
    {
        updateTickSpacingEdit(m_yTickSpacingEdit, true, false, m_yTickManualText, m_yTickAutoValue);
        return;
    }
    if (checked)
        m_yTickManualText = m_yTickSpacingEdit->text();
    updateTickSpacingEdit(m_yTickSpacingEdit, checked, m_yTickManualEnabled,
                          m_yTickManualText, m_yTickAutoValue);
}

void PlotSettingsDialog::pickGridColor()
{
    QColor chosen = QColorDialog::getColor(m_gridColor, this, tr("Select grid color"));
    if (!chosen.isValid())
        return;
    m_gridColor = chosen;
    updateColorIcon(m_gridColorButton, m_gridColor);
}

void PlotSettingsDialog::pickSubGridColor()
{
    QColor chosen = QColorDialog::getColor(m_subGridColor, this, tr("Select subgrid color"));
    if (!chosen.isValid())
        return;
    m_subGridColor = chosen;
    updateColorIcon(m_subGridColorButton, m_subGridColor);
}
