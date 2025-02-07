#include <obs-module.h>
#include "plugin-support.h"
#include "HeartRateAlgorithm.h"
#include <fstream>
#include <string>
#include <cstdlib>
#include <cmath>

using namespace std;
using namespace Eigen;
using FrameRGB = vector<vector<vector<uint8_t>>>;
using Windows = vector<vector<vector<double_t>>>;
using Window = vector<vector<double_t>>;

// Calculating the average/mean RGB values of a frame
vector<double_t> MovingAvg::averageRGB(FrameRGB rgb, vector<vector<bool>> skinKey)
{
	double sumR = 0.0, sumG = 0.0, sumB = 0.0;
	double count = 0;

	// Iterate through the frame pixels using the key
	for (int i = 0; i < static_cast<int>(rgb.size()); ++i) {
		for (int j = 0; j < static_cast<int>(rgb[0].size()); ++j) {
			if (skinKey.empty()) {
				sumR += rgb[i][j][0];
				sumG += rgb[i][j][1];
				sumB += rgb[i][j][2];
				count++;
			} else {
				if (skinKey[i][j]) {
					sumR += rgb[i][j][0];
					sumG += rgb[i][j][1];
					sumB += rgb[i][j][2];
					count++;
				}
			}
		}
	}
	if (count > 0) {
		return {sumR / count, sumG / count, sumB / count};
	}

	return {0.0, 0.0, 0.0};
}

vector<double_t> green(Window windowsRGB)
{
	vector<double_t> framesG;
	for (int i = 0; i < static_cast<int>(windowsRGB.size()); i++) {
		framesG.push_back(windowsRGB[i][1]);
	}

	return framesG;
}

vector<double_t> pca(Window windowsRGB)
{
	int numSamples = static_cast<int>(windowsRGB.size());
	int numFeatures = 3;

	MatrixXd mat(numSamples, numFeatures);
	for (int i = 0; i < numSamples; ++i) {
		for (int j = 0; j < numFeatures; ++j) {
			mat(i, j) = windowsRGB[i][j];
		}
	}

	VectorXd mean = mat.colwise().mean();
	MatrixXd centered = mat.rowwise() - mean.transpose();

	MatrixXd cov = (centered.transpose() * centered) / double(numSamples - 1);
	SelfAdjointEigenSolver<MatrixXd> solver(cov);
	MatrixXd eigenvects = solver.eigenvectors();

	VectorXd pc = eigenvects.col(numFeatures - 1);
	VectorXd transformed = centered * pc;

	vector<double_t> result(transformed.data(), transformed.data() + transformed.size());
	return result;
}

void MovingAvg::updateWindows(vector<double_t> frame_avg)
{
	if (windows.empty()) {
		windows.push_back({frame_avg});
		return;
	}

	Window last = windows.back();

	if (static_cast<int>(last.size()) == windowSize) {
		Window newWindow = Window(last.end() - windowStride, last.end());
		newWindow.push_back(frame_avg);
		if (static_cast<int>(windows.size()) == maxNumWindows) {
			windows.erase(windows.begin());
		}
		windows.push_back(newWindow);
	} else {
		windows.back().push_back(frame_avg);
	}
}

FrameRGB extractRGB(struct input_BGRA_data *BGRA_data)
{
	uint8_t *data = BGRA_data->data;
	uint32_t width = BGRA_data->width;
	uint32_t height = BGRA_data->height;
	uint32_t linesize = BGRA_data->linesize;

	FrameRGB frameRGB(height, vector<vector<uint8_t>>(width));

	for (uint32_t y = 0; y < height; ++y) {
		for (uint32_t x = 0; x < width; ++x) {
			uint8_t B = data[y * linesize + x * 4];
			uint8_t G = data[y * linesize + x * 4 + 1];
			uint8_t R = data[y * linesize + x * 4 + 2];

			frameRGB[y][x] = {R, G, B};
		}
	}

	return frameRGB;
}

