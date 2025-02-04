#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <opencv2/opencv.hpp>
//#include "HeartRateAlgorithm.h" // Include your heart rate algorithm header

struct VideoData {
    std::string videoPath;
    std::vector<double> groundTruthHeartRate;
};

std::vector<VideoData> readCSV(const std::string &csvFilePath) {
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
        while (std::getline(ss, token, ',')) {
            groundTruthHeartRates.push_back(std::stod(token));
        }

        videoDataList.push_back({videoPath, groundTruthHeartRates});
    }
    return videoDataList;
}

// // Function to extract BGRA data from a video frame
// input_BGRA_data extractBGRAData(const cv::Mat &frame) {
//     input_BGRA_data bgraData;
//     bgraData.width = frame.cols;
//     bgraData.height = frame.rows;
//     bgraData.linesize = frame.step;
//     bgraData.data = new uint8_t[frame.total() * frame.elemSize()];
//     std::memcpy(bgraData.data, frame.data, frame.total() * frame.elemSize());
//     return bgraData;
// }

// double calculateHeartRateForVideo(const std::string &videoPath) {
//     cv::VideoCapture cap(videoPath);
//     if (!cap.isOpened()) {
//         std::cerr << "Error: Could not open video file " << videoPath << std::endl;
//         return -1;
//     }

//     MovingAvg avg;
//     cv::Mat frame;
//     std::vector<struct vec4> faceCoordinates;

//     while (cap.read(frame)) {
//         // Convert the frame to BGRA format
//         cv::Mat bgraFrame;
//         cv::cvtColor(frame, bgraFrame, cv::COLOR_BGR2BGRA);

//         // Extract BGRA data
//         input_BGRA_data bgraData = extractBGRAData(bgraFrame);

//         // Calculate heart rate using your algorithm
//         double heartRate = avg.calculateHeartRate(&bgraData, faceCoordinates);

//         // Clean up
//         delete[] bgraData.data;

//         std::cout << "Calculated Heart Rate: " << heartRate << std::endl;
//     }

//     cap.release();
//     // Return the final calculated heart rate (this is a placeholder)
//     return avg.getFinalHeartRate();
// }

// void evaluateHeartRate(const std::string &csvFilePath) {
//     std::vector<VideoData> videoDataList = readCSV(csvFilePath);

//     for (const auto &videoData : videoDataList) {
//         double calculatedHeartRate = calculateHeartRateForVideo(videoData.videoPath);
//         std::cout << "Video: " << videoData.videoPath << std::endl;
//         std::cout << "Ground Truth Heart Rate: " << videoData.groundTruthHeartRate << std::endl;
//         std::cout << "Calculated Heart Rate: " << calculatedHeartRate << std::endl;
//         std::cout << "Difference: " << std::abs(calculatedHeartRate - videoData.groundTruthHeartRate) << std::endl;
//         std::cout << "----------------------------------------" << std::endl;
//     }
// }

int main() {
    std::string csvFilePath = "./ground_truth.csv";
    std::vector<VideoData> videoDataList = readCSV(csvFilePath);
    
    for (VideoData d : videoDataList) {
        std::cout << d.videoPath << std::endl;
        for (double bpm : d.groundTruthHeartRate) {
            std::cout << bpm << std::endl;
        }
    }
    
    return 0;
}