#ifndef SNPFILE_H
#define SNPFILE_H

#include <QString>
#include <QVector>
#include <QPointF>

struct SParameter {
    QVector<QPointF> magnitude;
    QVector<QPointF> phase;
};

class SnpFile
{
public:
    SnpFile(const QString &filePath);

    bool parse();
    const QVector<SParameter>& getSParameters() const;

private:
    QString m_filePath;
    QVector<SParameter> m_sParameters;
};

#endif // SNPFILE_H
