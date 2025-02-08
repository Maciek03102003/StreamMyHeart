import pandas as pd
import numpy as np
import csv

df = pd.read_hdf("./UBFC2_holistic.h5")
bpm_gt = df["bpmGT"]
filenames = df["videoFilename"]
rmse = df["RMSE"]
mae = df["MAE"]

ground_truth = []
for i in range(4, bpm_gt.shape[0], 5):
    folder_name = filenames[i].split("/")[5]  # Extract the folder name
    new_file_name = folder_name + ".avi"  # Create the new file name
    ground_truth.append(["../../../../../eval/video_files/" + new_file_name] + list("[") + list(bpm_gt[i]) + list("]") + list(rmse[i]) + list(mae[i]))
    
with open("ground_truth.csv", "w", newline="") as f:
    writer = csv.writer(f)
    writer.writerows(ground_truth)