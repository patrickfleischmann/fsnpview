#include <QApplication>
#include <QCoreApplication>
#include <QTableView>
#include <QWheelEvent>
#include <QStandardItemModel>
#include <QLineEdit>
#include <QRect>
#include <QEventLoop>

#include "mainwindow.h"
#include "networkcascade.h"
#include "networklumped.h"

#include <algorithm>
#include <iostream>

namespace {
constexpr int ColumnTo = 3;
constexpr int ColumnFrom = 4;

bool sendWheelEvent(QTableView *view, const QModelIndex &index, int angleDeltaY)
{
    if (!view || !index.isValid())
        return false;

    QRect rect = view->visualRect(index);
    if (!rect.isValid())
        return false;

    QPoint localPos = rect.center();
    QPoint globalPos = view->viewport()->mapToGlobal(localPos);

    QWheelEvent event(localPos, globalPos, QPoint(), QPoint(0, angleDeltaY),
                      Qt::NoButton, Qt::NoModifier, Qt::ScrollUpdate, false);
    return QCoreApplication::sendEvent(view->viewport(), &event);
}
}

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
    QApplication app(argc, argv);

    MainWindow window;
    window.show();
    QApplication::processEvents(QEventLoop::AllEvents);

    auto *cascade = window.cascade();
    if (!cascade) {
        std::cerr << "Cascade is not available" << std::endl;
        return 1;
    }

    auto *network = new NetworkLumped(NetworkLumped::NetworkType::R_series, {50.0});
    window.addNetworkToCascade(network);
    QApplication::processEvents();

    QTableView *cascadeView = window.cascadeTableView();
    if (!cascadeView) {
        std::cerr << "Cascade view not found" << std::endl;
        return 1;
    }

    auto *model = qobject_cast<QStandardItemModel*>(cascadeView->model());
    if (!model) {
        std::cerr << "Unexpected cascade model type" << std::endl;
        return 1;
    }

    if (model->rowCount() == 0) {
        std::cerr << "Cascade model has no rows" << std::endl;
        return 1;
    }

    if (QLineEdit *multiplier = window.findChild<QLineEdit*>("lineEditMouseWheelMult")) {
        multiplier->setText(QStringLiteral("10"));
    }

    const QModelIndex toIndex = model->index(0, ColumnTo);
    const QModelIndex fromIndex = model->index(0, ColumnFrom);

    cascadeView->scrollTo(toIndex);
    QApplication::processEvents();

    int initialTo = model->data(toIndex).toInt();
    int initialFrom = model->data(fromIndex).toInt();
    int portCount = cascade->getNetworks().isEmpty() ? 0 : cascade->getNetworks().first()->portCount();

    if (portCount <= 0) {
        std::cerr << "Invalid port count" << std::endl;
        return 1;
    }

    if (!sendWheelEvent(cascadeView, toIndex, -120)) {
        std::cerr << "Failed to send wheel event to 'To' column" << std::endl;
        return 1;
    }
    QApplication::processEvents();

    int decreasedTo = model->data(toIndex).toInt();
    int expectedDecrease = std::max(1, initialTo - 1);
    if (decreasedTo != expectedDecrease || cascade->toPort(0) != expectedDecrease) {
        std::cerr << "Unexpected 'To' value after decrement: " << decreasedTo << std::endl;
        return 1;
    }

    if (initialTo > 1 && decreasedTo - initialTo != -1) {
        std::cerr << "'To' column did not change by exactly one step" << std::endl;
        return 1;
    }

    if (!sendWheelEvent(cascadeView, toIndex, -120)) {
        std::cerr << "Failed to send wheel event to 'To' column for clamp" << std::endl;
        return 1;
    }
    QApplication::processEvents();

    int clampedTo = model->data(toIndex).toInt();
    if (clampedTo != 1 || cascade->toPort(0) != 1) {
        std::cerr << "'To' column should clamp at 1" << std::endl;
        return 1;
    }

    if (!sendWheelEvent(cascadeView, fromIndex, 120)) {
        std::cerr << "Failed to send wheel event to 'From' column" << std::endl;
        return 1;
    }
    QApplication::processEvents();

    int increasedFrom = model->data(fromIndex).toInt();
    int expectedIncrease = std::min(portCount, initialFrom + 1);
    if (increasedFrom != expectedIncrease || cascade->fromPort(0) != expectedIncrease) {
        std::cerr << "Unexpected 'From' value after increment: " << increasedFrom << std::endl;
        return 1;
    }

    if (initialFrom < portCount && increasedFrom - initialFrom != 1) {
        std::cerr << "'From' column did not change by exactly one step" << std::endl;
        return 1;
    }

    if (!sendWheelEvent(cascadeView, fromIndex, 120)) {
        std::cerr << "Failed to send wheel event to 'From' column for clamp" << std::endl;
        return 1;
    }
    QApplication::processEvents();

    int clampedFrom = model->data(fromIndex).toInt();
    if (clampedFrom != portCount || cascade->fromPort(0) != portCount) {
        std::cerr << "'From' column should clamp at port count" << std::endl;
        return 1;
    }

    return 0;
}
