import pandas as pd
import matplotlib.pyplot as plt

result_path = 'eval/results/csv/'

df_track = pd.read_csv(result_path + "Dlib_BP_PCA_BP_Track.csv")
df_no_track = pd.read_csv(result_path + "Dlib_Detrend_Green_BP_None.csv")

# work out the mean and standard deviation
face_tracking = df_track[(df_track['Duration (ns)'] <= 20000000) & (df_track['Function'] == 'Face detection')]
print(face_tracking)
face_tracking_mean = face_tracking['Duration (ns)'].mean()
face_tracking_std = face_tracking['Duration (ns)'].std()
print(f"Face tracking mean: {face_tracking_mean}, std: {face_tracking_std}")

face_detection = df_no_track[df_no_track['Function'] == 'Face detection']
face_detection = face_detection.iloc[1:]  # Remove the first line
face_detection_mean = face_detection['Duration (ns)'].mean()
face_detection_std = face_detection['Duration (ns)'].std()
print(f"Face detection mean: {face_detection_mean}, std: {face_detection_std}")

heart_rate_calculation = df_track[(df_track['Function'] == 'Heart rate calculation')]
heart_rate_calculation_mean = heart_rate_calculation['Duration (ns)'].mean()
heart_rate_calculation_std = heart_rate_calculation['Duration (ns)'].std()
print(f"Heart rate calculation mean: {heart_rate_calculation_mean}, std: {heart_rate_calculation_std}")

# Convert ns to seconds for plotting
face_detection_mean /= 1e9
face_tracking_mean /= 1e9
heart_rate_calculation_mean /= 1e9
face_detection_std /= 1e9
face_tracking_std /= 1e9
heart_rate_calculation_std /= 1e9

stds = [face_detection_std, face_tracking_std, heart_rate_calculation_std]
print("Standard deviation: " + str(stds))

# Plot the data
fig, ax = plt.subplots()
labels = ['Face Detect', 'Face Track', 'Heart Rate Calc']
means = [face_detection_mean, face_tracking_mean, heart_rate_calculation_mean]
print("Mean: " + str(means))

ax.bar(labels, means, color='lightblue')

ax.set_yscale('log')  # Log scale for better visualization
ax.set_ylabel("Time to run (s)")
ax.set_title("Time to complete pipeline steps")

# Display the plot
plt.show()