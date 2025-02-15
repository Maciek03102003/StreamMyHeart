#ifndef FACE_DETECTION_H
#define FACE_DETECTION_H

#include <vector>
#include <dlib/image_processing.h>
#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/image_processing/render_face_detections.h>
#include <dlib/image_processing/shape_predictor.h>
#include <dlib/opencv.h>

enum class FaceDetectionAlgorithm { HAAR_CASCADE, DLIB };

class FaceDetection {
public:
	virtual ~FaceDetection() = default;
	virtual std::vector<double_t> detectFace(struct input_BGRA_data *bgraData,
						 std::vector<struct vec4> &faceCoordinates, bool enableDebugBoxes,
						 bool enableTracker, int frameUpdateInterval,
						 bool evaluation = false) = 0;

	static std::unique_ptr<FaceDetection> create(FaceDetectionAlgorithm algorithm);
};

#endif // FACE_DETECTION_H