#ifndef PLOTSETTINGSDIALOG_H
#define PLOTSETTINGSDIALOG_H

#include <QDialog>

class QLineEdit;
class QLabel;

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
};

#endif // PLOTSETTINGSDIALOG_H
