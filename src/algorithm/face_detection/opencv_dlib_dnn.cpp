#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp> // Include for DNN module
#include <dlib/opencv.h>
#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/image_processing.h>

#include "opencv_dlib_dnn.h"
#include "plugin-support.h"

using namespace dlib;
using namespace std;

#include <chrono>
#include <cstdint>

static inline uint64_t os_gettime_ns()
{
	using namespace std::chrono;
	return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
}

static char *face_landmark_path;
static shape_predictor sp;
static char *modelConfiguration;
static char *modelWeights;
static cv::dnn::Net net;
static bool isLoaded = false;

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

	// Initialize dlib shape predictor
	obs_log(LOG_INFO, "Dlib shape predictor!!!!");
	deserialize(face_landmark_path) >> sp;
	obs_log(LOG_INFO, "Dlib deserialize!!!!");

	obs_log(LOG_INFO, "Detect faces using DNN!!!!");

	// Load OpenCV DNN face detection model (Caffe-based)
	modelConfiguration = obs_find_module_file(obs_get_module("pulse-obs"), "deploy.prototxt");
	obs_log(LOG_INFO, "Model configuration: %s", modelConfiguration);
	modelWeights =
		obs_find_module_file(obs_get_module("pulse-obs"), "res10_300x300_ssd_iter_140000_fp16.caffemodel");
	obs_log(LOG_INFO, "Model weights: %s", modelWeights);

	// cast char * to string
	std::string modelConfiguration_str(modelConfiguration);
	std::string modelWeights_str(modelWeights);

	// Load the pre-trained model
	net = cv::dnn::readNetFromCaffe(modelConfiguration_str, modelWeights_str);
	isLoaded = true;
	obs_log(LOG_INFO, "Model loaded!!!!");
}

std::vector<std::vector<bool>> faceMaskDNN(struct input_BGRA_data *frame, std::vector<struct vec4> &face_coordinates)
{
	obs_log(LOG_INFO, "Start face mask!!!!");
	// Extract frame parameters
	uint8_t *data = frame->data;
	uint32_t width = frame->width;
	uint32_t height = frame->height;
	uint32_t linesize = frame->linesize;

	uint64_t start_ns = os_gettime_ns(); // Overall start time

	// Convert BGRA to OpenCV Mat
	cv::Mat frameMat(height, width, CV_8UC4, data, linesize);

	// Convert BGRA to BGR for Dlib processing
	cv::Mat frameBGR;
	cv::cvtColor(frameMat, frameBGR, cv::COLOR_BGRA2BGR);

	obs_log(LOG_INFO, "Dlib initialization");

	if (!isLoaded) {
		obs_log(LOG_INFO, "Load files!!!!");
		loadFiles();
	}

	obs_log(LOG_INFO, "Start processing faces!!!!");
	// Convert frame to blob for the DNN model
	cv::Mat blob;
	cv::dnn::blobFromImage(frameBGR, blob, 1.0, cv::Size(300, 300), cv::Scalar(104.0, 177.0, 123.0), false, false);

	uint64_t setup_ns = os_gettime_ns();

	// Set the input blob for the network
	net.setInput(blob);

	uint64_t forward_ns = os_gettime_ns();
	obs_log(LOG_INFO, "Setup time: %lu ns", forward_ns - setup_ns);

	// Run forward pass to get detections
	cv::Mat detections = net.forward();

	uint64_t detection_ns = os_gettime_ns();

	obs_log(LOG_INFO, "detection time: %lu ns", detection_ns - forward_ns);

	// Create a binary mask (black background, white face)
	cv::Mat mask = cv::Mat::zeros(frameBGR.size(), CV_8UC1);

	uint64_t for_loop_before = os_gettime_ns();
	// Loop over detections and process each face
	for (int i = 0; i < detections.size[2]; ++i) {
		float confidence = detections.at<float>(i, 2);
		if (confidence > 0.5) { // Confidence threshold
			// Get the bounding box for the detected face
			int x1 = static_cast<int>(detections.at<float>(i, 3) * width);
			int y1 = static_cast<int>(detections.at<float>(i, 4) * height);
			int x2 = static_cast<int>(detections.at<float>(i, 5) * width);
			int y2 = static_cast<int>(detections.at<float>(i, 6) * height);

			// Create a rectangle from the bounding box
			rectangle faceRect(x1, y1, x2 - x1, y2 - y1);

			// Use dlib's shape predictor to detect landmarks
			uint64_t landmark_before = os_gettime_ns();
			full_object_detection shape = sp(cv_image<bgr_pixel>(frameBGR), faceRect);
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

			// Black out eyes and mouth in the mask
			cv::fillConvexPoly(mask, leftEyes, cv::Scalar(0));
			cv::fillConvexPoly(mask, rightEyes, cv::Scalar(0));
			cv::fillConvexPoly(mask, mouth, cv::Scalar(0));
		}
	}

	uint64_t for_loop_after = os_gettime_ns();

	obs_log(LOG_INFO, "For loop time: %lu ns", for_loop_after - for_loop_before);

	obs_log(LOG_INFO, "Finish processing faces!!!!");

	// Convert mask to a 2D boolean vector
	std::vector<std::vector<bool>> binaryMask(height, std::vector<bool>(width, false));
	for (uint32_t y = 0; y < height; y++) {
		for (uint32_t x = 0; x < width; x++) {
			binaryMask[y][x] = (mask.at<uint8_t>(y, x) > 0);
		}
	}

	bfree(face_landmark_path);

	uint64_t end_ns = os_gettime_ns();

	obs_log(LOG_INFO, "Whole function time: %lu ns", end_ns - start_ns);

	return binaryMask;
}
