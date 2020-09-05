#include "daubechies8.h"
#include <vector>
#include <algorithm>

/**
 * Class <code>WaveletBPMDetector</code> can be used to 
 * detect the tempo of a track in beats-per-minute.
 * The class implements the algorithm presented by 
 * Tzanetakis, Essl and Cookin the paper titled 
 * "Audio Analysis using the Discrete Wavelet Transform"
 *
 * Objects of the class can be created using the companion 
 * object's factory method. 
 * 
 * To detect the tempo the discrete wavelet transform is used. 
 * Track samples are divided into windows of frames. 
 * For each window data are divided into 4 frequency sub-bands 
 * through DWT. For each frequency sub-band an envelope is 
 * estracted from the detail coffecients by:
 * 1) Full wave rectification (take the absolute value), 
 * 2) Downsampling of the coefficients, 
 * 3) Normalization (via mean removal)
 * These 4 sub-band envelopes are then summed together. 
 * The resulting collection of data is then autocorrelated. 
 * Peaks in the correlated data correspond to peaks in the 
 * original signal. 
 * then peaks are identified on the filtered data.
 * Given the position of such a peak the approximated 
 * tempo of the window is computed and appended to a colletion.
 * Once all windows in the track are processed the beat-per-minute 
 * value is returned as the median of the windows values.
 * 
 * Audio track data is buffered so that there's no need 
 * to load the whole track in memory before applying 
 * the detection.
 * 
 * Class constructor is private, use the companion 
 * object instead.
 **/
class WaveletBPMDetector {
private:
    double sampleRate;

  /**
   * Identifies the location of data with the maximum absolute 
   * value (either positive or negative). If multiple data  
   * have the same absolute value the last positive is taken
   * @param data the input array from which to identify the maximum
   * @return the index of the maximum value in the array
   **/
  int detectPeak(std::vector<double> &data) {
    double max = DBL_MIN;

    for (double x : data)
      max = std::max(max, std::abs(x));

    for (int i = 0; i < data.size(); ++i)
      if (data[i] == max)
        return i;

    for (int i = 0; i < data.size(); ++i) {
      if (data[i] == -max)
        return i;

    return -1;
  }

  /**
   * Given <code>windowFrames</code> samples computes a BPM 
   * value for the window and pushes it in <code>instantBpm</code>
   * @param An array of <code>windowFrames</code> samples representing the window
   **/
  void computeWindowBpm(std::vector<double> &data) {
    int levels = 4;
    double maxDecimation = std::pow2(levels - 1);
    int minIndex = (int)(60.0 / 220.0 * sampleRate / maxDecimation);
    int maxIndex = (int)(60.0 / 40.0 * sampleRate / maxDecimation);
    int dCMinLength = int(decomp[0].second.size() / maxDecimation) + 1;
    std::vector<double> dCSum(dCMinLength);

    Daubechies8 wavelet;
    std::vector<decomposition> decomp = wavelet.decompose(data, levels);

    // 4 Level DWT
    for (int loop = 0; loop < levels; ++loop) {
      std::vector<double> &dC = decomp[loop].second;

      // Extract envelope from detail coefficients
      //  1) Undersample
      //  2) Absolute value
      //  3) Subtract mean
      int pace = std::pow2(levels - loop - 1);
      dC = dC.undersample(pace).abs
      dC = dC - dC.mean

      // Recombine detail coeffients
      if (dCSum == null) {
        dCSum = dC.slice(0, dCMinLength)
      } else {
        dCSum = dC.slice(0, std::min(dCMinLength, dC.length)) |+| dCSum
      }
    }
  
    // Add the last approximated data
    std::vector<double> &aC = decomp[levels - 1].first;
    aC = aC.abs
    aC = aC - aC.mean
    dCSum = aC.slice(0, min(dCMinLength, dC.length)) |+| dCSum

    // Autocorrelation
    var correlated : Array[Double] = dCSum.correlate
    val correlatedTmp = correlated.slice(minIndex, maxIndex)

    // Detect peak in correlated data
    val location = detectPeak(correlatedTmp)

    // Compute window BPM given the peak
    val realLocation = minIndex + location
    val windowBpm : Double = 60.toDouble / realLocation * (audioFile.sampleRate.toDouble/maxDecimation)
    instantBpm += windowBpm;
  }
};
