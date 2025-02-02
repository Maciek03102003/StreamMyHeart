#ifndef FACE_TRACKER_H
#define FACE_TRACKER_H

#include <opencv2/opencv.hpp>
#include <dlib/opencv.h>
#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/image_processing.h>
#include <dlib/image_processing/correlation_tracker.h>

#include "heart_rate_source.h"

std::vector<std::vector<bool>> detectFaceAOI(struct input_BGRA_data *frame, std::vector<struct vec4> &face_coordinates);

#endif // FACE_TRACKER_H