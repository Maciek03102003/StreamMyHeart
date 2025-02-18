#include "post_filters.h"
#include "filter_util.h"

using namespace std;
using namespace Eigen;

vector<double_t> applyPostFilter(vector<double_t> signal, int filter, int fps)
{
	if (filter == 0) {
		return signal;
	} else if (filter == 1) { // Band pass
		vector<vector<double_t>> newSignal(signal.size(), vector<double_t>(1, 0.0));
		for (size_t i = 0; i < signal.size(); ++i) {
			newSignal[i][0] = signal[i];
		}
		vector<vector<double_t>> filterere = bpFilter(newSignal, fps);
		vector<double_t> res(signal.size());
		for (size_t i = 0; i < signal.size(); ++i) {
			res[i] = filterere[i][0];
		}
		return res;
	}

	return {};
}