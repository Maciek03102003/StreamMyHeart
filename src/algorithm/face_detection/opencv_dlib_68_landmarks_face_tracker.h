#ifndef FACE_TRACKER_H
#define FACE_TRACKER_H

#include <opencv2/opencv.hpp>
#include <dlib/opencv.h>
#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/image_processing.h>
#include <dlib/image_processing/correlation_tracker.h>

#include "heart_rate_source.h"
#include "face_detection.h"

class DlibFaceDetection : public FaceDetection {
public:
	std::vector<double_t> detectFace(std::shared_ptr<struct input_BGRA_data> frame,
					 std::vector<struct vec4> &faceCoordinates, bool enableDebugBoxes,
					 bool enableTracker, int frameUpdateInterval, bool evaluation = false);

private:
	void loadFiles(bool evaluation);

	std::mutex detectionMutex;
	bool isLoaded = false;
	bool startedTracking = false;
	bool faceDetected = false;
	char *faceLandmarkPath;
	dlib::correlation_tracker tracker;
	dlib::frontal_face_detector detector;
	dlib::shape_predictor sp;
	int frameCount = 0;
	dlib::rectangle detectedFace; // only face detection
	dlib::rectangle initialFace;  // face tracking and detection
};

#endif // FACE_TRACKER_H