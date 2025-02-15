#include "algorithm/face_detection/opencv_haarcascade.h"
#include "../src/algorithm/heart_rate_algorithm.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <opencv2/opencv.hpp>

struct VideoData {
	std::string videoPath;
	std::vector<double> groundTruthHeartRate;
	double otherAlgorithmRMSE;
	double otherAlgorithmMAE;
};

std::vector<VideoData> readCSV(const std::string &csvFilePath)
{
	std::vector<VideoData> videoDataList;
	std::ifstream file(csvFilePath);
	std::string line;

	while (std::getline(file, line)) {
		std::istringstream ss(line);
		std::string videoPath;
		std::vector<double> groundTruthHeartRates;
		std::string token;

		// Read the video path
		std::getline(ss, videoPath, ',');

		// Read the ground truth heart rates
		std::getline(ss, token, ','); // Skip the initial '['
		while (std::getline(ss, token, ',')) {
			if (token == "]")
				break;
			groundTruthHeartRates.push_back(std::stod(token));
		}

		// Read the other algorithm's RMSE and MAE
		double otherAlgorithmRMSE, otherAlgorithmMAE;
		ss >> otherAlgorithmRMSE;
		ss.ignore(1); // Ignore the comma
		ss >> otherAlgorithmMAE;

		videoDataList.push_back({videoPath, groundTruthHeartRates, otherAlgorithmRMSE, otherAlgorithmMAE});
	}
	return videoDataList;
}

// Function to extract BGRA data from a video frame
input_BGRA_data extractBGRAData(const cv::Mat &frame)
{
	input_BGRA_data bgraData;
	bgraData.width = frame.cols;
	bgraData.height = frame.rows;
	bgraData.linesize = static_cast<uint32_t>(frame.step);
	bgraData.data = new uint8_t[frame.total() * frame.elemSize()];
	std::memcpy(bgraData.data, frame.data, frame.total() * frame.elemSize());
	return bgraData;
}

double calculateMAE(const std::vector<double> &actual, const std::vector<double> &predicted)
{
	double mae = 0.0;
	for (size_t i = 0; i < actual.size(); ++i) {
		mae += std::fabs(actual[i] - predicted[i]); // Absolute error
	}
	return mae / actual.size();
}

double calculateRMSE(const std::vector<double> &actual, const std::vector<double> &predicted)
{
	double rmse = 0.0;
	for (size_t i = 0; i < actual.size(); ++i) {
		rmse += std::pow(actual[i] - predicted[i], 2); // Squared error
	}
	return std::sqrt(rmse / actual.size());
}

std::vector<double> calculateHeartRateForVideo(const VideoData &videoData)
{ // {MAE, RMSE}
	cv::VideoCapture cap(videoData.videoPath);
	if (!cap.isOpened()) {
		std::cerr << "Error: Could not open video file " << videoData.videoPath << std::endl;
		return {};
	}

	MovingAvg movingAvg;
	HaarCascadeFaceDetection faceDetection;
	cv::Mat frame;

	int fps = static_cast<int>(cap.get(cv::CAP_PROP_FPS));

	std::vector<double> predicted;

	while (cap.read(frame)) {
		std::vector<struct vec4> faceCoordinates;

		// Convert the frame to BGRA format
		cv::Mat bgraFrame;
		cv::cvtColor(frame, bgraFrame, cv::COLOR_BGR2BGRA);

		// Extract BGRA data
		input_BGRA_data bgraData = extractBGRAData(bgraFrame);

		// Perform face detection
		std::vector<double_t> avg = faceDetection.detectFace(&bgraData, faceCoordinates, false, true, 60, true);

		// Calculate heart rate using your algorithm
		double heartRate = movingAvg.calculateHeartRate(avg, 0, 1, 0, fps);
		if (heartRate != 0) {
			predicted.push_back(heartRate);
		}

		// Clean up
		delete[] bgraData.data;
	}

	cap.release();
	return predicted;
}

// Function to center-align text within a field of a given width
std::string centerAlign(const std::string &text, int width)
{
	int padding = width - static_cast<int>(text.size());
	if (padding <= 0)
		return text;
	int padLeft = padding / 2;
	int padRight = padding - padLeft;
	return std::string(padLeft, ' ') + text + std::string(padRight, ' ');
}

void evaluateHeartRate(const std::string &csvFilePath)
{
	std::vector<VideoData> videoDataList = readCSV(csvFilePath);

	// Print the table header
	std::cout
		<< "| Test Subject | Our Algorithm MAE | Other Algorithm MAE | Our Algorithm RMSE | Other Algorithm RMSE |\n";
	std::cout
		<< "|--------------|-------------------|---------------------|--------------------|----------------------|\n";

	// Open the CSV file for writing
	std::ofstream outFile("evaluation_results.csv");
	outFile << "Test Subject,Our Algorithm MAE,Other Algorithm MAE,Our Algorithm RMSE,Other Algorithm RMSE\n";

	for (const auto &videoData : videoDataList) {
		std::vector<double> predicted = calculateHeartRateForVideo(videoData);
		double ourAlgorithmRMSE = calculateRMSE(videoData.groundTruthHeartRate, predicted);
		double ourAlgorithmMAE = calculateMAE(videoData.groundTruthHeartRate, predicted);

		// Extract the subject name from the video path
		std::string subjectName = videoData.videoPath.substr(videoData.videoPath.find_last_of("/") + 1);
		subjectName = subjectName.substr(0, subjectName.find("."));

		// Convert numbers to strings with fixed precision
		std::string ourAlgorithmMAEStr = std::to_string(ourAlgorithmMAE);
		std::string otherAlgorithmMAEStr = std::to_string(videoData.otherAlgorithmMAE);
		std::string ourAlgorithmRMSEStr = std::to_string(ourAlgorithmRMSE);
		std::string otherAlgorithmRMSEStr = std::to_string(videoData.otherAlgorithmRMSE);

		// Center-align the text and numbers
		std::cout << "| " << std::setw(12) << std::left << centerAlign(subjectName, 12) << " | "
			  << std::setw(17) << std::left << centerAlign(ourAlgorithmMAEStr, 17) << " | " << std::setw(19)
			  << std::left << centerAlign(otherAlgorithmMAEStr, 19) << " | " << std::setw(18) << std::left
			  << centerAlign(ourAlgorithmRMSEStr, 18) << " | " << std::setw(20) << std::left
			  << centerAlign(otherAlgorithmRMSEStr, 20) << " |\n";

		// Write the results to the CSV file
		outFile << subjectName << "," << ourAlgorithmMAEStr << "," << otherAlgorithmMAEStr << ","
			<< ourAlgorithmRMSEStr << "," << otherAlgorithmRMSEStr << "\n";
	}

	// Close the CSV file
	outFile.close();
}

int main()
{
	std::string csvFilePath = "../../../../../eval/ground_truth.csv";

	evaluateHeartRate(csvFilePath);

	std::cout << "Press Enter to exit..." << std::endl;
	std::cin.get(); // Wait for user input before closing

	return 0;
}
