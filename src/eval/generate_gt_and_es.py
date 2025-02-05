import pandas as pd
import numpy as np
import csv

df = pd.read_hdf("./UBFC2_holistic.h5")
bpm_gt = df["bpmGT"]
filenames = df["videoFilename"]
print(df["RMSE"])
print(df["method"])

rmse = []
for i in range(4, df["RMSE"].shape[0], 5):
    rmse.append(df["RMSE"][i])

RMSE_mean = np.round(np.array(rmse).mean(),2)

print(RMSE_mean)

ground_truth = []
for i in range(4, bpm_gt.shape[0], 5):
    ground_truth.append(["video_files/" + filenames[i].split("UBFC2/")[1]] + list(bpm_gt[i]))
    
with open("ground_truth.csv", "w", newline="") as f:
    writer = csv.writer(f)
    writer.writerows(ground_truth)