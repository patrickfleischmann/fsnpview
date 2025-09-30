#include "plotsettingsdialog.h"

#include <QDialogButtonBox>
#include <QDoubleValidator>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
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
{
    setWindowTitle(tr("Plot Settings"));
    setModal(true);

    auto *formLayout = new QFormLayout;
    formLayout->addRow(m_xMinLabel, m_xMinEdit);
    formLayout->addRow(m_xMaxLabel, m_xMaxEdit);
    formLayout->addRow(m_yMinLabel, m_yMinEdit);
    formLayout->addRow(m_yMaxLabel, m_yMaxEdit);
    formLayout->addRow(m_markerALabel, m_markerAEdit);
    formLayout->addRow(m_markerBLabel, m_markerBEdit);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(formLayout);
    layout->addWidget(buttonBox);

    setLayout(layout);

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
