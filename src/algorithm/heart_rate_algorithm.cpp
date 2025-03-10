#include "heart_rate_algorithm.h"
#include "plugin-support.h"
#include "filtering/pre_filters.h"
#include "filtering/post_filters.h"
#include "heart_rate_source.h"

#include <obs-module.h>
#include <fstream>
#include <string>
#include <cstdlib>
#include <cmath>
#include <util/platform.h>

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
	MatrixXd eigenVects = solver.eigenvectors();

	VectorXd pc = eigenVects.col(numFeatures - 1);
	VectorXd transformed = centered * pc;

	vector<double_t> result(transformed.data(), transformed.data() + transformed.size());
	return result;
}

vector<double_t> chrom(Window windowsRGB)
{
	int numSamples = static_cast<int>(windowsRGB.size());
	int numFeatures = 3;

	MatrixXd X(numSamples, numFeatures);
	for (int i = 0; i < numSamples; ++i) {
		for (int j = 0; j < numFeatures; ++j) {
			X(i, j) = windowsRGB[i][j];
		}
	}

	VectorXd Xc = 3 * X.col(0) - 2 * X.col(1);
	VectorXd Yc = 1.5 * X.col(0) + X.col(1) - 1.5 * X.col(2);

	double sX = sqrt((Xc.array() - Xc.mean()).square().sum() / (Xc.size() - 1));
	double sY = sqrt((Yc.array() - Yc.mean()).square().sum() / (Yc.size() - 1));

	VectorXd bvp = Xc - ((sX / sY) * Yc);

	return vector<double_t>(bvp.data(), bvp.data() + bvp.size());
}

void MovingAvg::updateWindows(vector<double_t> frameAvg)
{
	if (windows.empty()) {
		windows.push_back({frameAvg});
		return;
	}

	Window last = windows.back();

	if (static_cast<int>(last.size()) == windowSize) {
		Window newWindow = Window(last.end() - windowStride, last.end());
		newWindow.push_back(frameAvg);
		if (static_cast<int>(windows.size()) == maxNumWindows) {
			windows.erase(windows.begin());
		}
		windows.push_back(newWindow);
	} else {
		windows.back().push_back(frameAvg);
	}
}

