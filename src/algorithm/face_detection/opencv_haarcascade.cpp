#include "opencv_haarcascade.h"

#include <graphics/matrix4.h>
#include <algorithm>

// Static variables for face detection
static cv::CascadeClassifier face_cascade, mouth_cascade, left_eye_cascade, right_eye_cascade;
static bool cascade_loaded = false;

static void loadCascade(cv::CascadeClassifier &cascade, const char *module_name, const char *file_name)
{
	char *cascade_path = obs_find_module_file(obs_get_module(module_name), file_name);
	if (!cascade_path) {
		obs_log(LOG_INFO, "Error finding %s file!", file_name);
		throw std::runtime_error("Error finding cascade file!");
	}

	if (!cascade.load(cascade_path)) {
		obs_log(LOG_INFO, "Error loading %s!", file_name);
		throw std::runtime_error("Error loading cascade!");
	}

	bfree(cascade_path);
}

// Ensure the face cascade is loaded once
static void initializeFaceCascade()
{
	if (!cascade_loaded) {
		loadCascade(face_cascade, "pulse-obs", "haarcascade_frontalface_default.xml");
		loadCascade(mouth_cascade, "pulse-obs", "haarcascade_mcs_mouth.xml");
		loadCascade(left_eye_cascade, "pulse-obs", "haarcascade_lefteye_2splits.xml");
		loadCascade(right_eye_cascade, "pulse-obs", "haarcascade_righteye_2splits.xml");
		cascade_loaded = true;
	}
}

// Normalise the rectangle coordinates to pass to the effect files for drawing boxes
static struct vec4 getNormalisedRect(const cv::Rect &region, uint32_t width, uint32_t height)
{
	float norm_min_x = static_cast<float>(region.x) / width;
	float norm_max_x = static_cast<float>(region.x + region.width) / width;
	float norm_min_y = static_cast<float>(region.y) / height;
	float norm_max_y = static_cast<float>(region.y + region.height) / height;

	struct vec4 rect;
	vec4_set(&rect, norm_min_x, norm_max_x, norm_min_y, norm_max_y);
	return rect;
}

