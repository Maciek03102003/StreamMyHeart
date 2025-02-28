import re
import pandas as pd

def extract_obs_log_timings(input_file, output_csv):
    # Define regex pattern to match log lines
    log_pattern = re.compile(r'^(\d{2}:\d{2}:\d{2}\.\d{3}): \[stream-my-heart] (.+?) took: (\d+) ns')
    
    extracted_data = []
    
    # Read the file and extract matching log statements
    with open(input_file, 'r') as file:
        for line in file:
            match = log_pattern.search(line)
            if match:
                timestamp = match.group(1)
                function_name = match.group(2)
                duration = int(match.group(3))
                extracted_data.append([timestamp, function_name, duration])
    
    # Convert extracted data to a DataFrame
    df = pd.DataFrame(extracted_data, columns=["Timestamp", "Function", "Duration (ns)"])
    
    # Save to CSV
    df.to_csv(output_csv, index=False)
    
    print(f"Data successfully extracted and saved to {output_csv}")

# print curren tpath
import os
print(os.getcwd())

log_path = "eval/results/logs/"
result_path = "eval/results/csv/"

evaluate_targets = [
    # with smoothing:
    "Dlib_BP_PCA_BP_Track",
    "Dlib_Detrend_Green_BP_None",
    "OpenCV_ZeroMean_Chrom_BP",
    "Dlib_None_PCA_None_Track",
    "Dlib_BP_Chrom_BP_Track",
]

for target in evaluate_targets:
    extract_obs_log_timings(log_path + target + ".txt", result_path + target + ".csv")