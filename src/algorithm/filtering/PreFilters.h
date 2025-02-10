#ifndef PRE_FILTERS_H
#define PRE_FILTERS_H

#include <vector>
#include <Eigen/Dense>
#include <cmath>
#include <iostream>

std::vector<std::vector<double_t>> applyPreFilter(std::vector<std::vector<double_t>> signal, int filter, int fps);

#endif