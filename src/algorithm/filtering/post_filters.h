#ifndef POST_FILTERS_H
#define POST_FILTERS_H

#include <vector>
#include <Eigen/Dense>
#include <cmath>
#include <iostream>

std::vector<double_t> applyPostFilter(std::vector<double_t> signal, int filter, int fps);

#endif