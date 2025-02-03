#include <chrono>
#include <cstdint>

#include <opencv2/opencv.hpp>
#include <dlib/opencv.h>
#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/image_processing.h>

#include "opencv_dlib_68_landmarks.h"
#include "plugin-support.h"

using namespace dlib;
using namespace std;

static inline uint64_t os_gettime_ns()
{
	using namespace std::chrono;
	return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
}

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

static frontal_face_detector detector;
static shape_predictor sp;
static char *face_landmark_path;
static bool isLoaded = false;

static void loadFiles() {
	face_landmark_path =
		obs_find_module_file(obs_get_module("pulse-obs"), "shape_predictor_68_face_landmarks.dat");

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


std::vector<std::vector<bool>> faceMask(struct input_BGRA_data *frame, std::vector<struct vec4> &face_coordinates)
{
	obs_log(LOG_INFO, "Start face mask!!!!");
	// Extract frame parameters
	uint8_t *data = frame->data;
	uint32_t width = frame->width;
	uint32_t height = frame->height;
	uint32_t linesize = frame->linesize;

	// Convert BGRA to OpenCV Mat
	cv::Mat frameMat(height, width, CV_8UC4, data, linesize);

	// Convert BGRA to BGR for Dlib processing
	cv::Mat frameBGR;
	cv::cvtColor(frameMat, frameBGR, cv::COLOR_BGRA2BGR);

	obs_log(LOG_INFO, "Dlib initialzation");

	if (!isLoaded) {
		obs_log(LOG_INFO, "Load files!!!!");
		loadFiles();
	}

	uint64_t start_ns = os_gettime_ns();

	obs_log(LOG_INFO, "Detect faces!!!!");
	// Detect faces
	uint64_t detector_before = os_gettime_ns();
	std::vector<rectangle> faces = detector(cv_image<bgr_pixel>(frameBGR));
	uint64_t detector_after = os_gettime_ns();
	obs_log(LOG_INFO, "Detector time: %lu ns", detector_after - detector_before);

	// Create a binary mask (black background, white face)
	cv::Mat mask = cv::Mat::zeros(frameBGR.size(), CV_8UC1);

	uint64_t for_loop_before = os_gettime_ns();
	for (auto &face : faces) {
		obs_log(LOG_INFO, "Detect faces!!!!");
		uint64_t landmark_before = os_gettime_ns();
		full_object_detection shape = sp(cv_image<bgr_pixel>(frameBGR), face);
		uint64_t landmark_after = os_gettime_ns();
		obs_log(LOG_INFO, "Landmark time: %lu ns", landmark_after - landmark_before);

		// Store face contour
		std::vector<cv::Point> faceContour;
		for (int i = 0; i < 17; i++) { // Jawline (points 1–17)
			faceContour.push_back(cv::Point(shape.part(i).x(), shape.part(i).y()));
		}

		// Fill face area (excluding eyes/mouth)
		cv::fillConvexPoly(mask, faceContour, cv::Scalar(255));

		// Exclude eyes and mouth
		std::vector<cv::Point> leftEyes;
		std::vector<cv::Point> rightEyes;
		std::vector<cv::Point> mouth;
		for (int i = 36; i <= 41; i++) { // Right eye (37–42)
			leftEyes.push_back(cv::Point(shape.part(i).x(), shape.part(i).y()));
		}
		for (int i = 42; i <= 47; i++) { // Left eye (43–47)
			rightEyes.push_back(cv::Point(shape.part(i).x(), shape.part(i).y()));
		}
		for (int i = 48; i <= 60; i++) { // Mouth (48–60)
			mouth.push_back(cv::Point(shape.part(i).x(), shape.part(i).y()));
		}

		face_coordinates.push_back(getBoundingBox(leftEyes, width, height));
		face_coordinates.push_back(getBoundingBox(rightEyes, width, height));
		face_coordinates.push_back(getBoundingBox(mouth, width, height));

		cv::fillConvexPoly(mask, leftEyes, cv::Scalar(0)); // Black out eyes
		cv::fillConvexPoly(mask, rightEyes, cv::Scalar(0));
		cv::fillConvexPoly(mask, mouth, cv::Scalar(0)); // Black out mouth
	}
	uint64_t for_loop_after = os_gettime_ns();
	obs_log(LOG_INFO, "Finish processing faces!!!!");

	// Convert mask to a 2D boolean vector
	std::vector<std::vector<bool>> binaryMask(height, std::vector<bool>(width, false));
	for (uint32_t y = 0; y < height; y++) {
		for (uint32_t x = 0; x < width; x++) {
			binaryMask[y][x] = (mask.at<uint8_t>(y, x) > 0);
		}
	}

	obs_log(LOG_INFO, "For loop time: %lu ns", for_loop_after - for_loop_before);

	uint64_t end_ns = os_gettime_ns();

	obs_log(LOG_INFO, "Whole function time: %lu ns", end_ns - start_ns);

	return binaryMask;
}