double MovingAvg::welch(vector<double_t> bvps)
{

	using Eigen::ArrayXd;

	int num_frames = static_cast<int>(bvps.size());
	int nfft = 2048;

	// Define segment size and overlap
	int segment_size = 256;
	int overlap = 200;

	double frequency_resolution = (fps * 60.0) / num_frames;
	int nyquist_limit = segment_size / 2;

	

	// Convert signal to Eigen array
	ArrayXd signal = Eigen::Map<const ArrayXd>(bvps.data(), bvps.size());

	// Divide signal into overlapping segments
	int num_segments = 0;
	ArrayXd psd = ArrayXd::Zero(nfft / 2 + 1);

	if (num_frames < segment_size) {

		// Hann window
		ArrayXd hann_window(num_frames);
		for (int i = 0; i < num_frames; ++i) {
			hann_window[i] = 0.5 * (1 - std::cos(2 * M_PI * i / (num_frames - 1)));
		}

		ArrayXd windowed_signal = signal * hann_window;
		Eigen::ArrayXcd fft_result(num_frames);
		for (int k = 0; k < num_frames; ++k) {
			std::complex<double> sum(0.0, 0.0);
			for (int n = 0; n < num_frames; ++n) {
				double angle = -2.0 * M_PI * k * n / num_frames;
				sum += windowed_signal[n] * std::exp(std::complex<double>(0, angle));
			}
			fft_result[k] = sum;
		}

		// Compute power spectrum
		ArrayXd power_spectrum =
			ArrayXd::Zero(num_frames / 2 + 1); // Only half the spectrum (0 to Nyquist frequency)
		for (int k = 0; k <= num_frames / 2; ++k) {
			psd[k] += std::norm(fft_result[k]) / num_frames;
		}

		++num_segments;
	} else {
		
		ArrayXd hann_window(segment_size);
		for (int i = 0; i < segment_size; ++i) {
			hann_window[i] = 0.5 * (1 - std::cos(2 * M_PI * i / (segment_size - 1)));
		}

		for (int start = 0; start + segment_size <= num_frames; start += (segment_size - overlap)) {
			// Extract segment and apply window
			ArrayXd segment = signal.segment(start, segment_size) * hann_window;

			Eigen::ArrayXcd fft_result(segment_size);
			for (int k = 0; k < segment_size; ++k) {
				std::complex<double> sum(0.0, 0.0);
				for (int n = 0; n < segment_size; ++n) {
					double angle = -2.0 * M_PI * k * n / segment_size;
					sum += segment[n] * std::exp(std::complex<double>(0, angle));
				}
				fft_result[k] = sum;
			}

			// Compute power spectrum
			ArrayXd power_spectrum =
				ArrayXd::Zero(nyquist_limit + 1); // Only half the spectrum (0 to Nyquist frequency)
			for (int k = 0; k <= nyquist_limit; ++k) {
				psd[k] += std::norm(fft_result[k]) / segment_size;
			}

			++num_segments;
		}
	}

	// Average PSD for this estimator
	if (num_segments > 0) {
		psd /= num_segments;
	}

	// Adjust Nyquist limit for human heart rates
	int nyquist_limit_bpm = min(min(num_frames / 2, nyquist_limit), static_cast<int>(200 / frequency_resolution));

	double lower_limit = 50;
	for (int k = 0; k <= nyquist_limit_bpm; ++k) {
		if (k * frequency_resolution < lower_limit) {
			psd[k] = 0;
		}
	}

	// Find dominant frequency in BPM
	int max_index;
	psd.head(nyquist_limit_bpm + 1).maxCoeff(&max_index);
	double dominant_frequency = max_index * frequency_resolution;

	return dominant_frequency;
}

Window concatWindows(Windows windows)
{
	Window concatenatedWindow;

	for (const auto &window : windows) {
		for (const auto &sig : window) {
			concatenatedWindow.push_back(sig);
		}
	}

	return concatenatedWindow;
}

double MovingAvg::calculateHeartRate(vector<double_t> avg, int preFilter, int ppg, int postFilter)
{ // Assume frame in YUV format: struct obs_source_frame *source
	UNUSED_PARAMETER(preFilter);
	UNUSED_PARAMETER(postFilter);

	updateWindows(avg);

	vector<double_t> ppgSignal;

	if (!windows.empty() && static_cast<int>(windows.back().size()) == windowSize) {
		Window currentWindow = concatWindows(windows);
		switch (ppg) {
		case 0:
			ppgSignal = green(currentWindow);
			break;
		case 1:
			ppgSignal = pca(currentWindow);
			break;
		default:
			break;
		}

		return welch(ppgSignal);
	} else {
		return 0.0;
	}
}
