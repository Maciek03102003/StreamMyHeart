#ifndef FILTER_UTIL_H
#define FILTER_UTIL_H

#include <vector>
#include <Eigen/Dense>
#include <cmath>
#include <iostream>

using namespace Eigen;

void butterworthBandpass(int order, double minHz, double maxHz, double fps, VectorXd &a, VectorXd &b);
VectorXd applyIIRFilter(const VectorXd &b, const VectorXd &a, const VectorXd &x);
MatrixXd forwardBackFilter(const VectorXd &b, const VectorXd &a, const MatrixXd &x);

#endif