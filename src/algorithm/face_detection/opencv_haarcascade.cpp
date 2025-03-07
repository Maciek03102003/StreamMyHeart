#include "opencv_haarcascade.h"

#include <graphics/matrix4.h>
#include <algorithm>

static void loadCascade(cv::CascadeClassifier &cascade, const char *module_name, const char *file_name)
{
	char *cascadePath = obs_module_file(file_name);
	if (!cascadePath) {
		obs_log(LOG_INFO, "Error finding %s file!", file_name);
		throw std::runtime_error("Error finding cascade file!");
	}

	if (!cascade.load(cascadePath)) {
		obs_log(LOG_INFO, "Error loading %s!", file_name);
		throw std::runtime_error("Error loading cascade!");
	}

	bfree(cascadePath);
}

// Ensure the face cascade is loaded once
void HaarCascadeFaceDetection::initializeFaceCascade(bool evaluation)
{
	if (!cascadeLoaded) {
		if (evaluation) {
			faceCascade.load("./haarcascade_frontalface_default.xml");
			mouthCascade.load("./haarcascade_mcs_mouth.xml");
			leftEyeCascade.load("./haarcascade_lefteye_2splits.xml");
			rightEyeCascade.load("./haarcascade_righteye_2splits.xml");
			cascadeLoaded = true;
		} else {
			loadCascade(faceCascade, "stream-my-heart", "haarcascade_frontalface_default.xml");
			loadCascade(mouthCascade, "stream-my-heart", "haarcascade_mcs_mouth.xml");
			loadCascade(leftEyeCascade, "stream-my-heart", "haarcascade_lefteye_2splits.xml");
			loadCascade(rightEyeCascade, "stream-my-heart", "haarcascade_righteye_2splits.xml");
			cascadeLoaded = true;
		}
	}
}

// Normalise the rectangle coordinates to pass to the effect files for drawing boxes
static struct vec4 getNormalisedRect(const cv::Rect &region, uint32_t width, uint32_t height)
{
	float normMinX = static_cast<float>(region.x) / width;
	float normMaxX = static_cast<float>(region.x + region.width) / width;
	float normMinY = static_cast<float>(region.y) / height;
	float normMaxY = static_cast<float>(region.y + region.height) / height;

	struct vec4 rect;
	vec4_set(&rect, normMinX, normMaxX, normMinY, normMaxY);
	return rect;
}

