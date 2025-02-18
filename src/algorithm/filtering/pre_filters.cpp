#include "pre_filters.h"
#include "filter_util.h"

#include <obs-module.h>
#include "plugin-support.h"

using namespace std;
using namespace Eigen;

vector<double_t> detrendSignal(const vector<double_t> &signal)
{
	int n = static_cast<int>(signal.size());
	if (n < 2)
		return signal; // Not enough points to perform detrending

	VectorXd x(n), y(n);

	// Construct x (time indices) and y (signal values)
	for (int i = 0; i < n; ++i) {
		x(i) = i;         // Time index
		y(i) = signal[i]; // Original signal
	}

	// Construct the design matrix for Least Squares
	MatrixXd X(n, 2);
	X.col(0).setOnes(); // First column is all ones (for intercept)
	X.col(1) = x;       // Second column is time indices

	// Compute the least squares solution for the linear fit (Ax = b)
	VectorXd coeffs = (X.transpose() * X).ldlt().solve(X.transpose() * y);

	// Compute the linear trend (y = a + bt)
	VectorXd trend = X * coeffs;

	// Subtract the trend from the signal
	vector<double_t> detrendedSignal(n);
	for (int i = 0; i < n; ++i) {
		detrendedSignal[i] = signal[i] - trend(i);
	}

	return detrendedSignal;
}

vector<double_t> zeroMeanFilter(const vector<double_t> &signal)
{
	if (signal.empty())
		return signal; // Handle empty input

	// Compute the mean
	double mean = accumulate(signal.begin(), signal.end(), 0.0) / signal.size();

	// Subtract the mean from each sample
	vector<double_t> zeroMeanSignal(signal.size());
	for (size_t i = 0; i < signal.size(); ++i) {
		zeroMeanSignal[i] = signal[i] - mean;
	}

	return zeroMeanSignal;
}

vector<vector<double_t>> applyPreFilter(vector<vector<double_t>> signal, int filter, int fps)
{
	if (filter == 0) {
		return signal;
	} else if (filter == 1) { // Band pass
		return bpFilter(signal, fps);
	} else if (filter == 2) {
		// Apply Detrending on each RGB channel
		for (size_t i = 0; i < signal.size(); ++i) {
			signal[i] = detrendSignal(signal[i]);
		}
		return signal;
	} else if (filter == 3) {
		// Apply Zero-Mean Filtering on each channel
		for (size_t i = 0; i < signal.size(); ++i) {
			if (!signal[i].empty()) {
				signal[i] = zeroMeanFilter(signal[i]);
			}
		}
		return signal;
	}

	return {};
}
