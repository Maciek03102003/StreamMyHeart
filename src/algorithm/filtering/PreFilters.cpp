#include "PreFilters.h"

using namespace std;
using namespace Eigen;

void butterworth_bandpass(int order, double minHz, double maxHz, double fps, VectorXd &a, VectorXd &b) {
    int n = order / 2;
    double nyquist = fps / 2.0;
    double low = minHz / nyquist;
    double high = maxHz / nyquist;

    b = VectorXd::Zero(n + 1);
    a = VectorXd::Zero(n + 1);

    for (int i = 0; i <= n; ++i) {
        b(i) = (low + high) / 2.0;
        a(i) = 1.0;
    }
}

MatrixXd forward_back_filter(const VectorXd &b, const VectorXd &a, const MatrixXd &x) {
    int rows = static_cast<int>(x.rows()), cols = static_cast<int>(x.cols());
    MatrixXd y = x;
    
    // Apply filtering in forward direction
    cout << rows << cols << endl;
    for (int j = 0; j < cols; ++j) {
        VectorXd column = x.col(j);
        VectorXd filtered = column;
        for (int i = 1; i < b.size(); ++i) {
            if (j - i >= 0) {
                //cout << j << "," << i << endl;
                filtered(i) += b(i) * column(j - i) - a(i) * filtered(j - i);
            }
        }
        y.col(j) = filtered;
    }
    
    // Apply filtering in backward direction
    for (int j = 0; j < cols; ++j) {
        VectorXd column = y.col(j);
        VectorXd filtered = column;
        for (int i = 1; i < b.size(); ++i) {
            if (j - i >= 0) {
                filtered(i) += b(i) * column(j - i) - a(i) * filtered(j - i);
            }
        }
        y.col(j) = filtered;
    }
    
    return y;
}

vector<vector<double_t>> BP_filter(vector<vector<double_t>> signal, int fps) {

    int order = 6;
    double minHz = 0.65;
    double maxHz = 4.0;

    cout << "Band Pass filter" << endl;

    size_t rows = signal.size();
    size_t cols = signal[0].size();
    MatrixXd sig((int)rows, (int)cols);

    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            sig(i, j) = signal[i][j];
        }
    }

    sig.transposeInPlace();

    cout << "Create and transpose" << endl;

    VectorXd b, a;
    butterworth_bandpass(order, minHz, maxHz, fps, a, b);

    cout << "Create bandpass" << endl;

    MatrixXd y = forward_back_filter(a, b, sig);
    y.transposeInPlace();

    vector<vector<double_t>> result(rows, vector<double>(cols));
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            result[i][j] = y(i, j);
        }
    }
    
    return result;
}

vector<vector<double_t>> applyPreFilter(vector<vector<double_t>> signal, int filter, int fps)
{
	if (filter == 0) {
		return signal;
	} else if (filter == 1) { // Band pass
		return BP_filter(signal, fps);
	}

	return {};
}