// Function to detect faces and create a mask
std::vector<double_t> detectFacesAndCreateMask(struct input_BGRA_data *frame,
					       std::vector<struct vec4> &face_coordinates)
{
	if (!frame || !frame->data) {
		throw std::runtime_error("Invalid BGRA frame data!");
	}

	// Initialize the face cascade
	initializeFaceCascade();

	// Extract frame parameters
	uint8_t *data = frame->data;
	uint32_t width = frame->width;
	uint32_t height = frame->height;
	uint32_t linesize = frame->linesize;

	// Initialize a 2D boolean mask
	std::vector<std::vector<bool>> face_mask(height, std::vector<bool>(width, false));

	// Create an OpenCV Mat for the BGRA frame
	// `linesize` specifies the number of bytes per row, which can include padding
	cv::Mat bgra_frame(height, linesize / 4, CV_8UC4, data);

	// Crop to remove padding if linesize > width * 4
	cv::Mat cropped_bgra_frame = bgra_frame(cv::Rect(0, 0, width, height));

	// Convert BGRA to BGR (OpenCV processes images in BGR format)
	cv::Mat bgr_frame;
	cv::cvtColor(cropped_bgra_frame, bgr_frame, cv::COLOR_BGRA2BGR);

	// Detect faces
	std::vector<cv::Rect> faces;
	face_cascade.detectMultiScale(bgr_frame, faces, 1.1, 10, 0, cv::Size(30, 30));

	// Detect eyes and mouth within detected faces
	cv::Rect initial_face;
	if (!faces.empty()) {
		initial_face = faces[0]; // Assume first detected face is the target
	} else {
		// If no face detected, return empty mask
		return std::vector<double_t>(3, 0.0);
	}
	face_coordinates.push_back(getNormalisedRect(initial_face, width, height));

	// Define region of interest (ROI) for eyes and mouth
	cv::Mat faceROI = bgr_frame(initial_face);
	cv::Mat gray_faceROI;
	cv::cvtColor(faceROI, gray_faceROI, cv::COLOR_BGR2GRAY);

	cv::Mat lowerFaceROI =
		gray_faceROI(cv::Rect(0, gray_faceROI.rows / 2, gray_faceROI.cols, faceROI.rows / 2)); // Lower half
	cv::Mat leftFaceROI = gray_faceROI(cv::Rect(0, 0, gray_faceROI.cols / 2, gray_faceROI.rows));  // Upper half
	cv::Mat rightFaceROI =
		gray_faceROI(cv::Rect(gray_faceROI.cols / 2, 0, gray_faceROI.cols / 2, gray_faceROI.rows));

	// Detect left eyes
	std::vector<cv::Rect> left_eyes;
	left_eye_cascade.detectMultiScale(leftFaceROI, left_eyes, 1.1, 10, 0, cv::Size(15, 15));
	cv::Rect absolute_left_eye;
	if (!left_eyes.empty()) {
		const auto &eye = left_eyes[0];
		// Calculate absolute coordinates for the eye
		absolute_left_eye = cv::Rect(eye.x + initial_face.x, eye.y + initial_face.y, eye.width, eye.height);

		// Push absolute eye bounding box as normalized coordinates
		face_coordinates.push_back(getNormalisedRect(absolute_left_eye, width, height));
	}

	// Detect right eyes
	std::vector<cv::Rect> right_eyes;
	right_eye_cascade.detectMultiScale(rightFaceROI, right_eyes, 1.1, 10, 0, cv::Size(15, 15));
	cv::Rect absolute_right_eye;
	if (!right_eyes.empty()) {
		const auto &eye = right_eyes[0];
		// Calculate absolute coordinates for the eye
		absolute_right_eye = cv::Rect(eye.x + initial_face.x + initial_face.width / 2, eye.y + initial_face.y,
					      eye.width, eye.height);

		// Push absolute eye bounding box as normalized coordinates
		face_coordinates.push_back(getNormalisedRect(absolute_right_eye, width, height));
	}

	// Detect mouth in the lower half of the face ROI
	std::vector<cv::Rect> mouths;
	mouth_cascade.detectMultiScale(lowerFaceROI, mouths, 1.05, 35, 0, cv::Size(30, 15));
	cv::Rect absolute_mouth;
	if (!mouths.empty()) {
		const auto &mouth = mouths[0];
		// Calculate absolute coordinates for the mouth
		absolute_mouth = cv::Rect(mouth.x + initial_face.x, mouth.y + initial_face.y + faceROI.rows / 2,
					  mouth.width, mouth.height);

		// Push absolute mouth bounding box as normalized coordinates
		face_coordinates.push_back(getNormalisedRect(absolute_mouth, width, height));
	}

	// Instead of returning a 2D boolean mask, create an OpenCV mask image
	// that we will use to compute the average RGB.
	cv::Mat maskMat = cv::Mat::zeros(bgr_frame.size(), CV_8UC1);

	// Mark the entire face region (initial_face) as valid (white)
	cv::rectangle(maskMat, initial_face, cv::Scalar(255), cv::FILLED);

	// Remove (mask out) the detected left eye, right eye, and mouth regions by setting them to black.
	if (!left_eyes.empty()) {
		cv::rectangle(maskMat, absolute_left_eye, cv::Scalar(0), cv::FILLED);
	}
	if (!right_eyes.empty()) {
		cv::rectangle(maskMat, absolute_right_eye, cv::Scalar(0), cv::FILLED);
	}
	if (!mouths.empty()) {
		cv::rectangle(maskMat, absolute_mouth, cv::Scalar(0), cv::FILLED);
	}

	// Now compute the mean color in the face region (where maskMat is nonzero)
	cv::Scalar meanRGB = cv::mean(bgr_frame, maskMat);

	// Convert the result to a vector<double_t> (B, G, R order)
	std::vector<double_t> avgRGB = {meanRGB[0], meanRGB[1], meanRGB[2]};

	return avgRGB;
}
