#ifndef FACE_DETECT_H
#define FACE_DETECT_H

#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect.hpp>

#include <obs-module.h>
#include "plugin-support.h"
#include <vector>
#include <iostream>
#include <stdexcept>

#include "heart_rate_source.h"
#include "face_detection.h"

class HaarCascadeFaceDetection : public FaceDetection {
public:
	std::vector<double_t> detectFace(std::shared_ptr<struct input_BGRA_data> frame,
					 std::vector<struct vec4> &faceCoordinates, bool enableDebugBoxes,
					 bool enableTracker, int frameUpdateInterval, bool evaluation = false) override;

private:
	void initializeFaceCascade(bool evaluation);
	cv::CascadeClassifier faceCascade, mouthCascade, leftEyeCascade, rightEyeCascade;
	bool cascadeLoaded = false;
};

#endif
