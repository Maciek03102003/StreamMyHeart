#include "face_detection.h"
#include "opencv_haarcascade.h"
#include "opencv_dlib_68_landmarks_face_tracker.h"

std::unique_ptr<FaceDetection> FaceDetection::create(FaceDetectionAlgorithm algorithm)
{
	if (algorithm == FaceDetectionAlgorithm::HAAR_CASCADE) {
		return std::make_unique<HaarCascadeFaceDetection>();
	} else if (algorithm == FaceDetectionAlgorithm::DLIB) {
		return std::make_unique<DlibFaceDetection>();
	}
	return nullptr;
}