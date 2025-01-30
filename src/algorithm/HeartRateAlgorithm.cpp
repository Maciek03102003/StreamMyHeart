#include "FaceDetection.h"
#include <obs-module.h>
#include "plugin-support.h"
#include "HeartRateAlgorithm.h"
#include <fstream>
#include <string>
#include "patches-face-detection.h"

using namespace std;
using namespace Eigen;

// Calculating the moving average across a vector of frames
vector<vector<double>> MovingAvg::moving_average(const vector<vector<double>> &rgb, int n)
{
	int width = static_cast<int>(rgb[0].size());
	int height = static_cast<int>(rgb.size());
	// Convert the input vector to an Eigen matrix
	MatrixXd input(height, width);
	for (int i = 0; i < height; ++i) {
		for (int j = 0; j < width; ++j) {
			input(i, j) = static_cast<double>(rgb[i][j]);
		}
	}

	// Create an output matrix
	MatrixXd output = MatrixXd::Zero(height, width);

	// Apply moving average using a sliding window
	for (int i = 0; i < static_cast<int>(height); ++i) {
		for (int j = 0; j < static_cast<int>(width); ++j) {
			// Calculate the row range for the moving average
			int start_row = max(0, i - n / 2);
			int end_row = min(static_cast<int>(height), i + n / 2 + 1);

			// Calculate the moving average
			output(i, j) = input.block(start_row, j, end_row - start_row, 1).mean();
		}
	}

	// Convert the output matrix back to a vector of vectors with uint8_t
	vector<vector<double>> result(height, vector<double>(width));
	for (int i = 0; i < height; ++i) {
		for (int j = 0; j < width; ++j) {
			result[i][j] = output(i, j);
		}
	}

	return result;
}

// Calculating the average/mean RGB values of a frame
vector<double_t> MovingAvg::average_keyed(std::vector<std::vector<std::tuple<double, double, double>>> rgb,
					  std::vector<std::vector<bool>> skinkey)
{
	double sumR = 0.0, sumG = 0.0, sumB = 0.0;
	double count = 0;

	// Iterate through the frame pixels using the key
	for (int i = 0; i < static_cast<int>(rgb.size()); ++i) {
		for (int j = 0; j < static_cast<int>(rgb[0].size()); ++j) {
			if (skinkey[i][j]) {
				sumR += get<0>(rgb[i][j]);
				sumG += get<1>(rgb[i][j]);
				sumB += get<2>(rgb[i][j]);
				count++;
			}
		}
	}

	// Calculate averages
	if (count > 0) {
		return {sumR / count, sumG / count, sumB / count};
	} else {
		return {0.0, 0.0, 0.0}; // Handle the case where no pixels matched the key
	}
}

// Function to magnify color
std::vector<vector<double>> MovingAvg::magnify_colour_ma(const vector<vector<double>> &rgb, double delta, int n_bg_ma,
							 int n_smooth_ma)
{
	int height = static_cast<int>(rgb.size());
	int width = static_cast<int>(rgb[0].size());

	// Return if one of the parameters are 0
	if (width == 0 || height == 0)
		return {};

	// Step 1: Remove slow-moving background component
	vector<vector<double>> rgb_bg_ma = moving_average(rgb, n_bg_ma);

	vector<vector<double>> rgb_detrended(height, vector<double>(width));
	for (int i = 0; i < height; ++i) {
		for (int j = 0; j < width; ++j) {
			// Subtract the values
			rgb_detrended[i][j] = rgb[i][j] - rgb_bg_ma[i][j];
		}
	}

	// Step 2: Smooth the resulting PPG
	vector<vector<double>> ppg_smoothed = moving_average(rgb_detrended, n_smooth_ma);

	// Step 3: Remove NaNs (replace with 0)
	// Not necessray as we are working with uint8_t types which cannot be non-numbers

	// Step 4: Normalize to have a max delta of 'delta'
	int8_t max_val = 0;
	for (int i = 0; i < height; i++) {
		for (int j = 0; j < width; j++) {
			// TODO: We are not sure if we are working with unsigend numers correctly, might need to look after conversion
			max_val = std::max(max_val, static_cast<int8_t>(std::abs(ppg_smoothed[i][j])));
		}
	}

	// Now normalize the ppg_smoothed matrix using max delta
	if (max_val > 0) { // Avoid division by zero
		for (int i = 0; i < height; i++) {
			for (int j = 0; j < width; j++) {
				// TODO: We are not sure if we are working with unsigned numbers correctly, might need to look after conversion
				ppg_smoothed[i][j] = static_cast<double>(delta * ppg_smoothed[i][j] / max_val);
			}
		}
	}

	return ppg_smoothed;
}