FrameRGB extractRGB(std::shared_ptr<struct input_BGRA_data> bgraData)
{
	uint8_t *data = bgraData->data;
	uint32_t width = bgraData->width;
	uint32_t height = bgraData->height;
	uint32_t linesize = bgraData->linesize;

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

	int numFrames = static_cast<int>(bvps.size());
	int nfft = 2048;

	// Define segment size and overlap
	int segmentSize = 256;
	int overlap = 200;

	double frequencyResolution = (fps * 60.0) / numFrames;
	int nyquistLimit = fps / 2;

	// Convert signal to Eigen array
	ArrayXd signal = Eigen::Map<const ArrayXd>(bvps.data(), bvps.size());

	// Divide signal into overlapping segments
	int numSegments = 0;
	ArrayXd psd = ArrayXd::Zero(nfft / 2 + 1);

	if (numFrames < segmentSize) {
		// Hann window
		ArrayXd hannWindow(numFrames);
		for (int i = 0; i < numFrames; ++i) {
			hannWindow[i] = 0.5 * (1 - std::cos(2 * M_PI * i / (numFrames - 1)));
		}

		ArrayXd windowedSignal = signal * hannWindow;
		Eigen::ArrayXcd fft_result(numFrames);
		for (int k = 0; k < numFrames; ++k) {
			std::complex<double> sum(0.0, 0.0);
			for (int n = 0; n < numFrames; ++n) {
				double angle = -2.0 * M_PI * k * n / numFrames;
				sum += windowedSignal[n] * std::exp(std::complex<double>(0, angle));
			}
			fft_result[k] = sum;
		}

		// Compute power spectrum
		ArrayXd powerSpectrum =
			ArrayXd::Zero(numFrames / 2 + 1); // Only half the spectrum (0 to Nyquist frequency)
		for (int k = 0; k <= numFrames / 2; ++k) {
			psd[k] += std::norm(fft_result[k]) / numFrames;
		}

		++numSegments;
	} else {
		ArrayXd hannWindow(segmentSize);
		for (int i = 0; i < segmentSize; ++i) {
			hannWindow[i] = 0.5 * (1 - std::cos(2 * M_PI * i / (segmentSize - 1)));
		}

		for (int start = 0; start + segmentSize <= numFrames; start += (segmentSize - overlap)) {
			// Extract segment and apply window
			ArrayXd segment = signal.segment(start, segmentSize) * hannWindow;

			Eigen::ArrayXcd fftResult(segmentSize);
			for (int k = 0; k < segmentSize; ++k) {
				std::complex<double> sum(0.0, 0.0);
				for (int n = 0; n < segmentSize; ++n) {
					double angle = -2.0 * M_PI * k * n / segmentSize;
					sum += segment[n] * std::exp(std::complex<double>(0, angle));
				}
				fftResult[k] = sum;
			}

			// Compute power spectrum
			ArrayXd powerSpectrum =
				ArrayXd::Zero(nyquistLimit + 1); // Only half the spectrum (0 to Nyquist frequency)
			for (int k = 0; k <= nyquistLimit; ++k) {
				psd[k] += std::norm(fftResult[k]) / segmentSize;
			}

			++numSegments;
		}
	}

	// Average PSD for this estimator
	if (numSegments > 0) {
		psd /= numSegments;
	}

	// Adjust Nyquist limit for human heart rates
	int nyquistLimitBPM = min(min(numFrames / 2, nyquistLimit), static_cast<int>(200 / frequencyResolution));

	double lowerLimit = 55;
	double threshold = 70;
	double sf = 0.6;
	for (int k = 0; k <= nyquistLimitBPM; ++k) {
		if (k * frequencyResolution < lowerLimit) {
			psd[k] = 0;
		}
		if (k * frequencyResolution < threshold) {
			psd[k] *= sf;
		}
	}

	int maxIndex;
	psd.head(nyquistLimitBPM + 1).maxCoeff(&maxIndex);
	double dominantFrequency = maxIndex * frequencyResolution;

	return dominantFrequency;
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

double MovingAvg::smoothHeartRate(double hr)
{
	double meanHr = accumulate(heartRates.begin(), heartRates.end(), 0.0) / heartRates.size();

	double offset = 20.0;

	heartRates.erase(heartRates.begin());

	if (hr != 0) {
		heartRates.push_back(hr);
	}

	if (hr > meanHr + offset) {
		return meanHr + offset;
	} else if (hr < meanHr - offset) {
		return meanHr - offset;
	}

	return hr;
}

double MovingAvg::calculateHeartRate(vector<double_t> avg, int preFilter, int ppg, int postFilter, bool smooth, int Fps,
				     int sampleRate)
{
	uint64_t start_heart_rate;
	if (enableTiming) {
		start_heart_rate = os_gettime_ns();
	}

	fps = Fps;
	windowSize = sampleRate * fps;
	uiUpdateInterval = fps / 2;

	updateWindows(avg);

	vector<double_t> ppgSignal;

	if (!windows.empty() && static_cast<int>(windows.back().size()) == windowSize &&
	    static_cast<int>(windows.size()) >= calibrationTime) {
		Window currentWindow = concatWindows(windows);

		uint64_t start_pre_filter, end_pre_filter;
		if (enableTiming) {
			start_pre_filter = os_gettime_ns();
		}
		Window filteredWindow = applyPreFilter(currentWindow, preFilter, fps);
		if (enableTiming) {
			end_pre_filter = os_gettime_ns();
			obs_log(LOG_INFO, "Pre-filtering took: %lu ns", end_pre_filter - start_pre_filter);
		}

		uint64_t start_ppg, end_ppg;
		if (enableTiming) {
			start_ppg = os_gettime_ns();
		}
		switch (ppg) {
		case 0:
			ppgSignal = green(filteredWindow);
			break;
		case 1:
			ppgSignal = pca(filteredWindow);
			break;
		case 2:
			ppgSignal = chrom(filteredWindow);
			break;
		default:
			break;
		}
		if (enableTiming) {
			end_ppg = os_gettime_ns();
			obs_log(LOG_INFO, "PPG calculation took: %lu ns", end_ppg - start_ppg);
		}

		uint64_t start_post_filter, end_post_filter;
		if (enableTiming) {
			start_post_filter = os_gettime_ns();
		}
		vector<double_t> filtered_ppg = applyPostFilter(ppgSignal, postFilter, fps);
		if (enableTiming) {
			end_post_filter = os_gettime_ns();
			obs_log(LOG_INFO, "Post-filtering took: %lu ns", end_post_filter - start_post_filter);
		}

		uint64_t start_welch, end_welch;
		if (enableTiming) {
			start_welch = os_gettime_ns();
		}
		double heartRate = welch(filtered_ppg);
		if (enableTiming) {
			end_welch = os_gettime_ns();
			obs_log(LOG_INFO, "Welch took: %lu ns", end_welch - start_welch);
		}

		if (smooth) {
			uint64_t start_smooth, end_smooth;
			if (enableTiming) {
				start_smooth = os_gettime_ns();
			}
			if (static_cast<int>(heartRates.size()) < numHeartRates) {
				heartRates.push_back(heartRate);
			} else {
				heartRate = smoothHeartRate(heartRate);
			}
			if (enableTiming) {
				end_smooth = os_gettime_ns();
				obs_log(LOG_INFO, "Smoothing took: %lu ns", end_smooth - start_smooth);
			}
		}

		if (uiHeartRate == -1.0) {
			uiHeartRate = heartRate;
			prevHr = heartRate;
		} else {
			if (heartRates.size() >= 2 && heartRates[heartRates.size() - 2] - heartRate <= 30) {
				uiUpdateAmount = min((heartRate - uiHeartRate) / NUM_UPDATES, 5.0);
				prevHr = heartRate;
			}
		}

		if (enableTiming) {
			uint64_t end_heart_rate = os_gettime_ns();
			obs_log(LOG_INFO, "Heart rate calculation took: %lu ns", end_heart_rate - start_heart_rate);
		}

		return uiHeartRate;

	} else {
		if (static_cast<int>(windows.size()) < calibrationTime) {
			return -1.0;
		}

		framesSincePPG += 1;

		if (!heartRates.empty() && framesSincePPG % uiUpdateInterval == 0 && uiHeartRate != prevHr) {
			uiHeartRate += uiUpdateAmount;
		}

		if (enableTiming) {
			uint64_t end_heart_rate = os_gettime_ns();
			obs_log(LOG_INFO, "Heart rate update took: %lu ns", end_heart_rate - start_heart_rate);
		}

		return uiHeartRate;
	}
}
