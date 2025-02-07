#ifndef FACE_TRACKER_H
#define FACE_TRACKER_H

#include <opencv2/opencv.hpp>
#include <dlib/opencv.h>
#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/image_processing.h>
#include <dlib/image_processing/correlation_tracker.h>

#include "heart_rate_source.h"

std::vector<double_t> detectFaceAOI(struct input_BGRA_data *frame, std::vector<struct vec4> &face_coordinates,
				    int reset_tracker_count, bool enable_tracking, bool enable_debug_boxes);

#endif // FACE_TRACKER_H