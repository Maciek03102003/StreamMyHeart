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

static char *face_landmark_path;
static bool isLoaded = false;

// Global tracker and flag to indicate if the face was detected
static correlation_tracker tracker;
static frontal_face_detector detector;
static shape_predictor sp;
static int frame_count = 0;
// static bool is_tracking = false;

static rectangle detected_face;
static rectangle initial_face;

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
	face_landmark_path = obs_module_file("shape_predictor_68_face_landmarks.dat");

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

	bfree(face_landmark_path);
}

// Function to detect face on the first frame and track in subsequent frames
std::vector<double_t> detectFaceAOI(struct input_BGRA_data *frame, std::vector<struct vec4> &face_coordinates,
				    int reset_tracker_count, bool enable_tracking, bool enable_debug_boxes)
{
	uint64_t start_ns = os_gettime_ns();
	uint32_t width = frame->width;
	uint32_t height = frame->height;

	uint64_t convert_before = os_gettime_ns();
	// Convert BGRA to OpenCV Mat
	cv::Mat frameMat(frame->height, frame->width, CV_8UC4, frame->data, frame->linesize);

	// Convert to grayscale for dlib processing
	cv::Mat frameGray;
	cv::cvtColor(frameMat, frameGray, cv::COLOR_BGRA2GRAY);

	if (!isLoaded) {
		loadFiles();
	}

	dlib::cv_image<unsigned char> dlibImg(frameGray);
	uint64_t convert_after = os_gettime_ns();
	// obs_log(LOG_INFO, "Convert time: %lu ns", convert_after - convert_before);

	bool reset_face_detection = frame_count % reset_tracker_count == 0;
	bool run_face_detection = !enable_tracking || (enable_tracking && reset_face_detection);

	if (run_face_detection) {
		uint64_t detector_before = os_gettime_ns();
		std::vector<rectangle> faces = detector(dlibImg);
		uint64_t detector_after = os_gettime_ns();
		// obs_log(LOG_INFO, "Detector time: %lu ns", detector_after - detector_before);

		if (!faces.empty()) {
			// obs_log(LOG_INFO, "Face detected!!!!");
			detected_face = faces[0];
			initial_face = faces[0];

			if (enable_tracking) {
				uint64_t tracker_before = os_gettime_ns();
				tracker.start_track(dlibImg, initial_face);
				uint64_t tracker_after = os_gettime_ns();
				// obs_log(LOG_INFO, "Start Tracker time: %lu ns", tracker_after - tracker_before);
				// is_tracking = true;
			}
		} else {
			return std::vector<double_t>(3, 0.0); // No face detected
		}
	} else if (enable_tracking) {
		uint64_t tracker_before = os_gettime_ns();
		tracker.update(dlibImg);
		uint64_t tracker_after = os_gettime_ns();
		// obs_log(LOG_INFO, "Update Tracker time: %lu ns", tracker_after - tracker_before);
		initial_face = tracker.get_position();
	}

	// Perform landmark detection
	uint64_t landmark_before = os_gettime_ns();
	full_object_detection shape = sp(dlibImg, initial_face);
	uint64_t landmark_after = os_gettime_ns();
	// obs_log(LOG_INFO, "Landmark time: %lu ns", landmark_after - landmark_before);

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
	// obs_log(LOG_INFO, "Landmark loop time: %lu ns", for_loop_after - for_loop_before);

	if (enable_debug_boxes) {
		face_coordinates.push_back(getBoundingBox(faceContour, width, height));
		face_coordinates.push_back(getBoundingBox(leftEyes, width, height));
		face_coordinates.push_back(getBoundingBox(rightEyes, width, height));
		face_coordinates.push_back(getBoundingBox(mouth, width, height));
		if (enable_tracking) { // Add face box for tracking
			face_coordinates.push_back(getBoundingBox(
				{
					{static_cast<int>(detected_face.left()),
					 static_cast<int>(detected_face.top())}, // Top-left
					{static_cast<int>(detected_face.right()),
					 static_cast<int>(detected_face.top())}, // Top-right
					{static_cast<int>(detected_face.right()),
					 static_cast<int>(detected_face.bottom())}, // Bottom-right
					{static_cast<int>(detected_face.left()),
					 static_cast<int>(detected_face.bottom())} // Bottom-left
				},
				width, height));
		}
	}

	uint64_t convex_before = os_gettime_ns();
	cv::Mat maskMat = cv::Mat::zeros(frameMat.size(), CV_8UC1);
	cv::fillConvexPoly(maskMat, faceContour, cv::Scalar(255));
	cv::fillConvexPoly(maskMat, leftEyes, cv::Scalar(0));
	cv::fillConvexPoly(maskMat, rightEyes, cv::Scalar(0));
	cv::fillConvexPoly(maskMat, mouth, cv::Scalar(0));
	uint64_t convex_after = os_gettime_ns();
	// obs_log(LOG_INFO, "Convex time: %lu ns", convex_after - convex_before);

	cv::Scalar meanRGB = cv::mean(frameMat, maskMat);
	std::vector<double_t> avgRGB = {meanRGB[0], meanRGB[1], meanRGB[2]};

	// if (frame_count == reset_tracker_count && enable_tracking) {
	// 	obs_log(LOG_INFO, "Reset tracker!!!!");
	// 	is_tracking = false;
	// 	frame_count = 0;
	// }
	// obs_log(LOG_INFO, "Frame count: %d", frame_count);
	if (enable_tracking && reset_face_detection) {
		frame_count = 0;
	}
	frame_count++;

	uint64_t end_ns = os_gettime_ns();
	// obs_log(LOG_INFO, "Whole function time: %lu ns", end_ns - start_ns);

	return avgRGB;
}