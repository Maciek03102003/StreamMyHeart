import pandas as pd
import numpy as np
import csv

df = pd.read_hdf("./UBFC2_holistic.h5")
bpm_gt = df["bpmGT"]
filenames = df["videoFilename"]
MAE_mean = np.round(df['MAE'].mean()[0],2)
RMSE_mean = np.round(df['RMSE'].mean()[0],2)

print(MAE_mean)
print(RMSE_mean)

ground_truth = []
for i in range(0, bpm_gt.shape[0], 5):
    ground_truth.append(["video_files/" + filenames[i].split("UBFC2/")[1]] + list(bpm_gt[i]))
    
with open("ground_truth.csv", "w", newline="") as f:
    writer = csv.writer(f)
    writer.writerows(ground_truth)