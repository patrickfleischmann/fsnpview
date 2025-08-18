#include "mainwindow.h"
#include "parser_touchstone.h"
#include <QApplication>
#include <QColor>
#include <QVector>
#include <QList>
#include <map>
#include <string>
#include <memory>
#include <iostream>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    QList<QColor> colors;
    colors.append(QColor::fromRgbF(0, 0.4470, 0.7410));
    colors.append(QColor::fromRgbF(0.8500, 0.3250, 0.0980));
    colors.append(QColor::fromRgbF(0.9290, 0.6940, 0.1250));
    colors.append(QColor::fromRgbF(0.4940, 0.1840, 0.5560));
    colors.append(QColor::fromRgbF(0.4660, 0.6740, 0.1880));
    colors.append(QColor::fromRgbF(0.3010, 0.7450, 0.9330));
    colors.append(QColor::fromRgbF(0.6350, 0.0780, 0.1840));
    colors.append(QColor::fromRgbF(0, 0, 1.0000));
    colors.append(QColor::fromRgbF(0, 0.5000, 0));
    colors.append(QColor::fromRgbF(1.0000, 0, 0));
    colors.append(QColor::fromRgbF(0, 0.7500, 0.7500));
    colors.append(QColor::fromRgbF(0.7500, 0, 0.7500));
    colors.append(QColor::fromRgbF(0.7500, 0.7500, 0));
    colors.append(QColor::fromRgbF(0.2500, 0.2500, 0.2500));

    std::map<std::string, std::unique_ptr<ts::TouchstoneData>> parsed_data;

    QStringList args = a.arguments();
    for (int i = 1; i < args.size(); ++i) {
        try {
            std::string path = args.at(i).toStdString();
            auto data = std::make_unique<ts::TouchstoneData>(ts::parse_touchstone(path));

            Eigen::ArrayXd xValues = data->freq;
            Eigen::ArrayXd yValues = data->sparams.col(1).abs().log10() * 20; // s21 dB

            std::vector<double> xValuesStdVector(xValues.data(), xValues.data() + xValues.rows() * xValues.cols());
            std::vector<double> yValuesStdVector(yValues.data(), yValues.data() + yValues.rows() * yValues.cols());

            QVector<double> xValuesQVector = QVector<double>(xValuesStdVector.begin(), xValuesStdVector.end());
            QVector<double> yValuesQVector = QVector<double>(yValuesStdVector.begin(), yValuesStdVector.end());

            QColor color = colors.at((i - 1) % colors.size());

            w.plot(xValuesQVector, yValuesQVector, color);

            parsed_data[path] = std::move(data);
        } catch (const std::exception& e) {
            std::cerr << "Error processing file " << args.at(i).toStdString() << ": " << e.what() << std::endl;
        }
    }

    return a.exec();
}
