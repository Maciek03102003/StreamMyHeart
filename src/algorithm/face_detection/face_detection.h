#ifndef FACE_DETECTION_H
#define FACE_DETECTION_H

#include "opencv_dlib_68_landmarks_face_tracker.h"
#include "opencv_haarcascade.h"
#include "heart_rate_source.h"

#include <vector>

std::vector<double_t> detectFace(int faceDetectionAlgorithm, struct input_BGRA_data *bgraData,
				 std::vector<struct vec4> &faceCoordinates, bool enableDebugBoxes,
				 bool enableTracker = true, bool frameUpdateInterval = 60);

#endif // FACE_DETECTION_H