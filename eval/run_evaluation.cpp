#include <opencv2/opencv.hpp>
#include "algorithm/face_detection/face_detection.h"
#include "../src/algorithm/heart_rate_algorithm.h"

#include <thread>
#include <mutex>
#include <future>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

std::mutex outputMutex;

enum class PreFilteringAlgorithm { NONE, BUTTERWORTH_BANDPASS, DETREND, ZERO_MEAN, LAST };

enum class PPGAlgorithm {
	GREEN,
	PCA,
	CHROM,
};

enum class PostFilteringAlgorithm { NONE, BUTTERWORTH_BANDPASS, LAST };

std::string toString(FaceDetectionAlgorithm algo)
{
	switch (algo) {
	case FaceDetectionAlgorithm::HAAR_CASCADE:
		return "HAAR_CASCADE";
	case FaceDetectionAlgorithm::DLIB:
		return "DLIB";
	default:
		return "UNKNOWN";
	}
}

std::string toString(PreFilteringAlgorithm algo)
{
	switch (algo) {
	case PreFilteringAlgorithm::NONE:
		return "NONE";
	case PreFilteringAlgorithm::BUTTERWORTH_BANDPASS:
		return "BUTTERWORTH_BANDPASS";
	case PreFilteringAlgorithm::DETREND:
		return "DETREND";
	case PreFilteringAlgorithm::ZERO_MEAN:
		return "ZERO_MEAN";
	default:
		return "UNKNOWN";
	}
}

std::string toString(PPGAlgorithm algo)
{
	switch (algo) {
	case PPGAlgorithm::GREEN:
		return "GREEN";
	case PPGAlgorithm::PCA:
		return "PCA";
	case PPGAlgorithm::CHROM:
		return "CHROM";
	default:
		return "UNKNOWN";
	}
}

std::string toString(PostFilteringAlgorithm algo)
{
	switch (algo) {
	case PostFilteringAlgorithm::NONE:
		return "NONE";
	case PostFilteringAlgorithm::BUTTERWORTH_BANDPASS:
		return "BUTTERWORTH_BANDPASS";
	default:
		return "UNKNOWN";
	}
}

struct VideoData {
	std::string videoPath;
	std::vector<double> groundTruthHeartRate;
	double pcaRMSE;
	double pcaMAE;
	double chromRMSE;
	double chromMAE;
};

std::vector<VideoData> readCSV(const std::string &csvFilePath)
{
	static int calibrationTime = 5;
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
		int count = 0;
		while (std::getline(ss, token, ',')) {
			if (token == "]")
				break;
			if (count >= calibrationTime) { // Skip the first few heart rates for calibration
				groundTruthHeartRates.push_back(std::stod(token));
			}
			count++;
		}

		// Read the PCA RMSE and MAE
		double pcaRMSE, pcaMAE;
		ss >> pcaRMSE;
		ss.ignore(1); // Ignore the comma
		ss >> pcaMAE;
		ss.ignore(1); // Ignore the comma

		// Read the Chrom RMSE and MAE
		double chromRMSE, chromMAE;
		ss >> chromRMSE;
		ss.ignore(1); // Ignore the comma
		ss >> chromMAE;

		videoDataList.push_back({videoPath, groundTruthHeartRates, pcaRMSE, pcaMAE, chromRMSE, chromMAE});
		;
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
	// Use the smaller length of the two vectors
	size_t length = std::min(actual.size(), predicted.size());

	double mae = 0.0;
	for (size_t i = 0; i < length; ++i) {
		mae += std::fabs(actual[i] - predicted[i]); // Absolute error
	}
	return mae / length;
}

double calculateRMSE(const std::vector<double> &actual, const std::vector<double> &predicted)
{
	// Use the smaller length of the two vectors
	size_t length = std::min(actual.size(), predicted.size());

	double rmse = 0.0;
	for (size_t i = 0; i < length; ++i) {
		rmse += std::pow(actual[i] - predicted[i], 2); // Squared error
	}
	return std::sqrt(rmse / length);
}

