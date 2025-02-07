#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect.hpp>

#include <obs-module.h>
#include <vector>
#include <iostream>
#include <stdexcept>

#include "heart_rate_source.h"

std::vector<std::vector<bool>> faceMaskDNN(struct input_BGRA_data *frame, std::vector<struct vec4> &face_coordinates);
