#include <opencv2/opencv.hpp>
#include <dlib/opencv.h>
#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/image_processing.h>

#include "patches-face-detection.h"

using namespace dlib;
using namespace std;

std::vector<double_t> faceMask(struct input_BGRA_data *frame)
{
	// Extract frame parameters
	uint8_t *data = frame->data;
	uint32_t width = frame->width;
	uint32_t height = frame->height;
	uint32_t linesize = frame->linesize;

	// Convert BGRA to OpenCV Mat
	cv::Mat frameOriginal(height, width, CV_8UC4, data, linesize);

	// frame = frame(cv::Rect(0, 0, width, height));

	// Convert BGRA to BGR (dlib works with BGR format)
	cv::Mat frameBGR;
	cv::cvtColor(frameOriginal, frameBGR, cv::COLOR_BGRA2BGR);

	// Initialize dlib face detector and shape predictor
	frontal_face_detector detector = get_frontal_face_detector();
	shape_predictor sp;
	deserialize("shape_predictor_68_face_landmarks.dat") >> sp;

	std::vector<rectangle> faces = detector(cv_image<bgr_pixel>(frameBGR));

	// Create a mask for the skin region
	cv::Mat skinMask = cv::Mat::zeros(frameBGR.size(), CV_8UC1);

	for (auto &face : faces) {
		full_object_detection shape = sp(cv_image<bgr_pixel>(frameBGR), face);

		// Create a vector of points for the face (jawline, etc.)
		std::vector<cv::Point> faceContour;
		for (int i = 0; i < 17; i++) { // Jawline (1–17)
			faceContour.push_back(cv::Point(shape.part(i).x(), shape.part(i).y()));
		}

		// Draw the face contour on the mask (exclude hair, eyes, and mouth)
		cv::fillConvexPoly(skinMask, faceContour, cv::Scalar(255));

		// Now exclude the eyes and mouth using landmarks
		std::vector<cv::Point> eyePoints, mouthPoints;
		// Eyes: 36-41 (right) and 42-47 (left)
		for (int i = 36; i <= 41; i++) {
			eyePoints.push_back(cv::Point(shape.part(i).x(), shape.part(i).y()));
		}
		for (int i = 42; i <= 47; i++) {
			eyePoints.push_back(cv::Point(shape.part(i).x(), shape.part(i).y()));
		}

		// Mouth: 48-59
		for (int i = 48; i <= 59; i++) {
			mouthPoints.push_back(cv::Point(shape.part(i).x(), shape.part(i).y()));
		}

		// Draw eyes and mouth as black on the skin mask to exclude them
		cv::fillConvexPoly(skinMask, eyePoints, cv::Scalar(0));
		cv::fillConvexPoly(skinMask, mouthPoints, cv::Scalar(0));
	}

	// Optional: Apply skin color segmentation (HSV range for skin)
	cv::Mat hsvImage;
	cv::cvtColor(frameBGR, hsvImage, cv::COLOR_BGR2HSV);
	cv::Mat skinRegion;
	cv::inRange(hsvImage, cv::Scalar(0, 20, 70), cv::Scalar(20, 255, 255), skinRegion);

	// Combine the mask with skin color segmentation to refine the result
	cv::bitwise_and(skinMask, skinRegion, skinMask);

	// Apply the skin mask to the frame
	cv::Mat result;
	// Returns the original frame but with all non-face + eyes + mouth regions as black so [0, 0, 0]
	// And face regions is 
	frameBGR.copyTo(result, skinMask);

	return result;
}
