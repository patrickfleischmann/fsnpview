#include "parameterstyledialog.h"

#include "network.h"

#include <QComboBox>
#include <QCursor>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QColorDialog>
#include <QList>
#include <QObject>
#include <QVariant>

namespace
{
    struct PenStyleOption
    {
        QString label;
        Qt::PenStyle style;
    };

    QList<PenStyleOption> availablePenStyles()
    {
        return {
            {QObject::tr("Solid"), Qt::SolidLine},
            {QObject::tr("Dashed"), Qt::DashLine},
            {QObject::tr("Dotted"), Qt::DotLine},
            {QObject::tr("Dash-Dot"), Qt::DashDotLine},
            {QObject::tr("Dash-Dot-Dot"), Qt::DashDotDotLine}
        };
    }
}

ParameterStyleDialog::ParameterStyleDialog(Network* network, QWidget* parent)
    : QDialog(parent)
    , m_network(network)
{
    setWindowTitle(tr("Select Color and Style"));
    setModal(true);

    if (m_network)
        m_parameters = m_network->parameterNames();

    m_parameterCombo = new QComboBox(this);
    m_parameterCombo->setObjectName(QStringLiteral("parameterCombo"));
    m_parameterCombo->addItem(tr("All Parameters"), QString());
    for (const QString& parameter : m_parameters)
        m_parameterCombo->addItem(parameter, parameter);

    m_widthCombo = new QComboBox(this);
    m_widthCombo->setObjectName(QStringLiteral("widthCombo"));
    for (int i = 0; i <= 10; ++i)
    {
        const QString label = QString::number(i);
        m_widthCombo->addItem(label);
        const int index = m_widthCombo->count() - 1;
        m_widthCombo->setItemData(index, i, Qt::UserRole);
    }

    m_styleCombo = new QComboBox(this);
    m_styleCombo->setObjectName(QStringLiteral("styleCombo"));
    for (const PenStyleOption& option : availablePenStyles())
        m_styleCombo->addItem(option.label, static_cast<int>(option.style));

    m_colorPreview = new QFrame(this);
    m_colorPreview->setFrameShape(QFrame::Box);
    m_colorPreview->setMinimumSize(32, 20);
    m_colorPreview->setMaximumHeight(24);
    m_colorPreview->setCursor(Qt::PointingHandCursor);
    m_colorPreview->setToolTip(tr("Click to choose a color"));
    m_colorPreview->installEventFilter(this);

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    auto colorLayout = new QHBoxLayout;
    colorLayout->addWidget(m_colorPreview);
    colorLayout->addStretch(1);

    auto formLayout = new QFormLayout;
    formLayout->addRow(tr("Parameter"), m_parameterCombo);
    formLayout->addRow(tr("Color"), colorLayout);
    formLayout->addRow(tr("Line width"), m_widthCombo);
    formLayout->addRow(tr("Line style"), m_styleCombo);

    auto mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(m_buttonBox);

    connect(m_parameterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ParameterStyleDialog::parameterChanged);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &ParameterStyleDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &ParameterStyleDialog::reject);

    if (m_network)
        m_color = m_network->color();
    else
        m_color = Qt::black;

    updateColorPreview();
    parameterChanged(m_parameterCombo->currentIndex());
}

QString ParameterStyleDialog::selectedParameter() const
{
    return m_parameterCombo->currentData().toString();
}

QColor ParameterStyleDialog::selectedColor() const
{
    return m_color;
}

int ParameterStyleDialog::selectedWidth() const
{
    const QVariant widthData = m_widthCombo->currentData();
    if (widthData.isValid())
        return widthData.toInt();

    bool ok = false;
    const int fromText = m_widthCombo->currentText().toInt(&ok);
    return ok ? fromText : 0;
}

Qt::PenStyle ParameterStyleDialog::selectedStyle() const
{
    return static_cast<Qt::PenStyle>(m_styleCombo->currentData().toInt());
}

bool ParameterStyleDialog::applyToAllParameters() const
{
    return selectedParameter().isEmpty();
}

void ParameterStyleDialog::chooseColor()
{
    QColor chosen = QColorDialog::getColor(m_color.isValid() ? m_color : Qt::white,
                                           this, tr("Select Color"));
    if (!chosen.isValid())
        return;
    m_color = chosen;
    updateColorPreview();
}

void ParameterStyleDialog::parameterChanged(int index)
{
    Q_UNUSED(index);
    if (!m_network)
        return;

    const QString parameterKey = m_parameterCombo->currentData().toString();
    updateControlsForParameter(parameterKey);
}

bool ParameterStyleDialog::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_colorPreview && event)
    {
        if (event->type() == QEvent::MouseButtonRelease)
        {
            auto mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton)
            {
                chooseColor();
                return true;
            }
        }
    }

    return QDialog::eventFilter(watched, event);
}

void ParameterStyleDialog::updateControlsForParameter(const QString& parameterKey)
{
    if (!m_network)
        return;

    if (parameterKey.isEmpty())
    {
        m_color = m_network->color();

        int uniformWidth = 0;
        bool widthInitialized = false;
        bool widthUniform = true;

        Qt::PenStyle uniformStyle = Qt::SolidLine;
        bool styleInitialized = false;
        bool styleUniform = true;

        for (const QString& param : m_parameters)
        {
            int width = m_network->parameterWidth(param);
            Qt::PenStyle style = m_network->parameterStyle(param);

            if (!widthInitialized)
            {
                uniformWidth = width;
                widthInitialized = true;
            }
            else if (uniformWidth != width)
            {
                widthUniform = false;
            }

            if (!styleInitialized)
            {
                uniformStyle = style;
                styleInitialized = true;
            }
            else if (uniformStyle != style)
            {
                styleUniform = false;
            }
        }

        int widthValue = widthUniform ? uniformWidth : 0;
        int widthIndex = m_widthCombo->findData(widthValue);
        if (widthIndex < 0)
            widthIndex = 0;
        m_widthCombo->setCurrentIndex(widthIndex);

        Qt::PenStyle styleValue = styleUniform ? uniformStyle : Qt::SolidLine;
        int styleIndex = m_styleCombo->findData(static_cast<int>(styleValue));
        if (styleIndex < 0)
            styleIndex = 0;
        m_styleCombo->setCurrentIndex(styleIndex);

        updateColorPreview();
        return;
    }

    m_color = m_network->parameterColor(parameterKey);
    updateColorPreview();

    int width = m_network->parameterWidth(parameterKey);
    int widthIndex = m_widthCombo->findData(width);
    if (widthIndex < 0)
        widthIndex = 0;
    m_widthCombo->setCurrentIndex(widthIndex);

    Qt::PenStyle style = m_network->parameterStyle(parameterKey);
    int styleIndex = m_styleCombo->findData(static_cast<int>(style));
    if (styleIndex < 0)
        styleIndex = 0;
    m_styleCombo->setCurrentIndex(styleIndex);
}

void ParameterStyleDialog::updateColorPreview()
{
    QString colorName = m_color.isValid() ? m_color.name(QColor::HexRgb)
                                          : QStringLiteral("transparent");
    m_colorPreview->setStyleSheet(QStringLiteral("background-color: %1; border: 1px solid palette(mid);")
                                  .arg(colorName));
}
