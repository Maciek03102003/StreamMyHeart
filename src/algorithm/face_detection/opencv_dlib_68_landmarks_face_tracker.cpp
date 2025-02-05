#include <opencv2/opencv.hpp>
#include <dlib/opencv.h>
#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/image_processing.h>
#include <dlib/image_processing/correlation_tracker.h>

#include <chrono>
#include <cstdint>

#include "heart_rate_source.h"
#include "plugin-support.h"

using namespace std;
using namespace dlib;

static inline uint64_t os_gettime_ns()
{
	using namespace std::chrono;
	return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
}

// Global tracker and flag to indicate if the face was detected
static correlation_tracker tracker;
static bool is_tracking = false;
static rectangle initial_face;
static shape_predictor sp;
static frontal_face_detector detector;
static char *face_landmark_path;
static bool isLoaded = false;
static int frame_count = 0;

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

static void loadFiles()
{
	face_landmark_path = obs_find_module_file(obs_get_module("pulse-obs"), "shape_predictor_68_face_landmarks.dat");

	if (!face_landmark_path) {
		obs_log(LOG_ERROR, "Failed to find face landmark file");
		throw std::runtime_error("Failed to find face landmark file");
	}

	// Initialize dlib shape predictor and face detector
	detector = get_frontal_face_detector();

	deserialize(face_landmark_path) >> sp;
	obs_log(LOG_INFO, "Dlib deserialize!!!!");

	isLoaded = true;
	obs_log(LOG_INFO, "Model loaded!!!!");
}

// Function to detect face on the first frame and track in subsequent frames
std::vector<double_t> detectFaceAOI(struct input_BGRA_data *frame, std::vector<struct vec4> &face_coordinates)
{
	uint64_t start_ns = os_gettime_ns();
	uint32_t width = frame->width;
	uint32_t height = frame->height;

	// Convert BGRA to OpenCV Mat
	cv::Mat frameMat(frame->height, frame->width, CV_8UC4, frame->data, frame->linesize);

	// Convert to grayscale for dlib processing
	cv::Mat frameGray;
	cv::cvtColor(frameMat, frameGray, cv::COLOR_BGRA2GRAY);

	obs_log(LOG_INFO, "Dlib initialization");

	if (!isLoaded) {
		loadFiles();
	}

	dlib::cv_image<unsigned char> dlibImg(frameGray);

	if (!is_tracking) {
		obs_log(LOG_INFO, "Detect faces!!!!");
		// First frame: Detect face
		uint64_t detector_before = os_gettime_ns();
		std::vector<rectangle> faces = detector(dlibImg);
		uint64_t detector_after = os_gettime_ns();
		obs_log(LOG_INFO, "Detector time: %lu ns", detector_after - detector_before);

		if (!faces.empty()) {
			initial_face = faces[0]; // Assume first detected face is the target
			uint64_t tracker_before = os_gettime_ns();
			tracker.start_track(dlibImg, initial_face);
			uint64_t tracker_after = os_gettime_ns();
			obs_log(LOG_INFO, "Start Tracker time: %lu ns", tracker_after - tracker_before);
			is_tracking = true;
		} else {
			obs_log(LOG_INFO, "No face detected!!!!");
			// if not face detected, return empty mask
			return std::vector<double_t>(3, 0.0);
			// return std::vector<std::vector<bool>>(frame->height, std::vector<bool>(frame->width, false));
		}
	} else {
		obs_log(LOG_INFO, "Update tracker!!!!");
		// Track face in subsequent frames
		uint64_t tracker_before = os_gettime_ns();
		tracker.update(dlibImg);
		uint64_t tracker_after = os_gettime_ns();
		obs_log(LOG_INFO, "Update Tracker time: %lu ns", tracker_after - tracker_before);
		initial_face = tracker.get_position();
	}

	// Perform landmark detection
	uint64_t landmark_before = os_gettime_ns();
	full_object_detection shape = sp(dlibImg, initial_face);
	uint64_t landmark_after = os_gettime_ns();
	obs_log(LOG_INFO, "Landmark time: %lu ns", landmark_after - landmark_before);

	obs_log(LOG_INFO, "Initialize AOI mask!!!!");

	// Initialize AOI mask (false for non-face pixels)
	std::vector<std::vector<bool>> mask(frame->height, std::vector<bool>(frame->width, false));

	obs_log(LOG_INFO, "Mark eye regions!!!!");
	// Exclude eyes and mouth from the mask
	uint64_t for_loop_before = os_gettime_ns();
	std::vector<cv::Point> leftEyes, rightEyes, mouth, faceContour;
	for (int i = 1; i <= 17; i++)
		faceContour.push_back(cv::Point(shape.part(i).x(), shape.part(i).y()));
	for (int i = 36; i <= 41; i++)
		leftEyes.push_back(cv::Point(shape.part(i).x(), shape.part(i).y()));
	for (int i = 42; i <= 47; i++)
		rightEyes.push_back(cv::Point(shape.part(i).x(), shape.part(i).y()));
	for (int i = 48; i <= 60; i++)
		mouth.push_back(cv::Point(shape.part(i).x(), shape.part(i).y()));
	uint64_t for_loop_after = os_gettime_ns();
	obs_log(LOG_INFO, "Landmark loop time: %lu ns", for_loop_after - for_loop_before);

	obs_log(LOG_INFO, "Get bounding boxes!!!!");
	face_coordinates.push_back(getBoundingBox(faceContour, width, height));
	face_coordinates.push_back(getBoundingBox(leftEyes, width, height));
	face_coordinates.push_back(getBoundingBox(rightEyes, width, height));
	face_coordinates.push_back(getBoundingBox(mouth, width, height));

	obs_log(LOG_INFO, "Fill eye and mouth regions!!!!");
	cv::Mat maskMat = cv::Mat::zeros(frameMat.size(), CV_8UC1);
	cv::fillConvexPoly(maskMat, faceContour, cv::Scalar(255));
	cv::fillConvexPoly(maskMat, leftEyes, cv::Scalar(0));
	cv::fillConvexPoly(maskMat, rightEyes, cv::Scalar(0));
	cv::fillConvexPoly(maskMat, mouth, cv::Scalar(0));

	cv::Scalar meanRGB = cv::mean(frameMat, maskMat);
	std::vector<double_t> avgRGB = {meanRGB[0], meanRGB[1], meanRGB[2]};

	obs_log(LOG_INFO, "Convert mask to 2D boolean vector!!!!");
	// for (uint32_t y = 0; y < height; y++) {
	// 	for (uint32_t x = 0; x < width; x++) {
	// 		mask[y][x] = (maskMat.at<uint8_t>(y, x) == 255);
	// 	}
	// }

	frame_count++;
	obs_log(LOG_INFO, "Frame count: %d", frame_count);

	if (frame_count == 60) {
		obs_log(LOG_INFO, "Reset tracker!!!!");
		is_tracking = false;
		frame_count = 0;
	}

	uint64_t end_ns = os_gettime_ns();
	obs_log(LOG_INFO, "Whole function time: %lu ns", end_ns - start_ns);

	return avgRGB;
}