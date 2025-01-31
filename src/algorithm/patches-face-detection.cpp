// #include <opencv2/opencv.hpp>
// #include <opencv2/dnn.hpp> // Include for DNN module
// #include <dlib/opencv.h>
// #include <dlib/image_processing/frontal_face_detector.h>
// #include <dlib/image_processing.h>

// #include "patches-face-detection.h"
// #include "plugin-support.h"

// using namespace dlib;
// using namespace std;

// static struct vec4 getBoundingBox(const std::vector<cv::Point> &landmarks, uint32_t width, uint32_t height)
// {
// 	float minX = std::numeric_limits<float>::max();
// 	float maxX = std::numeric_limits<float>::lowest();
// 	float minY = std::numeric_limits<float>::max();
// 	float maxY = std::numeric_limits<float>::lowest();

// 	for (const auto &landmark : landmarks) {
// 		minX = std::min(minX, static_cast<float>(landmark.x));
// 		maxX = std::max(maxX, static_cast<float>(landmark.x));
// 		minY = std::min(minY, static_cast<float>(landmark.y));
// 		maxY = std::max(maxY, static_cast<float>(landmark.y));
// 	}
// 	struct vec4 rect;
// 	vec4_set(&rect, minX / width, maxX / width, minY / height, maxY / height);
// 	return rect;
// }

// std::vector<std::vector<bool>> faceMask(struct input_BGRA_data *frame, std::vector<struct vec4> &face_coordinates)
// {
// 	obs_log(LOG_INFO, "Start face mask!!!!");
// 	// Extract frame parameters
// 	uint8_t *data = frame->data;
// 	uint32_t width = frame->width;
// 	uint32_t height = frame->height;
// 	uint32_t linesize = frame->linesize;

// 	// Convert BGRA to OpenCV Mat
// 	cv::Mat frameMat(height, width, CV_8UC4, data, linesize);

// 	// Convert BGRA to BGR for Dlib processing
// 	cv::Mat frameBGR;
// 	cv::cvtColor(frameMat, frameBGR, cv::COLOR_BGRA2BGR);

// 	obs_log(LOG_INFO, "Dlib initialization");

// 	char *face_landmark_path =
// 		obs_find_module_file(obs_get_module("pulse-obs"), "shape_predictor_68_face_landmarks.dat");

// 	if (!face_landmark_path) {
// 		obs_log(LOG_ERROR, "Failed to find face landmark file");
// 		throw std::runtime_error("Failed to find face landmark file");
// 	}

// 	// Initialize dlib shape predictor
// 	shape_predictor sp;
// 	obs_log(LOG_INFO, "Dlib shape predictor!!!!");
// 	deserialize(face_landmark_path) >> sp;
// 	obs_log(LOG_INFO, "Dlib deserialize!!!!");

// 	obs_log(LOG_INFO, "Detect faces using DNN!!!!");

// 	// Load OpenCV DNN face detection model (Caffe-based)
// 	std::string modelConfiguration = obs_find_module_file(obs_get_module("pulse-obs"), "deploy.prototxt");
// 	std::string modelWeights =
// 		obs_find_module_file(obs_get_module("pulse-obs"), "res10_300x300_ssd_iter_140000_fp16.caffemodel");

// 	// Load the pre-trained model
// 	cv::dnn::Net net = cv::dnn::readNetFromCaffe(modelConfiguration, modelWeights);

// 	// Convert frame to blob for the DNN model
// 	cv::Mat blob;
// 	cv::dnn::blobFromImage(frameBGR, blob, 1.0, cv::Size(300, 300), cv::Scalar(104.0, 177.0, 123.0), false, false);

// 	// Set the input blob for the network
// 	net.setInput(blob);

// 	// Run forward pass to get detections
// 	cv::Mat detections = net.forward();

// 	// Create a binary mask (black background, white face)
// 	cv::Mat mask = cv::Mat::zeros(frameBGR.size(), CV_8UC1);

// 	// Loop over detections and process each face
// 	for (int i = 0; i < detections.size[2]; ++i) {
// 		float confidence = detections.at<float>(i, 2);
// 		if (confidence > 0.5) { // Confidence threshold
// 			// Get the bounding box for the detected face
// 			int x1 = static_cast<int>(detections.at<float>(i, 3) * width);
// 			int y1 = static_cast<int>(detections.at<float>(i, 4) * height);
// 			int x2 = static_cast<int>(detections.at<float>(i, 5) * width);
// 			int y2 = static_cast<int>(detections.at<float>(i, 6) * height);

