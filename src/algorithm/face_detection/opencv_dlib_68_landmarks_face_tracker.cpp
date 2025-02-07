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
		throw std::runtime_error("Failed to find face landmark file");
	}

	// Initialize dlib shape predictor and face detector
	detector = get_frontal_face_detector();

	deserialize(face_landmark_path) >> sp;

	isLoaded = true;

	bfree(face_landmark_path);
}

// Function to detect face on the first frame and track in subsequent frames
std::vector<double_t> detectFaceAOI(struct input_BGRA_data *frame, std::vector<struct vec4> &face_coordinates,
				    int reset_tracker_count, bool enable_tracking, bool enable_debug_boxes)
{
	uint32_t width = frame->width;
	uint32_t height = frame->height;

	// Convert BGRA to OpenCV Mat
	cv::Mat frameMat(frame->height, frame->width, CV_8UC4, frame->data, frame->linesize);

	// Convert to grayscale for dlib processing
	cv::Mat frameGray;
	cv::cvtColor(frameMat, frameGray, cv::COLOR_BGRA2GRAY);

	if (!isLoaded) {
		loadFiles();
	}

	dlib::cv_image<unsigned char> dlibImg(frameGray);

	bool reset_face_detection = frame_count % reset_tracker_count == 0;
	bool run_face_detection = !enable_tracking || (enable_tracking && reset_face_detection);

	if (run_face_detection) {
		std::vector<rectangle> faces = detector(dlibImg);

		if (!faces.empty()) {
			detected_face = faces[0];
			initial_face = faces[0];

			if (enable_tracking) {
				tracker.start_track(dlibImg, initial_face);
				// is_tracking = true;
			}
		} else {
			return std::vector<double_t>(3, 0.0); // No face detected
		}
	} else if (enable_tracking) {
		tracker.update(dlibImg);
		initial_face = tracker.get_position();
	}

	// Perform landmark detection
	full_object_detection shape = sp(dlibImg, initial_face);

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

	cv::Mat maskMat = cv::Mat::zeros(frameMat.size(), CV_8UC1);
	cv::fillConvexPoly(maskMat, faceContour, cv::Scalar(255));
	cv::fillConvexPoly(maskMat, leftEyes, cv::Scalar(0));
	cv::fillConvexPoly(maskMat, rightEyes, cv::Scalar(0));
	cv::fillConvexPoly(maskMat, mouth, cv::Scalar(0));

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

	return avgRGB;
}