std::vector<double> calculateHeartRateForVideo(const VideoData &videoData, FaceDetectionAlgorithm faceDetect,
					       PreFilteringAlgorithm preFilter, PPGAlgorithm ppg,
					       PostFilteringAlgorithm postFilter, bool smooth)
{
	cv::VideoCapture cap(videoData.videoPath);
	if (!cap.isOpened()) {
		std::cerr << "Error: Could not open video file " << videoData.videoPath << std::endl;
		return {};
	}

	MovingAvg movingAvg;
	std::unique_ptr<FaceDetection> faceDetection = FaceDetection::create(faceDetect);
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
		std::vector<double_t> avg =
			faceDetection->detectFace(&bgraData, faceCoordinates, false, true, 60, true);

		// Calculate heart rate using your algorithm
		double heartRate = movingAvg.calculateHeartRate(avg, static_cast<int>(preFilter), static_cast<int>(ppg),
								static_cast<int>(postFilter), fps, 1, smooth);

		if (heartRate != 0 && heartRate != -1) {
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

void processVideo(const VideoData &videoData, FaceDetectionAlgorithm faceDetect, PreFilteringAlgorithm preFilter,
		  PPGAlgorithm ppg, PostFilteringAlgorithm postFilter, bool smooth, std::ofstream &outFile)
{
	std::vector<double> predicted =
		calculateHeartRateForVideo(videoData, faceDetect, preFilter, ppg, postFilter, smooth);
	double ourAlgorithmRMSE = calculateRMSE(videoData.groundTruthHeartRate, predicted);
	double ourAlgorithmMAE = calculateMAE(videoData.groundTruthHeartRate, predicted);

	// Extract the subject name from the video path
	std::string subjectName = videoData.videoPath.substr(videoData.videoPath.find_last_of("/") + 1);
	subjectName = subjectName.substr(0, subjectName.find("."));

	// Convert numbers to strings with fixed precision
	std::string ourAlgorithmMAEStr = std::to_string(ourAlgorithmMAE);
	std::string otherAlgorithmMAEStr = (ppg == PPGAlgorithm::CHROM) ? std::to_string(videoData.chromMAE)
									: std::to_string(videoData.pcaMAE);
	std::string ourAlgorithmRMSEStr = std::to_string(ourAlgorithmRMSE);
	std::string otherAlgorithmRMSEStr = (ppg == PPGAlgorithm::CHROM) ? std::to_string(videoData.chromRMSE)
									 : std::to_string(videoData.pcaRMSE);

	// Lock the mutex before writing to the console and file
	std::lock_guard<std::mutex> lock(outputMutex);

	// Center-align the text and numbers
	std::cout << "| " << std::setw(12) << std::left << centerAlign(subjectName, 12) << " | " << std::setw(17)
		  << std::left << centerAlign(ourAlgorithmMAEStr, 17) << " | " << std::setw(19) << std::left
		  << centerAlign(otherAlgorithmMAEStr, 19) << " | " << std::setw(18) << std::left
		  << centerAlign(ourAlgorithmRMSEStr, 18) << " | " << std::setw(20) << std::left
		  << centerAlign(otherAlgorithmRMSEStr, 20) << " |\n";

	// Write the results to the CSV file
	outFile << subjectName << "," << ourAlgorithmMAEStr << "," << otherAlgorithmMAEStr << "," << ourAlgorithmRMSEStr
		<< "," << otherAlgorithmRMSEStr << "\n";
}

void evaluateHeartRate(const std::string &csvFilePath, FaceDetectionAlgorithm faceDetect,
		       PreFilteringAlgorithm preFilter, PPGAlgorithm ppg, PostFilteringAlgorithm postFilter,
		       bool smooth)
{
	std::vector<VideoData> videoDataList = readCSV(csvFilePath);

	// Construct the results filename based on the parameters
	std::string resultsFilename = "../../../../../eval/results/Iteration 3/" + toString(faceDetect) + "_" +
				      toString(preFilter) + "_" + toString(ppg) + "_" + toString(postFilter) + "_" +
				      (smooth ? "SMOOTHING_ON" : "SMOOTHING_OFF") + ".csv";

	// Print the table header
	std::cout
		<< "| Test Subject | Our Algorithm MAE | Other Algorithm MAE | Our Algorithm RMSE | Other Algorithm RMSE |\n";
	std::cout
		<< "|--------------|-------------------|---------------------|--------------------|----------------------|\n";

	// Open the CSV file for writing
	std::ofstream outFile(resultsFilename);
	outFile << "Test Subject,Our Algorithm MAE,Other Algorithm MAE,Our Algorithm RMSE,Other Algorithm RMSE\n";

	// Vector to hold futures for each thread
	std::vector<std::future<void>> futures;

	for (const auto &videoData : videoDataList) {
		// Create a future for each video evaluation
		futures.push_back(std::async(std::launch::async, processVideo, std::ref(videoData), faceDetect,
					     preFilter, ppg, postFilter, smooth, std::ref(outFile)));
	}

	// Wait for all threads to complete
	for (auto &future : futures) {
		future.get();
	}

	// Close the CSV file
	outFile.close();
}

int main()
{
	std::string csvFilePath = "../../../../../eval/ground_truth.csv";

	std::vector<FaceDetectionAlgorithm> faceDetectionAlgorithms = {FaceDetectionAlgorithm::DLIB,
								       FaceDetectionAlgorithm::HAAR_CASCADE};

	std::vector<PreFilteringAlgorithm> preFilteringAlgorithms = {PreFilteringAlgorithm::NONE,
								     PreFilteringAlgorithm::BUTTERWORTH_BANDPASS,
								     PreFilteringAlgorithm::DETREND,
								     PreFilteringAlgorithm::ZERO_MEAN};

	std::vector<PPGAlgorithm> ppgAlgorithms = {PPGAlgorithm::PCA, PPGAlgorithm::CHROM, PPGAlgorithm::GREEN};

	std::vector<PostFilteringAlgorithm> postFilteringAlgorithms = {PostFilteringAlgorithm::NONE,
								       PostFilteringAlgorithm::BUTTERWORTH_BANDPASS};

	std::vector<bool> smoothingOptions = {false, true};

	for (FaceDetectionAlgorithm faceDetectionAlgorithm : faceDetectionAlgorithms) {
		for (PPGAlgorithm ppgAlgorithm : ppgAlgorithms) {
			for (PreFilteringAlgorithm preFilteringAlgorithm : preFilteringAlgorithms) {
				for (PostFilteringAlgorithm postFilteringAlgorithm : postFilteringAlgorithms) {
					for (bool smoothing : smoothingOptions) {
						evaluateHeartRate(csvFilePath, faceDetectionAlgorithm,
								  preFilteringAlgorithm, ppgAlgorithm,
								  postFilteringAlgorithm, smoothing);
					}
				}
			}
		}
	}

	std::cout << "Press Enter to exit..." << std::endl;
	std::cin.get(); // Wait for user input before closing

	return 0;
}