// Function to detect faces and create a mask
std::vector<double_t> HaarCascadeFaceDetection::detectFace(std::shared_ptr<struct input_BGRA_data> frame,
							   std::vector<struct vec4> &faceCoordinates,
							   bool enableDebugBoxes, bool enableTracker,
							   int frameUpdateInterval, bool evaluation)
{
	UNUSED_PARAMETER(frameUpdateInterval);
	UNUSED_PARAMETER(enableTracker);

	if (!frame || !frame->data) {
		throw std::runtime_error("Invalid BGRA frame data!");
	}

	// Initialize the face cascade
	initializeFaceCascade(evaluation);

	// Extract frame parameters
	uint8_t *data = frame->data;
	uint32_t width = frame->width;
	uint32_t height = frame->height;
	uint32_t linesize = frame->linesize;

	// Initialize a 2D boolean mask
	std::vector<std::vector<bool>> faceMask(height, std::vector<bool>(width, false));

	// Create an OpenCV Mat for the BGRA frame
	// `linesize` specifies the number of bytes per row, which can include padding
	cv::Mat bgraFrame(height, linesize / 4, CV_8UC4, data);

	// Crop to remove padding if linesize > width * 4
	cv::Mat croppedBgraFrame = bgraFrame(cv::Rect(0, 0, width, height));

	// Convert BGRA to BGR (OpenCV processes images in BGR format)
	cv::Mat bgrFrame;
	cv::cvtColor(croppedBgraFrame, bgrFrame, cv::COLOR_BGRA2BGR);

	// frameCount++;
	// bool resetFaceDetection = frameCount % 3 == 0;

	// if (!resetFaceDetection) {
	// 	if (noFaceDetected) {
	// 		return std::vector<double_t>(3, 0.0);
	// 	}

	// 	// Now compute the mean color in the face region (where maskMat is nonzero)
	// 	cv::Scalar meanRGB = cv::mean(bgrFrame, maskMat);

	// 	// Convert the result to a vector<double_t> (B, G, R order)
	// 	std::vector<double_t> avgRGB = {meanRGB[0], meanRGB[1], meanRGB[2]};

	// 	faceCoordinates = faceCoordinatesCopy;
	// 	return avgRGB;
	// } else {
	// 	frameCount = 0;
	// }

	// Detect faces
	std::vector<cv::Rect> faces;
	faceCascade.detectMultiScale(bgrFrame, faces, 1.1, 10, 0, cv::Size(30, 30));

	// Detect eyes and mouth within detected faces
	cv::Rect initialFace;
	if (!faces.empty()) {
		noFaceDetected = false;
		initialFace = faces[0]; // Assume first detected face is the target
	} else {
		noFaceDetected = true;
		maskMat.release();           // Frees memory and makes it an empty matrix
		faceCoordinatesCopy.clear(); // Clears all elements, size becomes 0
		// If no face detected, return empty mask
		return std::vector<double_t>(3, 0.0);
	}

	if (enableDebugBoxes) {
		// Push absolute face bounding box as normalized coordinates
		faceCoordinates.push_back(getNormalisedRect(initialFace, width, height));
	}

	// Define region of interest (ROI) for eyes and mouth
	cv::Mat faceROI = bgrFrame(initialFace);
	cv::Mat grayFaceROI;
	cv::cvtColor(faceROI, grayFaceROI, cv::COLOR_BGR2GRAY);

	cv::Mat lowerFaceROI =
		grayFaceROI(cv::Rect(0, grayFaceROI.rows / 2, grayFaceROI.cols, faceROI.rows / 2)); // Lower half
	cv::Mat leftFaceROI = grayFaceROI(cv::Rect(0, 0, grayFaceROI.cols / 2, grayFaceROI.rows));  // Upper half
	cv::Mat rightFaceROI = grayFaceROI(cv::Rect(grayFaceROI.cols / 2, 0, grayFaceROI.cols / 2, grayFaceROI.rows));

	// Detect left eyes
	std::vector<cv::Rect> leftEyes;
	leftEyeCascade.detectMultiScale(leftFaceROI, leftEyes, 1.1, 10, 0, cv::Size(15, 15));
	cv::Rect absoluteLeftEye;
	if (!leftEyes.empty()) {
		const auto &eye = leftEyes[0];
		// Calculate absolute coordinates for the eye
		absoluteLeftEye = cv::Rect(eye.x + initialFace.x, eye.y + initialFace.y, eye.width, eye.height);

		if (enableDebugBoxes) {
			// Push absolute eye bounding box as normalized coordinates
			faceCoordinates.push_back(getNormalisedRect(absoluteLeftEye, width, height));
		}
	}

	// Detect right eyes
	std::vector<cv::Rect> rightEyes;
	rightEyeCascade.detectMultiScale(rightFaceROI, rightEyes, 1.1, 10, 0, cv::Size(15, 15));
	cv::Rect absoluteRightEye;
	if (!rightEyes.empty()) {
		const auto &eye = rightEyes[0];
		// Calculate absolute coordinates for the eye
		absoluteRightEye = cv::Rect(eye.x + initialFace.x + initialFace.width / 2, eye.y + initialFace.y,
					    eye.width, eye.height);

		if (enableDebugBoxes) {
			// Push absolute eye bounding box as normalized coordinates
			faceCoordinates.push_back(getNormalisedRect(absoluteRightEye, width, height));
		}
	}

	// Detect mouth in the lower half of the face ROI
	std::vector<cv::Rect> mouths;
	mouthCascade.detectMultiScale(lowerFaceROI, mouths, 1.05, 35, 0, cv::Size(30, 15));
	cv::Rect absoluteMouth;
	if (!mouths.empty()) {
		const auto &mouth = mouths[0];
		// Calculate absolute coordinates for the mouth
		absoluteMouth = cv::Rect(mouth.x + initialFace.x, mouth.y + initialFace.y + faceROI.rows / 2,
					 mouth.width, mouth.height);

		if (enableDebugBoxes) {
			// Push absolute mouth bounding box as normalized coordinates
			faceCoordinates.push_back(getNormalisedRect(absoluteMouth, width, height));
		}
	}

	faceCoordinatesCopy = faceCoordinates;

	// Instead of returning a 2D boolean mask, create an OpenCV mask image
	// that we will use to compute the average RGB.
	maskMat = cv::Mat::zeros(bgrFrame.size(), CV_8UC1);

	// Mark the entire face region (initialFace) as valid (white)
	cv::rectangle(maskMat, initialFace, cv::Scalar(255), cv::FILLED);

	// Remove (mask out) the detected left eye, right eye, and mouth regions by setting them to black.
	if (!leftEyes.empty()) {
		cv::rectangle(maskMat, absoluteLeftEye, cv::Scalar(0), cv::FILLED);
	}
	if (!rightEyes.empty()) {
		cv::rectangle(maskMat, absoluteRightEye, cv::Scalar(0), cv::FILLED);
	}
	if (!mouths.empty()) {
		cv::rectangle(maskMat, absoluteMouth, cv::Scalar(0), cv::FILLED);
	}

	// Now compute the mean color in the face region (where maskMat is nonzero)
	cv::Scalar meanRGB = cv::mean(bgrFrame, maskMat);

	// Convert the result to a vector<double_t> (B, G, R order)
	std::vector<double_t> avgRGB = {meanRGB[0], meanRGB[1], meanRGB[2]};

	return avgRGB;
}
