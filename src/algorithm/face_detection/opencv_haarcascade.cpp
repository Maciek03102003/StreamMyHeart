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

// Mark pixels within detected regions as true/false depending on whether its the face/eyes/mouth
static void mask_face(std::vector<std::vector<bool>> face_mask, cv::Rect rect, bool is_face)
{
	for (int y = rect.y; y < rect.y + rect.height; ++y) {
		for (int x = rect.x; x < rect.x + rect.width; ++x) {
			face_mask[y][x] = is_face;
		}
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
std::vector<std::vector<bool>> detectFacesAndCreateMask(struct input_BGRA_data *frame,
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
	for (size_t i = 0; i < faces.size(); i++) {
		face_coordinates.push_back(getNormalisedRect(faces[i], width, height));

		// Define region of interest (ROI) for eyes and mouth
		cv::Mat faceROI = bgr_frame(faces[i]);
		cv::Mat gray_faceROI;
		cv::cvtColor(faceROI, gray_faceROI, cv::COLOR_BGR2GRAY);
		cv::Mat upperFaceROI =
			gray_faceROI(cv::Rect(0, 0, gray_faceROI.cols, gray_faceROI.rows / 2)); // Upper half
		cv::Mat lowerFaceROI = gray_faceROI(
			cv::Rect(0, gray_faceROI.rows / 2, gray_faceROI.cols, faceROI.rows / 2)); // Lower half

		// Detect left eyes
		std::vector<cv::Rect> left_eyes;
		left_eye_cascade.detectMultiScale(upperFaceROI, left_eyes, 1.1, 10, 0, cv::Size(15, 15));
		std::vector<vec4> left_eye_rects;
		for (size_t i = 0; i < std::min(static_cast<size_t>(1), left_eyes.size()); i++) {
			const auto &eye = left_eyes[i];
			// Calculate absolute coordinates for the eye
			cv::Rect absolute_eye(eye.x + faces[i].x, eye.y + faces[i].y, eye.width, eye.height);

			// Push absolute eye bounding box as normalized coordinates
			face_coordinates.push_back(getNormalisedRect(absolute_eye, width, height));
		}

		// Detect right eyes
		std::vector<cv::Rect> right_eyes;
		right_eye_cascade.detectMultiScale(upperFaceROI, right_eyes, 1.1, 10, 0, cv::Size(15, 15));
		std::vector<vec4> right_eye_rects;
		for (size_t i = 0; i < std::min(static_cast<size_t>(1), right_eyes.size()); i++) {
			const auto &eye = right_eyes[i];
			// Calculate absolute coordinates for the eye
			cv::Rect absolute_eye(eye.x + faces[i].x, eye.y + faces[i].y, eye.width, eye.height);

			// Push absolute eye bounding box as normalized coordinates
			face_coordinates.push_back(getNormalisedRect(absolute_eye, width, height));
		}

		// Detect mouth in the lower half of the face ROI
		std::vector<cv::Rect> mouths;
		mouth_cascade.detectMultiScale(lowerFaceROI, mouths, 1.05, 35, 0, cv::Size(30, 15));
		std::vector<vec4> mouth_rects;
		for (size_t i = 0; i < std::min(static_cast<size_t>(1), mouths.size()); i++) {
			const auto &mouth = mouths[i];
			// Calculate absolute coordinates for the mouth
			cv::Rect absolute_mouth(mouth.x + faces[i].x, mouth.y + faces[i].y + faceROI.rows / 2,
						mouth.width, mouth.height);

			// Push absolute mouth bounding box as normalized coordinates
			face_coordinates.push_back(getNormalisedRect(absolute_mouth, width, height));
		}

		if (i == 0) {
			// Mark pixels within detected face regions as true
			mask_face(face_mask, faces[0], true);
			// Mark pixels within detected eye regions as false
			if (left_eyes.size() > 0) {
				mask_face(face_mask, left_eyes[0], false);
			}
			if (right_eyes.size() > 0) {
				mask_face(face_mask, right_eyes[0], false);
			}
			for (size_t i = 0; i < std::min(static_cast<size_t>(1), mouths.size()); i++) {
				// Mark pixels within detected mouth region as false
				mask_face(face_mask, mouths[i], false);
			}
		}
	}

	return face_mask;
}
