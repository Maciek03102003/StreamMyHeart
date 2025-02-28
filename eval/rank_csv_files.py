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
    rmseMeans = {}
    haarCascadeRmse = []
    dlibRmse = []
    preFilteringNoneRmse = []
    preFilteringBandpassRmse = []
    preFilteringDetrendRmse = []
    preFilteringZeroMeanRmse = []
    greenRmse = []
    pcaRmse = []
    chromRmse = []
    postFilteringNoneRMSE = []
    postFilteringBandpassRMSE = []
    smoothingOnRmse = []
    smoothingOffRmse = []
    for filename in os.listdir(directory):
        if filename.endswith('.csv'):
            filePath = os.path.join(directory, filename)
            parts = filename.split('_')
            meanRmse = calculate_mean_rmse(filePath)
            rmseMeans[filename] = meanRmse
            
            if "HAAR_CASCADE" in filename:
                haarCascadeRmse.append(meanRmse)
            elif "DLIB" in filename:
                dlibRmse.append(meanRmse)
                
            preFiltering = 0
            if "HAAR_CASCADE" in filename:
                preFiltering = parts[2]
            else:
                preFiltering = parts[1]

            print(preFiltering)
                
            if preFiltering == "NONE":
                preFilteringNoneRmse.append(meanRmse)
            elif preFiltering == "BUTTERWORTH":
                preFilteringBandpassRmse.append(meanRmse)
            elif preFiltering == "DETREND":
                preFilteringDetrendRmse.append(meanRmse)
            elif preFiltering == "ZERO":
                preFilteringZeroMeanRmse.append(meanRmse)
                
            if "GREEN" in filename:
                greenRmse.append(meanRmse)
            elif "PCA" in filename:
                pcaRmse.append(meanRmse)
            elif "CHROM" in filename:
                chromRmse.append(meanRmse)
                
            postFiltering = parts[-3]
            if postFiltering == "BANDPASS":
                postFilteringBandpassRMSE.append(meanRmse)
            elif postFiltering == "NONE":
                postFilteringNoneRMSE.append(meanRmse)
            
            if "SMOOTHING_ON" in filename:
                smoothingOnRmse.append(meanRmse)
            elif "SMOOTHING_OFF" in filename:
                smoothingOffRmse.append(meanRmse)

    rankedFiles = sorted(rmseMeans.items(), key=lambda x: x[1])
    for rank, (filename, meanRmse) in enumerate(rankedFiles, start=1):
        print(f"{rank}. {filename}: {meanRmse:.4f}")
    
    if haarCascadeRmse and dlibRmse:
        meanRmseHaar = np.mean(haarCascadeRmse)
        meanRmseDlib = np.mean(dlibRmse)
        print(f"\nMean RMSE with Haar cascade: {meanRmseHaar:.4f}")
        print(f"Mean RMSE with DLIB: {meanRmseDlib:.4f}")
        
    if preFilteringNoneRmse and preFilteringBandpassRmse and preFilteringDetrendRmse and preFilteringZeroMeanRmse:
        meanRmsePreFilteringNone = np.mean(preFilteringNoneRmse)
        meanRmsePreFilteringBandpass = np.mean(preFilteringBandpassRmse)
        meanRmsePreFilteringDetrend = np.mean(preFilteringDetrendRmse)
        meanRmsePreFilteringZeroMean = np.mean(preFilteringZeroMeanRmse)
        print(f"\nMean RMSE with pre-filtering none: {meanRmsePreFilteringNone:.4f}")
        print(f"Mean RMSE with pre-filtering bandpass: {meanRmsePreFilteringBandpass:.4f}")
        print(f"Mean RMSE with pre-filtering detrend: {meanRmsePreFilteringDetrend:.4f}")
        print(f"Mean RMSE with pre-filtering zero mean: {meanRmsePreFilteringZeroMean:.4f}")
    
    if greenRmse and pcaRmse:
        meanRmseGreen = np.mean(greenRmse)
        meanRmsePca = np.mean(pcaRmse)
        meanRmseChrom = np.mean(chromRmse)
        print(f"\nMean RMSE with Green: {meanRmseGreen:.4f}")
        print(f"Mean RMSE with PCA: {meanRmsePca:.4f}")
        print(f"Mean RMSE with Chrom: {meanRmseChrom:.4f}")
        
    if postFilteringBandpassRMSE and postFilteringNoneRMSE:
        meanRmsePostFilterNone = np.mean(postFilteringNoneRMSE)
        meanRmsePostFilterBandpass = np.mean(postFilteringBandpassRMSE)
        print(f"\nMean RMSE with post-filtering none: {meanRmsePostFilterNone:.4f}")
        print(f"Mean RMSE with post-filtering bandpass: {meanRmsePostFilterBandpass:.4f}")
        
    if smoothingOnRmse and smoothingOffRmse:
        meanRmseSmoothingOff = np.mean(smoothingOffRmse)
        meanRmseSmoothingOn = np.mean(smoothingOnRmse)
        print(f"\nMean RMSE with smoothing off: {meanRmseSmoothingOff:.4f}")
        print(f"Mean RMSE with smoothing on: {meanRmseSmoothingOn:.4f}")

    # Write the results to a CSV file
    with open('results/Iteration 4/mean_rmse_results.csv', 'w', newline='') as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow(['Filename', 'Mean RMSE'])
        for filename, meanRmse in rankedFiles:
            writer.writerow([filename[:-4], meanRmse])
        
if __name__ == "__main__":
    rank_csv_files('results/Iteration 3/')