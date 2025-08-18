#include "mainwindow.h"
#include "parser_touchstone.h"
#include <QApplication>
#include <QColor>
#include <QVector>
#include <map>
#include <string>
#include <memory>
#include <iostream>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();

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

            QColor color = QColor::fromHsv(((i - 1) * 60) % 360, 255, 255);

            w.plot(xValuesQVector, yValuesQVector, color);

            parsed_data[path] = std::move(data);
        } catch (const std::exception& e) {
            std::cerr << "Error processing file " << args.at(i).toStdString() << ": " << e.what() << std::endl;
        }
    }

    return a.exec();
}
