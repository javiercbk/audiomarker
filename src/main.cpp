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

class AudioVisualizer {
public:
    AudioVisualizer() : windowWidth(1280), windowHeight(720) {}
    
    bool init(const char* wavFile) {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
            return false;
        }

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

        return true;
    }

    void cleanup() {
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
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("Waveform", nullptr, 
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

            
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
                if (ImGui::IsMouseClicked(0) && ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
                    printf("Clicked at sample %ld, num samples: %ld\n", mousePosX, audioProcessor.getNumSamples());
                    if (mousePosX > 0 && mousePosX < audioProcessor.getNumSamples()) {
                        insertMarkSorted(mousePosX);
                    }
                }
            }

            ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
		    ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1);
            std::vector<double> temp_marks;
            temp_marks.reserve(marks.size());
            for (const size_t& mark : marks) {
                temp_marks.push_back(static_cast<double>(mark));
            }
            ImPlot::PlotInfLines("audio marks", temp_marks.data(), temp_marks.size());
            ImPlot::PopStyleColor();
		    ImPlot::PopStyleVar();
            ImPlot::EndPlot();
        }

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
    int windowWidth, windowHeight;
    AudioProcessor audioProcessor;
    std::vector<size_t> marks;

    void insertMarkSorted(size_t mark) {
        // Find the position where mark should be inserted
        auto pos = lower_bound(marks.begin(), marks.end(), mark);
        
        // Insert at the found position
        marks.insert(pos, mark);
    }
};

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <wav_file>\n", argv[0]);
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