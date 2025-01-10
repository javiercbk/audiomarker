#pragma once
#include <vector>
#include <string>
#include <cstdint>

class AudioProcessor {
public:
    struct WAVHeader {
        char riff[4];
        uint32_t fileSize;
        char wave[4];
        char fmt[4];
        uint32_t fmtSize;
        uint16_t audioFormat;
        uint16_t numChannels;
        uint32_t sampleRate;
        uint32_t byteRate;
        uint16_t blockAlign;
        uint16_t bitsPerSample;
    };

    bool loadWAV(const std::string& filename);
    const std::vector<float>& getSamples() const { return samples; }
    std::vector<float> getDownsampledData(size_t maxPoints) const;
    size_t getSampleRate() const { return sampleRate; }
    size_t getNumSamples() const { return samples.size(); }

private:
    std::vector<float> samples;
    size_t sampleRate = 0;
};