#include "post_filters.h"
#include "filter_util.h"

using namespace std;
using namespace Eigen;

vector<double_t> bpFilter(vector<double_t> signal, int fps)
{
	int order = 6;       // Standard filter order
	double minHz = 0.65; // Corresponds to 40 BPM
	double maxHz = 3.0;  // Corresponds to 180 BPM

	size_t n = signal.size();
	if (n < 2)
		return signal; // Not enough data to filter

	// Convert to Eigen vector
	VectorXd sig(n);
	for (size_t i = 0; i < n; ++i) {
		sig(i) = signal[i];
	}

	// Compute Butterworth filter coefficients
	VectorXd b, a;
	butterworthBandpass(order, minHz, maxHz, fps, a, b);

	// Apply forward-backward filtering for zero-phase distortion
	VectorXd y = applyIIRFilter(b, a, sig);
	y = applyIIRFilter(b, a, y.reverse()).reverse();

	// Convert back to standard C++ vector
	vector<double_t> filteredHeartRates(n);
	for (size_t i = 0; i < n; ++i) {
		filteredHeartRates[i] = y(i);
	}

	return filteredHeartRates;
}

vector<double_t> applyPostFilter(vector<double_t> signal, int filter, int fps)
{
	if (filter == 0) {
		return signal;
	} else if (filter == 1) { // Band pass
		return bpFilter(signal, fps);
	}

	return {};
}