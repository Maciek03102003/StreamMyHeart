import os
import csv
import numpy as np

def calculate_mean_rmse(file_path):
    rmses = []
    with open(file_path, 'r') as csvfile:
        reader = csv.reader(csvfile)
        next(reader)  # Skip header
        for row in reader:
            rmses.append(float(row[3]))  # Our RMSE column is 4th
    return np.mean(rmses)

def rank_csv_files(directory):
    rmse_means = {}
    for filename in os.listdir(directory):
        if filename.endswith('.csv'):
            file_path = os.path.join(directory, filename)
            mean_rmse = calculate_mean_rmse(file_path)
            rmse_means[filename] = mean_rmse

    ranked_files = sorted(rmse_means.items(), key=lambda x: x[1])
    for rank, (filename, mean_rmse) in enumerate(ranked_files, start=1):
        print(f"{rank}. {filename}: {mean_rmse:.4f}")

if __name__ == "__main__":
    rank_csv_files('new_results')