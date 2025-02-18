#include "post_filters.h"
#include "filter_util.h"
#include "plugin-support.h"
#include <obs-module.h>

using namespace std;
using namespace Eigen;

// vector<double_t> bpFilter(vector<double_t> signal, int fps)
// {
// 	int order = 6;       // Standard filter order
// 	double minHz = 0.65; // Corresponds to 40 BPM
// 	double maxHz = 3.0;  // Corresponds to 180 BPM

// 	size_t n = signal.size();
// 	if (n < 2)
// 		return signal; // Not enough data to filter

// 	// Convert to Eigen vector
// 	std::string sigggg = "Signal before post filter: [";
// 	VectorXd sig(n);
// 	for (size_t i = 0; i < n; ++i) {
// 		sigggg += std::to_string(signal[i]) + ", ";
// 		sig(i) = signal[i];
// 	}
// 	sigggg += "]";
// 	obs_log(LOG_INFO, sigggg.c_str());

// 	// Compute Butterworth filter coefficients
// 	VectorXd b, a;
// 	butterworthBandpass(order, minHz, maxHz, fps, a, b);

// 	// Apply forward-backward filtering for zero-phase distortion
// 	VectorXd y = applyIIRFilter(b, a, sig);
// 	VectorXd reverse_y = y.reverse();
// 	y = applyIIRFilter(b, a, reverse_y);
// 	y = y.reverse();

// 	std::string hrs = "Filtered Heart Rates: [";
// 	// Convert back to standard C++ vector
// 	vector<double_t> filteredHeartRates(n);
// 	for (size_t i = 0; i < n; ++i) {
// 		hrs += std::to_string(y(i)) + ", ";
// 		filteredHeartRates[i] = y(i);
// 	}
// 	hrs += "]";
// 	obs_log(LOG_INFO, hrs.c_str());

// 	return filteredHeartRates;
// }

vector<vector<double_t>> bpFilter_ha(vector<vector<double_t>> signal, int fps)
{

	int order = 6;
	double minHz = 0.65;
	double maxHz = 3.0;

	size_t rows = signal.size();
	size_t cols = signal[0].size();
	MatrixXd sig((int)rows, (int)cols);

	for (int i = 0; i < static_cast<int>(rows); ++i) {
		for (int j = 0; j < static_cast<int>(cols); ++j) {
			sig(i, j) = signal[i][j];
		}
	}

	sig.transposeInPlace();

	VectorXd b, a;
	butterworthBandpass(order, minHz, maxHz, fps, a, b);

	MatrixXd y = forwardBackFilter(b, a, sig);
	y.transposeInPlace();

	vector<vector<double_t>> result(rows, vector<double>(cols));
	for (int i = 0; i < static_cast<int>(rows); ++i) {
		for (int j = 0; j < static_cast<int>(cols); ++j) {
			result[i][j] = y(i, j);
		}
	}

	std::string hrs = "Pre filter heart rates: [";
	for (int i = 0; i < static_cast<int>(rows); ++i) {
		hrs += std::to_string(result[i][1]) + ", ";
	}
	hrs += "]";
	obs_log(LOG_INFO, hrs.c_str());

	return result;
}

vector<double_t> applyPostFilter(vector<double_t> signal, int filter, int fps)
{
	if (filter == 0) {
		return signal;
	} else if (filter == 1) { // Band pass
		// return bpFilter(signal, fps);
		vector<vector<double_t>> newSignal(signal.size(), vector<double_t>(1, 0.0));
		for (size_t i=0; i < signal.size(); ++i) {
			newSignal[i][0] = signal[i];
		}
		vector<vector<double_t>> filterere = bpFilter_ha(newSignal, fps);
		vector<double_t> res(signal.size());
		for (size_t i = 0; i < signal.size(); ++i) {
			res[i] = filterere[i][0];
		}
		return res;
	}

	return {};
}