// 			// Create a rectangle from the bounding box
// 			rectangle faceRect(x1, y1, x2 - x1, y2 - y1);

// 			// Use dlib's shape predictor to detect landmarks
// 			full_object_detection shape = sp(cv_image<bgr_pixel>(frameBGR), faceRect);

// 			// Store face contour
// 			std::vector<cv::Point> faceContour;
// 			for (int i = 0; i < 17; i++) { // Jawline (points 1–17)
// 				faceContour.push_back(cv::Point(shape.part(i).x(), shape.part(i).y()));
// 			}

// 			// Fill face area (excluding eyes/mouth)
// 			cv::fillConvexPoly(mask, faceContour, cv::Scalar(255));

// 			// Exclude eyes and mouth
// 			std::vector<cv::Point> leftEyes;
// 			std::vector<cv::Point> rightEyes;
// 			std::vector<cv::Point> mouth;
// 			for (int i = 36; i <= 41; i++) { // Right eye (37–42)
// 				leftEyes.push_back(cv::Point(shape.part(i).x(), shape.part(i).y()));
// 			}
// 			for (int i = 42; i <= 47; i++) { // Left eye (43–47)
// 				rightEyes.push_back(cv::Point(shape.part(i).x(), shape.part(i).y()));
// 			}
// 			for (int i = 48; i <= 60; i++) { // Mouth (48–60)
// 				mouth.push_back(cv::Point(shape.part(i).x(), shape.part(i).y()));
// 			}

// 			face_coordinates.push_back(getBoundingBox(leftEyes, width, height));
// 			face_coordinates.push_back(getBoundingBox(rightEyes, width, height));
// 			face_coordinates.push_back(getBoundingBox(mouth, width, height));

// 			// Black out eyes and mouth in the mask
// 			cv::fillConvexPoly(mask, leftEyes, cv::Scalar(0));
// 			cv::fillConvexPoly(mask, rightEyes, cv::Scalar(0));
// 			cv::fillConvexPoly(mask, mouth, cv::Scalar(0));
// 		}
// 	}

// 	obs_log(LOG_INFO, "Finish processing faces!!!!");

// 	// Convert mask to a 2D boolean vector
// 	std::vector<std::vector<bool>> binaryMask(height, std::vector<bool>(width, false));
// 	for (uint32_t y = 0; y < height; y++) {
// 		for (uint32_t x = 0; x < width; x++) {
// 			binaryMask[y][x] = (mask.at<uint8_t>(y, x) > 0);
// 		}
// 	}

// 	bfree(face_landmark_path);

// 	return binaryMask;
// }

#include <opencv2/opencv.hpp>
#include <dlib/opencv.h>
#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/image_processing.h>

#include "patches-face-detection.h"
#include "plugin-support.h"

using namespace dlib;
using namespace std;

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

	char *face_landmark_path =
		obs_find_module_file(obs_get_module("pulse-obs"), "shape_predictor_68_face_landmarks.dat");

	if (!face_landmark_path) {
		obs_log(LOG_ERROR, "Failed to find face landmark file");
		throw std::runtime_error("Failed to find face landmark file");
	}

	// Initialize Dlib face detector and shape predictor
	frontal_face_detector detector = get_frontal_face_detector();
	obs_log(LOG_INFO, "Dlib detector!!!!");
	shape_predictor sp;
	obs_log(LOG_INFO, "Dlib shape predictor!!!!");
	deserialize(face_landmark_path) >> sp;
	obs_log(LOG_INFO, "Dlib deserialize!!!!");

	obs_log(LOG_INFO, "Detect faces!!!!");
	// Detect faces
	std::vector<rectangle> faces = detector(cv_image<bgr_pixel>(frameBGR));

	// Create a binary mask (black background, white face)
	cv::Mat mask = cv::Mat::zeros(frameBGR.size(), CV_8UC1);

	for (auto &face : faces) {
		full_object_detection shape = sp(cv_image<bgr_pixel>(frameBGR), face);

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
	obs_log(LOG_INFO, "Finish processing faces!!!!");

	// Convert mask to a 2D boolean vector
	std::vector<std::vector<bool>> binaryMask(height, std::vector<bool>(width, false));
	for (uint32_t y = 0; y < height; y++) {
		for (uint32_t x = 0; x < width; x++) {
			binaryMask[y][x] = (mask.at<uint8_t>(y, x) > 0);
		}
	}

	bfree(face_landmark_path);

	return binaryMask;
}