// main.cpp
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"
#include "audio_processor.h"
#include <SDL.h>
#include <GL/gl3w.h>
#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>

template<typename T> static inline T ImMin(T lhs, T rhs)                        { return lhs < rhs ? lhs : rhs; }
template<typename T> static inline T ImMax(T lhs, T rhs)                        { return lhs >= rhs ? lhs : rhs; }

class AudioVisualizer {
public:
    AudioVisualizer() : windowWidth(1280), windowHeight(720) {
        initAudioPlayback();
    }

    struct Marker {
        size_t sample;
        int intensity; // -1: section, 0: Low, 1: Med, 2: High, 3: Very High
        size_t end;
    };
    
    bool init(const char* wavFile) {
        currentWavFile = wavFile;
        // Setup SDL window
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

        std::string window_name = std::string("Audio Visualizer - ") + wavFile;
        window = SDL_CreateWindow(window_name.data(),
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            windowWidth, windowHeight,
            SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
        
        glContext = SDL_GL_CreateContext(window);
        SDL_GL_MakeCurrent(window, glContext);
        SDL_GL_SetSwapInterval(1); // Enable vsync

        if (gl3wInit() != 0) {
            return false;
        }

        // Setup Dear ImGui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImPlot::CreateContext();
        
        ImGui_ImplSDL2_InitForOpenGL(window, glContext);
        ImGui_ImplOpenGL3_Init("#version 130");

        // Load WAV file
        if (!audioProcessor.loadWAV(wavFile)) {
            return false;
        }

        // Construct CSV filename (same base name as WAV file)
        std::string csvFilename = std::string(wavFile);
        size_t dotPos = csvFilename.find_last_of('.');
        if (dotPos != std::string::npos) {
            csvFilename = csvFilename.substr(0, dotPos) + ".csv";
        }

        // Load markers from CSV
        std::ifstream csvFile(csvFilename);
        if (csvFile.is_open()) {
            std::string line;
            while (std::getline(csvFile, line)) {
                std::istringstream ss(line);
                std::string sampleStr, intensityStr, endStr;
                
                // Parse sample number and intensity
                if (std::getline(ss, sampleStr, ',') && 
                    std::getline(ss, intensityStr, ',')) {
                    try {
                        size_t sample = std::stoull(sampleStr);
                        int intensity = std::stoi(intensityStr);
                        if (intensity == -1 && std::getline(ss, endStr)) {
                            size_t end = std::stoull(endStr);
                            marks.push_back({sample, intensity, end});
                        } else {
                            marks.push_back({sample, intensity});
                        }
                    } catch (const std::exception& e) {
                        printf("Error parsing CSV line: %s\n", line.c_str());
                    }
                }
            }
            
            // Sort markers after loading
            std::sort(marks.begin(), marks.end(), 
                [](const Marker& a, const Marker& b) { 
                    return a.sample < b.sample; 
                });
            
            printf("Loaded %zu markers from CSV\n", marks.size());
        } else {
            printf("No markers CSV found at %s\n", csvFilename.c_str());
        }

        return true;
    }

    void saveMarkersToCsv() {
        // Construct CSV filename (same base name as WAV file)
        std::string csvFilename = std::string(currentWavFile.c_str());
        size_t dotPos = csvFilename.find_last_of('.');
        if (dotPos != std::string::npos) {
            csvFilename = csvFilename.substr(0, dotPos) + ".csv";
        }

        // Open file for writing
        std::ofstream csvFile(csvFilename);
        if (csvFile.is_open()) {
            // Write each marker
            for (const auto& marker : marks) {
                if (marker.end == 0) {
                    csvFile << marker.sample << "," << marker.intensity << "\n";
                } else {
                    csvFile << marker.sample << "," << marker.intensity << "," << marker.end << "\n";
                }
            }
            
            printf("Saved %zu markers to CSV: %s\n", marks.size(), csvFilename.c_str());
        } else {
            printf("Failed to open CSV for writing: %s\n", csvFilename.c_str());
        }
    }

    void cleanup() {
        // Close audio device during cleanup
        if (audioDevice != 0) {
            SDL_CloseAudioDevice(audioDevice);
        }

        // Existing cleanup code...
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImPlot::DestroyContext();
        ImGui::DestroyContext();
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
    }

    void render() {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
        ImGui::Begin("Audio Visualizer", nullptr, 
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);


        ImGui::BeginChild("plot", ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y * 0.4), true);
        auto samples = audioProcessor.getSamples();
        static size_t base_sample = 0;
        if (ImPlot::BeginPlot("##Waveform", ImVec2(-1, -1))) {
            // no zoom
            // ImPlot::SetupAxisZoomConstraints(ImAxis_X1, 1.0, INFINITY);
            ImPlot::SetupAxisZoomConstraints(ImAxis_Y1, 1.0, 1.0);
            ImPlot::SetupAxisLimitsConstraints(ImAxis_X1, 0, samples.size());
            ImPlot::SetupAxes("Sample Number", "Amplitude");
            ImPlot::SetupAxisLimits(ImAxis_X1, 0, 10000, ImGuiCond_Once);
            ImPlot::SetupAxisLimits(ImAxis_Y1, -1, 1, ImGuiCond_Once);

            auto limits = ImPlot::GetPlotLimits();


            size_t minX = static_cast<size_t>(limits.X.Min);
            size_t maxX = static_cast<size_t>(limits.X.Max);
            auto total_samples = maxX - minX;
            base_sample = minX;
            if (base_sample > samples.size()) {
                base_sample = samples.size() -1;
                total_samples = 0;
            } else if (samples.size() < maxX) {
                total_samples = samples.size() - minX;
            }
            
            auto downsampledData = samples.data() + base_sample;


            auto getter = [](int idx, void* data) -> ImPlotPoint {
                float* samples = (float*)(data);
                // pointer arythmetics FTW
                float y = *(samples + idx);
                return ImPlotPoint(idx+base_sample, y);
            };

            ImPlot::PlotLineG("Waveform", getter, downsampledData, total_samples, 0.0);

            auto mousePosX = (size_t)std::floor(ImPlot::GetPlotMousePos().x);
            double mousePosDouble = double(mousePosX);
            // Handle plot interactions
            if (ImPlot::IsPlotHovered()) {
                ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
		        ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1);
                ImPlot::PlotInfLines("audio marks", &mousePosDouble, 1);
                ImPlot::PopStyleColor();
                ImPlot::PopStyleVar();
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
                    if (mousePosX > 0 && mousePosX < audioProcessor.getNumSamples()) {
                        insertMarkSorted(mousePosX, currentIntensity);
                    }
                }
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsKeyDown(ImGuiKey_LeftAlt)) {
                    if (mousePosX > 0 && mousePosX < audioProcessor.getNumSamples()) {
                        insertSectionSorted(mousePosX, -1);
                    }
                }
                if (ImGui::IsKeyDown(ImGuiKey_Escape)) {
                    // cancel selection
                    section_mark = 0;
                }
            }

            ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
		    ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1);
            std::vector<double> temp_marks;
            std::vector<ImVec4> mark_colors;
            temp_marks.reserve(marks.size());
            mark_colors.reserve(marks.size());
            int i = 0;
            for (auto& mark : marks) {
                if (mark.end == 0) {
                    temp_marks.push_back(static_cast<double>(mark.sample));
                    mark_colors.push_back(intensityColors[mark.intensity]);
                } else {
                    //FIXME: should crash because it will be writing memory to the stack when changed
                    double start = (double) mark.sample;
                    double end = (double) mark.end;
                    double prev_start = start;
                    double prev_end = end;
                    bool clicked = false, hovered = false, held = false;
                    ImVec4 color(1.0f, 0.0f, 0.0f, 0.5f); // Red, 50%
                    ImPlot::DragRect(i, &start, &limits.Y.Min, &end, &limits.Y.Max, color, ImPlotDragToolFlags_None, &clicked, &hovered, &held);
                    if (held) {
                        if (prev_start != start || prev_end != end) {
                            double min, max = 0.0;
                            if (start < end) {
                                min = std::max(start, 0.0);
                                max = std::min(end, (double) audioProcessor.getNumSamples());
                            } else {
                                min = std::max(end, 0.0);
                                max = std::min(start, (double) audioProcessor.getNumSamples());
                            }
                            mark.sample = min;
                            mark.end = max;
                            needsSectionsUpdate = i;
                        }
                    } else if (needsSectionsUpdate == i) {
                        saveMarkersToCsv();
                        needsSectionsUpdate = -1;
                    }
                }
                i++;
            }
            for (size_t i = 0; i < temp_marks.size(); ++i) {
                ImPlot::PushStyleColor(ImPlotCol_Line, mark_colors[i]);
                ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1);
                ImPlot::PlotInfLines("audio marks", &temp_marks[i], 1);
                ImPlot::PopStyleColor();
                ImPlot::PopStyleVar();
            }
            if (section_mark != 0) {
                ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(1.0f, 0.0f, 0.0f, 0.5f));
                ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1);
                double d_section_mark = (double) section_mark;
                ImPlot::PlotInfLines("mark", &d_section_mark, 1);
                ImPlot::PopStyleColor();
                ImPlot::PopStyleVar();
            }
            ImPlot::PopStyleColor();
		    ImPlot::PopStyleVar();
            ImPlot::EndPlot();
        }

        ImGui::EndChild();

        ImGui::BeginChild("ControlPanel", ImVec2(ImGui::GetContentRegionAvail().x, 200), true);

        // Split the child window into two columns
        ImGui::Columns(2, "ControlColumns", true);
        
        // Left column - List of rows
        ImGui::SetColumnWidth(0, ImGui::GetWindowWidth() * 0.7f);
        {
            ImGui::Text("Markers");
            ImGui::Separator();
            
            // Scrollable list of markers
            ImGui::BeginChild("MarkerList", ImVec2(0, 150), true);
            for (size_t i = 0; i < marks.size(); ++i) {
                ImGui::PushID(i);
                
                ImGui::Text("Sample %zu", marks[i].sample);
                ImGui::SameLine(ImGui::GetWindowWidth() * 0.4f);
                if (marks[i].intensity >= 0 ) {
                    ImGui::TextColored(
                        intensityColors[marks[i].intensity], 
                        "%s", 
                        intensityLevels[marks[i].intensity]
                    );
                } else {
                    ImGui::Text("end %zu", marks[i].end);
                }
                ImGui::SameLine(ImGui::GetWindowWidth() * 0.6f);
                
                if (ImGui::Button("Play")) {
                    // Play audio from this marker
                    playAudioSegment(marks[i].sample);
                }
                ImGui::SameLine();
                if (ImGui::Button("See")) {
                    printf("See marker %zu\n", i);
                }
                ImGui::SameLine();
                if (ImGui::Button("Delete")) {
                    marks.erase(marks.begin() + i);
                    i--; // Adjust loop counter
                    // Save to CSV after deletion
                    saveMarkersToCsv();
                }
                
                ImGui::Separator();
                ImGui::PopID();
            }
            ImGui::EndChild();
        }
        
        // Right column - Dropdown and additional controls
        ImGui::NextColumn();
        {
            const char* intensityLevels[] = {"Low", "Med", "High", "Very High"};
            
            // Dropdown for intensity
            ImGui::Text("Intensity Level");
            if (ImGui::BeginCombo("##Intensity", intensityLevels[currentIntensity])) {
                for (int n = 0; n < 4; n++) {
                    bool is_selected = (currentIntensity == n);
                    if (ImGui::Selectable(intensityLevels[n], is_selected)) {
                        currentIntensity = n;
                    }
                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            
            // Keyboard shortcuts for intensity
            ImGuiIO& io = ImGui::GetIO();
            if (io.KeysDown[ImGuiKey_1]) currentIntensity = 0;
            if (io.KeysDown[ImGuiKey_2]) currentIntensity = 1;
            if (io.KeysDown[ImGuiKey_3]) currentIntensity = 2;
            if (io.KeysDown[ImGuiKey_4]) currentIntensity = 3;
        }
        
        ImGui::Columns(1);
        ImGui::EndChild();


        ImGui::End();

        ImGui::Render();
        glViewport(0, 0, windowWidth, windowHeight);
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    bool handleEvents() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                return false;
            if (event.type == SDL_WINDOWEVENT && 
                event.window.event == SDL_WINDOWEVENT_RESIZED) {
                windowWidth = event.window.data1;
                windowHeight = event.window.data2;
            }
        }
        return true;
    }

