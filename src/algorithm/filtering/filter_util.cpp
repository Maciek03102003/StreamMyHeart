#include "filter_util.h"

#include "plugin-support.h"
#include <obs-module.h>

using namespace std;
using namespace Eigen;

void butterworthBandpass(int order, double minHz, double maxHz, double fps, VectorXd &a, VectorXd &b)
{
	double nyquist = fps / 2.0;
	double low = minHz / nyquist;
	double high = maxHz / nyquist;

	// Convert to pre-warped analog frequencies
	double omega1 = tan(M_PI * low);
	double omega2 = tan(M_PI * high);

	// Get the center frequency and bandwidth
	double omega0 = sqrt(omega1 * omega2);
	double bandwidth = omega2 - omega1;

	// Initialize coefficient vectors
	a = VectorXd(order + 1);
	b = VectorXd(order + 1);

	// Compute coefficients (Butterworth poles in s-domain)
	vector<double> A(order + 1, 0);
	vector<double> B(order + 1, 0);

	for (int i = 0; i <= order; ++i) {
		double theta = M_PI * (2 * i + 1) / (2.0 * order);
		double sigma = -omega0 * sin(theta);
		double omega = omega0 * cos(theta);
		double denominator = sigma * sigma + omega * omega + bandwidth * sigma;

		B[i] = bandwidth / denominator;
		A[i] = 1.0 - (2 * sigma / denominator);
	}

	// Convert to Eigen vectors
	for (int i = 0; i <= order; ++i) {
		b(i) = B[i];
		a(i) = A[i];
	}
}

VectorXd applyIIRFilter(const VectorXd &b, const VectorXd &a, const VectorXd &x)
{
	size_t n = x.size();
	VectorXd y(n);

	// Ensure a(0) is 1, normalize otherwise
	if (a(0) == 0) {
		std::cerr << "Error: a(0) cannot be zero in IIR filter!" << std::endl;
		return y.setConstant(n, NAN); // Return NaN-filled vector
	}

	VectorXd a_norm = a / a(0);
	VectorXd b_norm = b / a(0);

	for (int i = 0; i < static_cast<int>(n); ++i) {
		y(i) = 0.0;

		// Apply FIR part (b coefficients)
		for (int j = 0; j < static_cast<int>(b.size()); ++j) {
			if (i - j >= 0) {
				y(i) += b_norm(j) * x(i - j);
			}
		}

		// Apply IIR part (a coefficients)
		for (int j = 1; j < static_cast<int>(a.size()); ++j) { // a[0] is already normalized
			if (i - j >= 0) {
				y(i) -= a_norm(j) * y(i - j);
			}
		}
	}

	return y;
}

MatrixXd forwardBackFilter(const VectorXd &b, const VectorXd &a, const MatrixXd &x)
{
	int rows = static_cast<int>(x.rows()), cols = static_cast<int>(x.cols());
	MatrixXd y(rows, cols);

	// Apply filtering in forward direction
	for (int j = 0; j < cols; ++j) {
		y.col(j) = applyIIRFilter(b, a, x.col(j));
	}

	// Apply filtering in backward direction
	for (int j = 0; j < cols; ++j) {
		VectorXd reversed = y.col(j).reverse();
		reversed = applyIIRFilter(b, a, reversed);
		y.col(j) = reversed.reverse();
	}

	return y;
}

vector<vector<double_t>> bpFilter(vector<vector<double_t>> signal, int fps)
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

	std::string hrs = "BP filter heart rates: [";
	for (int i = 0; i < static_cast<int>(rows); ++i) {
		hrs += std::to_string(result[i][1]) + ", ";
	}
	hrs += "]";
	obs_log(LOG_INFO, hrs.c_str());

	return result;
}
