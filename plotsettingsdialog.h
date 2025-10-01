#ifndef PLOTSETTINGSDIALOG_H
#define PLOTSETTINGSDIALOG_H

#include <QDialog>
#include <QColor>
#include <QPen>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QLabel;
class QToolButton;

class PlotSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit PlotSettingsDialog(QWidget *parent = nullptr);

    void setAxisLabels(const QString &xLabel, const QString &yLabel);
    void setXAxisRange(double minimum, double maximum);
    void setYAxisRange(double minimum, double maximum);
    void setMarkerLabelTexts(const QString &labelA, const QString &labelB);
    void setMarkerValues(double markerA, bool markerAEnabled,
                         double markerB, bool markerBEnabled);

    void setGridSettings(const QPen &gridPen, bool gridVisible,
                         const QPen &subGridPen, bool subGridVisible);
    Qt::PenStyle gridPenStyle() const;
    QColor gridColor() const;
    Qt::PenStyle subGridPenStyle() const;
    QColor subGridColor() const;

    void setXAxisTickSpacing(double manualSpacing, bool automatic,
                             double autoSpacingDisplayValue, bool manualControlsEnabled);
    void setYAxisTickSpacing(double manualSpacing, bool automatic,
                             double autoSpacingDisplayValue, bool manualControlsEnabled);
    bool xTickSpacingIsAutomatic() const;
    bool yTickSpacingIsAutomatic() const;
    double xTickSpacing() const;
    double yTickSpacing() const;

    double xMinimum() const;
    double xMaximum() const;
    double yMinimum() const;
    double yMaximum() const;
    double markerAValue() const;
    double markerBValue() const;

    bool markerAIsEnabled() const;
    bool markerBIsEnabled() const;

private:
    static QString formatValue(double value);
    static double parseValue(const QLineEdit *edit, double fallback);
    static void populatePenStyleCombo(QComboBox *combo);
    static int indexForPenStyle(const QComboBox *combo, Qt::PenStyle style);
    static Qt::PenStyle penStyleFromCombo(const QComboBox *combo);
    static void updateColorIcon(QToolButton *button, const QColor &color);
    static QString effectiveManualText(const QString &manualText, double autoValue);
    void updateTickSpacingEdit(QLineEdit *edit, bool isAuto, bool manualEnabled,
                               const QString &manualText, double autoValue);
    void handleXAxisAutoToggled(bool checked);
    void handleYAxisAutoToggled(bool checked);
    void pickGridColor();
    void pickSubGridColor();

    QLabel *m_xMinLabel;
    QLabel *m_xMaxLabel;
    QLabel *m_yMinLabel;
    QLabel *m_yMaxLabel;
    QLabel *m_markerALabel;
    QLabel *m_markerBLabel;

    QLineEdit *m_xMinEdit;
    QLineEdit *m_xMaxEdit;
    QLineEdit *m_yMinEdit;
    QLineEdit *m_yMaxEdit;
    QLineEdit *m_markerAEdit;
    QLineEdit *m_markerBEdit;

    QComboBox *m_gridStyleCombo;
    QToolButton *m_gridColorButton;
    QColor m_gridColor;

    QComboBox *m_subGridStyleCombo;
    QToolButton *m_subGridColorButton;
    QColor m_subGridColor;

    QLineEdit *m_xTickSpacingEdit;
    QCheckBox *m_xTickAutoCheck;
    QString m_xTickManualText;
    double m_xTickAutoValue;
    bool m_xTickManualEnabled;

    QLineEdit *m_yTickSpacingEdit;
    QCheckBox *m_yTickAutoCheck;
    QString m_yTickManualText;
    double m_yTickAutoValue;
    bool m_yTickManualEnabled;
};

#endif // PLOTSETTINGSDIALOG_H