private:
    SDL_Window* window = nullptr;
    SDL_GLContext glContext = nullptr;
    SDL_AudioDeviceID audioDevice = 0;
    std::vector<float> playbackBuffer;
    bool isPlaying = false;
    int needsSectionsUpdate = -1;
    int windowWidth, windowHeight;
    size_t section_mark = 0;
    AudioProcessor audioProcessor;
    int currentIntensity = 0;
    std::vector<Marker> marks;
    std::string currentWavFile;
    const char* intensityLevels[4] = {"Low", "Med", "High", "Very High"};

    ImVec4 intensityColors[4] = {
        ImVec4(0.4f, 0.8f, 0.4f, 1.0f),   // blue for Low
        ImVec4(1.0f, 1.0f, 0.0f, 1.0f),   // Yellow for Med
        ImVec4(1.0f, 0.5f, 0.0f, 1.0f),   // Orange for High
        ImVec4(1.0f, 0.0f, 0.0f, 1.0f)    // Red for Very High
    };

    void insertMarkSorted(size_t mark, int currentIntensity) {
        // Find the position where mark should be inserted
        auto pos = lower_bound(marks.begin(), marks.end(), mark, 
            [](const Marker& a, size_t b) { return a.sample < b; });
        
        // Insert at the found position
        marks.insert(pos, {mark, currentIntensity});
        
        // Save to CSV
        saveMarkersToCsv();
    }

    void insertSectionSorted(size_t mark, int currentIntensity) {
        // Find the position where mark should be inserted
        auto pos = lower_bound(marks.begin(), marks.end(), mark, 
            [](const Marker& a, size_t b) { return a.sample < b; });
        printf("section_mark = %ld\n", section_mark);
        if (section_mark != 0) {
            // Insert at the found position
            // FIXME: remove marks between section_mark and mark.
            size_t min_number = std::min(mark, section_mark);
            size_t max_number = std::max(mark, section_mark);
            marks.insert(pos, {min_number, currentIntensity, max_number});
            // Save to CSV
            saveMarkersToCsv();
            section_mark = 0;
            printf("section marked end\n");
        } else {
            printf("section marked start\n");
            section_mark = mark;
        }
    }

    void initAudioPlayback() {
        SDL_AudioSpec want, have;
        SDL_zero(want);
        want.freq = audioProcessor.getSampleRate();
        want.format = AUDIO_F32;
        want.channels = 1;
        want.samples = 4096;
        want.callback = nullptr; // We'll use SDL_QueueAudio for streaming

        audioDevice = SDL_OpenAudioDevice(
            nullptr, 0, &want, &have, 
            SDL_AUDIO_ALLOW_FORMAT_CHANGE
        );

        if (audioDevice == 0) {
            printf("Failed to open audio device: %s\n", SDL_GetError());
        }
    }

    void playAudioSegment(size_t startSample) {
        // Stop any ongoing playback
        if (isPlaying) {
            SDL_PauseAudioDevice(audioDevice, 1);
            SDL_ClearQueuedAudio(audioDevice);
        }

        // Get samples from AudioProcessor
        auto samples = audioProcessor.getSamples();
        
        // Ensure we don't overflow
        size_t segmentLength = 2000;
        size_t endSample = std::min(startSample + segmentLength, samples.size());
        
        // Create playback buffer
        playbackBuffer.clear();
        playbackBuffer.insert(
            playbackBuffer.begin(), 
            samples.begin() + startSample, 
            samples.begin() + endSample
        );

        // Queue audio
        SDL_QueueAudio(audioDevice, playbackBuffer.data(), 
            playbackBuffer.size() * sizeof(float));
        
        // Start playback
        SDL_PauseAudioDevice(audioDevice, 0);
        isPlaying = true;
    }
};

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <wav_file>\n", argv[0]);
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO) != 0) {
        printf("SDL initialization failed: %s\n", SDL_GetError());
        return 1;
    }


    AudioVisualizer visualizer;
    if (!visualizer.init(argv[1])) {
        printf("Failed to initialize visualizer. Make sure the file is MONO\n");
        return 1;
    }

    while (visualizer.handleEvents()) {
        visualizer.render();
    }

    visualizer.cleanup();
    return 0;
}