#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <opencv2/opencv.hpp>
#include "../algorithm/HeartRateAlgorithm.h" // Include your heart rate algorithm header

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

// Function to extract BGRA data from a video frame
input_BGRA_data extractBGRAData(const cv::Mat &frame) {
    input_BGRA_data bgraData;
    bgraData.width = frame.cols;
    bgraData.height = frame.rows;
    bgraData.linesize = static_cast<uint32_t>(frame.step);
    bgraData.data = new uint8_t[frame.total() * frame.elemSize()];
    std::memcpy(bgraData.data, frame.data, frame.total() * frame.elemSize());
    return bgraData;
}

double calculateMAE(const std::vector<double>& actual, const std::vector<double>& predicted) {
    double mae = 0.0;
    for (size_t i = 0; i < static_cast<int>(actual.size()); ++i) {
        mae += std::fabs(actual[i] - predicted[i]); // Absolute error
    }
    return mae / actual.size();
}

double calculateRMSE(const std::vector<double>& actual, const std::vector<double>& predicted) {
    double rmse = 0.0;
    for (size_t i = 0; i < static_cast<int>(actual.size()); ++i) {
        rmse += std::pow(actual[i] - predicted[i], 2); // Squared error
    }
    return std::sqrt(rmse / actual.size());
}

std::vector<double> calculateHeartRateForVideo(const VideoData &videoData) { // {MAE, RMSE}
    cv::VideoCapture cap(videoData.videoPath);
    if (!cap.isOpened()) {
        std::cerr << "Error: Could not open video file " << videoData.videoPath << std::endl;
        return {};
    }

    MovingAvg avg;
    cv::Mat frame;
    std::vector<struct vec4> faceCoordinates;

    int frameCount = 0;
    int fps = static_cast<int>(cap.get(cv::CAP_PROP_FPS));

    std::vector<double> predicted;

    while (cap.read(frame)) {
        // Convert the frame to BGRA format
        cv::Mat bgraFrame;
        cv::cvtColor(frame, bgraFrame, cv::COLOR_BGR2BGRA);

        // Extract BGRA data
        input_BGRA_data bgraData = extractBGRAData(bgraFrame);


        // Calculate heart rate using your algorithm
        double heartRate = avg.calculateHeartRate(&bgraData, faceCoordinates, 0, 0, 0, fps, 1, false);
        if (heartRate != 0) {
            std::cout << heartRate << std::endl;
        }

        // Clean up
        delete[] bgraData.data;

        // if (frameCount != 0 && frameCount % fps == 0) {
        //     predicted.push_back(heartRate);
        //     std::cout << "Push" << heartRate << std::endl;
        // }

        frameCount++;
    }

    cap.release();
    // Return the final calculated heart rate (this is a placeholder)
    return predicted;
}

void evaluateHeartRate(const std::string &csvFilePath) {
    std::vector<VideoData> videoDataList = readCSV(csvFilePath);

    for (const auto &videoData : videoDataList) {
        std::vector<double> predicted = calculateHeartRateForVideo(videoData);
        std::cout << "Predicted values: " << predicted.size() << std::endl;
        //std::cout << "RMSE: " << calculateRMSE(videoData.groundTruthHeartRate, predicted) << std::endl;
        //std::cout << "MAE: " << calculateMAE(videoData.groundTruthHeartRate, predicted) << std::endl;
    }
}

int main() {
    std::string csvFilePath = "../../src/eval/ground_truth.csv";

    evaluateHeartRate(csvFilePath);

    while(true) {

    }
    
    return 0;
}