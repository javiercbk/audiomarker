#include "audio_processor.h"
#include <fstream>
#include <algorithm>
#include <cstring>

bool AudioProcessor::loadWAV(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) return false;

    WAVHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(WAVHeader));

    // Verify RIFF header
    if (strncmp(header.riff, "RIFF", 4) != 0 || 
        strncmp(header.wave, "WAVE", 4) != 0) {
        return false;
    }

    // Check if the file is stereo
    if (header.numChannels != 1) {
        // Stereo file detected, return false or handle error
        return false;
    }

    // Skip to data chunk
    char chunkID[4];
    uint32_t chunkSize;
    while (file.read(chunkID, 4)) {
        file.read(reinterpret_cast<char*>(&chunkSize), 4);
        if (strncmp(chunkID, "data", 4) == 0) {
            break;
        }
        file.seekg(chunkSize, std::ios::cur);
    }

    // Read samples
    samples.clear();
    sampleRate = header.sampleRate;
    size_t numSamples = chunkSize / (header.bitsPerSample / 8);
    samples.reserve(numSamples);

    if (header.bitsPerSample == 16) {
        int16_t sample;
        while (file.read(reinterpret_cast<char*>(&sample), sizeof(sample))) {
            samples.push_back(sample / 32768.0f);
        }
    } else if (header.bitsPerSample == 32) {
        float sample;
        while (file.read(reinterpret_cast<char*>(&sample), sizeof(sample))) {
            samples.push_back(sample);
        }
    }

    return !samples.empty();
}