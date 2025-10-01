#ifndef CASCADEIO_H
#define CASCADEIO_H

#include <QString>
#include <Eigen/Dense>

class NetworkCascade;

bool saveCascadeToFile(const NetworkCascade& cascade,
                       const Eigen::VectorXd& freq,
                       QString path,
                       QString* savedAbsolutePath = nullptr,
                       QString* errorMessage = nullptr);

#endif // CASCADEIO_H