double MovingAvg::Welch_cpu_heart_rate(const std::vector<std::vector<double>> &bvps, int num_data_points)
{

	using Eigen::ArrayXd;

	double frequency_resolution = (fps * 60.0) / num_data_points;

	// Convert input to a single signal by averaging RGB channels
	ArrayXd signal(num_data_points);
	for (int i = 0; i < num_data_points; ++i) {
		signal[i] = bvps[i][0];
	}

	// Apply Hann window
	ArrayXd hann_window(num_data_points);
	for (int i = 0; i < num_data_points; ++i) {
		hann_window[i] = 0.5 * (1 - std::cos(2 * M_PI * i / (num_data_points - 1)));
	}
	ArrayXd windowed_signal = signal * hann_window;

	// Compute DFT manually
	Eigen::ArrayXcd fft_result(num_data_points);
	for (int k = 0; k < num_data_points; ++k) {
		std::complex<double> sum(0.0, 0.0);
		for (int n = 0; n < num_data_points; ++n) {
			double angle = -2.0 * M_PI * k * n / num_data_points;
			sum += windowed_signal[n] * std::exp(std::complex<double>(0, angle));
		}
		fft_result[k] = sum;
	}

	// Apply frequency-domain filtering
	double lower_cutoff_hz = 30.0 / 60.0;  // ~40 BPM
	double upper_cutoff_hz = 200.0 / 60.0; // ~200 BPM
	for (int k = 0; k < num_data_points; ++k) {
		double freq = k * (fps / num_data_points);
		if (freq < lower_cutoff_hz || freq > upper_cutoff_hz) {
			fft_result[k] = 0; // Remove frequencies outside the range
		}
	}

	// Compute power spectrum
	int nyquist_limit = num_data_points / 2;
	ArrayXd power_spectrum = ArrayXd::Zero(nyquist_limit + 1); // Only half the spectrum (0 to Nyquist frequency)
	for (int k = 0; k <= nyquist_limit; ++k) {
		power_spectrum[k] = std::norm(fft_result[k]) / num_data_points;
	}

	// Adjust Nyquist limit for human heart rates
	int nyquist_limit_bpm = std::min(nyquist_limit, static_cast<int>(200 / frequency_resolution));

	// Log power spectrum to OBS console in BPM
	std::ostringstream log_stream;
	for (int k = 0; k <= nyquist_limit_bpm; ++k) {
		double bpm = k * frequency_resolution;
		log_stream << bpm << " BPM: " << power_spectrum[k];
		if (k < nyquist_limit_bpm) {
			log_stream << ", ";
		}
	}
	obs_log(LOG_INFO, "%s", log_stream.str().c_str());

	// Find dominant frequency in BPM
	int max_index;
	power_spectrum.head(nyquist_limit_bpm + 1).maxCoeff(&max_index);
	double dominant_frequency = max_index * frequency_resolution;

	return dominant_frequency;
}

double MovingAvg::calculateHeartRate(struct input_BGRA_data *BGRA_data, std::vector<struct vec4> &face_coordinates)
{
	// Assume frame in YUV format: struct obs_source_frame *source
	uint8_t *data = BGRA_data->data;
	uint32_t width = BGRA_data->width;
	uint32_t height = BGRA_data->height;
	uint32_t linesize = BGRA_data->linesize;

	// Create a 2D vector to store RGB tuples
	std::vector<std::vector<std::tuple<double, double, double>>> rgb(
		height, std::vector<std::tuple<double, double, double>>(width));

	for (uint32_t y = 0; y < height; ++y) {
		for (uint32_t x = 0; x < width; ++x) {
			uint8_t B = data[y * linesize + x * 4 + 0];
			uint8_t G = data[y * linesize + x * 4 + 1];
			uint8_t R = data[y * linesize + x * 4 + 2];

			// Store as a tuple in the vector
			rgb[y][x] = std::make_tuple(R, G, B);
		}
	}

	// uncomment this when face detect fixed and add to next line as param
	std::vector<std::vector<bool>> skinKey = faceMask(BGRA_data, face_coordinates);
	// detectFacesAndCreateMask(BGRA_data, face_coordinates);
	vector<double_t> averageRGBValues = average_keyed(rgb, skinKey);

	frame_data.push_back(averageRGBValues);

	if (static_cast<int>(frame_data.size()) >= maxBufSize) { // Calculate heart rate when frame list "full"
		std::vector<std::vector<double>> ppg_rgb_ma = magnify_colour_ma(frame_data);

		std::vector<vector<double>> ppg_w_ma;
		for (auto &f : ppg_rgb_ma) {
			std::vector<double> avg;
			avg.push_back((f[0] + f[1] + f[2]) / 3);
			ppg_w_ma.push_back(avg);
		}

		prev_hr = Welch_cpu_heart_rate(ppg_w_ma, static_cast<int>(frame_data.size()));

		frame_data = {}; // Naive approach - can change but just for simplicity
	}

	return prev_hr;
}
