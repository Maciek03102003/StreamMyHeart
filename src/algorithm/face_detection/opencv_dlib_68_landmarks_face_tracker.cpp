#include <opencv2/opencv.hpp>
#include <dlib/opencv.h>
#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/image_processing.h>
#include <dlib/image_processing/correlation_tracker.h>

#include <chrono>
#include <cstdint>

#include "heart_rate_source.h"
#include "opencv_dlib_68_landmarks_face_tracker.h"
#include "plugin-support.h"

using namespace std;
using namespace dlib;

static struct vec4 getBoundingBox(const std::vector<cv::Point> &landmarks, uint32_t width, uint32_t height)
{
	float minX = std::numeric_limits<float>::max();
	float maxX = std::numeric_limits<float>::lowest();
	float minY = std::numeric_limits<float>::max();
	float maxY = std::numeric_limits<float>::lowest();

	for (const auto &landmark : landmarks) {
		minX = std::min(minX, static_cast<float>(landmark.x));
		maxX = std::max(maxX, static_cast<float>(landmark.x));
		minY = std::min(minY, static_cast<float>(landmark.y));
		maxY = std::max(maxY, static_cast<float>(landmark.y));
	}
	struct vec4 rect;
	vec4_set(&rect, minX / width, maxX / width, minY / height, maxY / height);
	return rect;
}

void DlibFaceDetection::loadFiles(bool evaluation)
{
	if (evaluation) {
		faceLandmarkPath = (char *)bmalloc(strlen("shape_predictor_68_face_landmarks.dat") + 1);
		strcpy(faceLandmarkPath, "shape_predictor_68_face_landmarks.dat");
	} else {
		faceLandmarkPath = obs_module_file("shape_predictor_68_face_landmarks.dat");
	}

	if (!faceLandmarkPath) {
		throw std::runtime_error("Failed to find face landmark file");
	}

	// Initialize dlib shape predictor and face detector
	detector = get_frontal_face_detector();

	deserialize(faceLandmarkPath) >> sp;

	isLoaded = true;

	bfree(faceLandmarkPath);
}

// Function to detect face on the first frame and track in subsequent frames
std::vector<double_t> DlibFaceDetection::detectFace(std::shared_ptr<struct input_BGRA_data> frame,
						    std::vector<struct vec4> &faceCoordinates, bool enableDebugBoxes,
						    bool enableTracker, int frameUpdateInterval, bool evaluation)
{
	uint32_t width = frame->width;
	uint32_t height = frame->height;

	// Convert BGRA to OpenCV Mat
	cv::Mat frameMat(frame->height, frame->width, CV_8UC4, frame->data, frame->linesize);

	// Convert to grayscale for dlib processing
	cv::Mat frameGray;
	cv::cvtColor(frameMat, frameGray, cv::COLOR_BGRA2GRAY);

	if (!isLoaded) {
		loadFiles(evaluation);
	}

	dlib::cv_image<unsigned char> dlibImg(frameGray);

	bool resetFaceDetection = frameCount % frameUpdateInterval == 0;
	bool runFaceDetection = !enableTracker || (enableTracker && (resetFaceDetection || !startedTracking));

	if (runFaceDetection) {
		std::vector<rectangle> faces = detector(dlibImg);

		if (!faces.empty()) {
			detectedFace = faces[0];
			initialFace = faces[0];

			if (enableTracker) {
				tracker.start_track(dlibImg, initialFace);
				startedTracking = true;
			}
		} else {
			return std::vector<double_t>(3, 0.0); // No face detected
		}
	} else if (enableTracker) {
		tracker.update(dlibImg);
		initialFace = tracker.get_position();
	}

	// Perform landmark detection
	full_object_detection shape = sp(dlibImg, initialFace);

	// Exclude eyes and mouth from the mask
	std::vector<cv::Point> leftEyes, rightEyes, mouth, faceContour;
	for (int i = 1; i <= 17; i++)
		faceContour.push_back(cv::Point(shape.part(i).x(), shape.part(i).y()));
	for (int i = 36; i <= 41; i++)
		leftEyes.push_back(cv::Point(shape.part(i).x(), shape.part(i).y()));
	for (int i = 42; i <= 47; i++)
		rightEyes.push_back(cv::Point(shape.part(i).x(), shape.part(i).y()));
	for (int i = 48; i <= 60; i++)
		mouth.push_back(cv::Point(shape.part(i).x(), shape.part(i).y()));

	if (enableDebugBoxes) {
		faceCoordinates.push_back(getBoundingBox(faceContour, width, height));
		faceCoordinates.push_back(getBoundingBox(leftEyes, width, height));
		faceCoordinates.push_back(getBoundingBox(rightEyes, width, height));
		faceCoordinates.push_back(getBoundingBox(mouth, width, height));
		if (enableTracker) { // Add face box for tracking
			faceCoordinates.push_back(getBoundingBox(
				{
					{static_cast<int>(detectedFace.left()),
					 static_cast<int>(detectedFace.top())}, // Top-left
					{static_cast<int>(detectedFace.right()),
					 static_cast<int>(detectedFace.top())}, // Top-right
					{static_cast<int>(detectedFace.right()),
					 static_cast<int>(detectedFace.bottom())}, // Bottom-right
					{static_cast<int>(detectedFace.left()),
					 static_cast<int>(detectedFace.bottom())} // Bottom-left
				},
				width, height));
		}
	}

	cv::Mat maskMat = cv::Mat::zeros(frameMat.size(), CV_8UC1);
	cv::fillConvexPoly(maskMat, faceContour, cv::Scalar(255));
	cv::fillConvexPoly(maskMat, leftEyes, cv::Scalar(0));
	cv::fillConvexPoly(maskMat, rightEyes, cv::Scalar(0));
	cv::fillConvexPoly(maskMat, mouth, cv::Scalar(0));

	cv::Scalar meanRGB = cv::mean(frameMat, maskMat);
	std::vector<double_t> avgRGB = {meanRGB[0], meanRGB[1], meanRGB[2]};

	// if (frame_count == frameUpdateInterval && enableTracker) {
	// 	obs_log(LOG_INFO, "Reset tracker!!!!");
	// 	is_tracking = false;
	// 	frame_count = 0;
	// }
	// obs_log(LOG_INFO, "Frame count: %d", frame_count);
	if (enableTracker && resetFaceDetection) {
		frameCount = 0;
	}
	frameCount++;

	return avgRGB;
}