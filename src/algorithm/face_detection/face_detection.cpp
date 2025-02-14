#include "face_detection.h"

enum FaceDetectionAlgorithm { OPENCV_HAARCASCADE = 0, DLIB_FACE_DETECTION = 1 };

std::vector<double_t> detectFace(int faceDetectionAlgorithm, struct input_BGRA_data *bgraData,
				 std::vector<struct vec4> &faceCoordinates, bool enableDebugBoxes, bool enableTracker,
				 bool frameUpdateInterval)
{
	std::vector<double_t> avg;

	if (faceDetectionAlgorithm == OPENCV_HAARCASCADE) {
		// Haarcascade face detection with OpenCV
		avg = detectFacesAndCreateMask(bgraData, faceCoordinates, enableDebugBoxes);
	} else if (faceDetectionAlgorithm == DLIB_FACE_DETECTION) {
		// Dlib face detection with 68 landmarks, with/without tracking
		avg = detectFaceAOI(bgraData, faceCoordinates, frameUpdateInterval, enableTracker, enableDebugBoxes);
	}

	return avg;
}