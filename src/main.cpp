#include <SDL.h>
#include <cstdio>

#include "AudioVisulizationQueue.hpp"

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

struct AudioCallbackData
{
    float currentFreq = 220.0;
    double currentPhase = 0.0;
    float currentAmplitude = 0.5;
    uint8_t silence = 0;
    AudioVisualizationQueue<double, 2400> visualizationQueue;
};

static void audioCallback(void *userdata, Uint8 *stream, int len)
{
    AudioCallbackData &callbackData = *reinterpret_cast<AudioCallbackData *>(userdata);
    if (callbackData.currentAmplitude < DBL_EPSILON) {
        callbackData.currentPhase = 0.0;
    }

    int16_t *data = reinterpret_cast<int16_t *>(stream);

    for (int i = 0; i < (len / sizeof(int16_t)); i++)
    {
        double val = sin(2.0 * M_PI * callbackData.currentPhase) * callbackData.currentAmplitude;
        callbackData.visualizationQueue.writeBlocking(std::span<double>(&val, 1));
        data[i] = (int16_t)(val * std::numeric_limits<int16_t>::max());

        double deltaPhase = callbackData.currentFreq / 48000.0;
        callbackData.currentPhase += deltaPhase;
        while (callbackData.currentPhase >= 1.0)
        {
            callbackData.currentPhase -= 1.0;
        }
    }
}

static AudioCallbackData callbackData;
static SDL_Window *window;
static SDL_Renderer *renderer;

static std::vector<double> waveform = std::vector<double>(2400, 0.0);
static std::vector<SDL_FPoint> waveformPoints = std::vector<SDL_FPoint>(2400, SDL_FPoint{.x = 0.0, .y = 0.0});

static bool quit = false;
static bool audioInit = false;

static void initAudio() {
    SDL_InitSubSystem(SDL_INIT_AUDIO);
    SDL_AudioSpec desiredSpec{
        .freq = 48000,
        .format = AUDIO_S16SYS,
        .channels = 1,
        .samples = 1024,
        .callback = &audioCallback,
        .userdata = &callbackData};

    SDL_AudioSpec obtainedSpec;

    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(nullptr, 0, &desiredSpec, &obtainedSpec, 0);

    callbackData.silence = obtainedSpec.silence;

    SDL_PauseAudioDevice(dev, 0);

    audioInit = true;
}

static int audioEventFilter(void*, SDL_Event* e) {
    switch(e->type) {
        case SDL_FINGERDOWN:
        case SDL_MOUSEBUTTONDOWN:
            initAudio();
            SDL_SetEventFilter(nullptr, nullptr);
            return 0;
        default:
            break;
    }

    return 1;
}

static void runLoop()
{
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        ImGui_ImplSDL2_ProcessEvent(&e);
        switch (e.type)
        {
        case SDL_QUIT:
            quit = true;
            break;
        default:
            break;
        }
    }

    ImGui_ImplSDL2_NewFrame();
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowSize(ImVec2(300.0, 75.0), ImGuiCond_Once);
    ImGui::SetNextWindowPos(ImVec2(675.0, 10.0), ImGuiCond_Once);

    ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_NoDecoration);

    ImGui::SliderFloat("Frequency", &callbackData.currentFreq, 220.0, 500.0, "%.1f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SliderFloat("Amplitude", &callbackData.currentAmplitude, 0.0, 0.75, "%.3f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::End();

    ImGui::Render();

    #ifndef __EMSCRIPTEN__
    callbackData.visualizationQueue.readIntoBlocking(waveform);
    #else
    bool updateWaveform = callbackData.visualizationQueue.tryReadInto(waveform);

    static int pointsToDraw = 0;

    if(updateWaveform) {
    #endif
    pointsToDraw = 0;

    waveformPoints = std::vector<SDL_FPoint>(2400, SDL_FPoint{.x = 0.0, .y = 0.0});

    int counter = 0;
    double prevVal = waveform[0];
    bool triggered = false;
    int triggerCounter = 0;

    for(int i = 0; i < waveform.size(); i++) {
        double val = waveform[i];
        if(!triggered) {
            if(val - prevVal > 0.0 && val > callbackData.currentAmplitude * 0.9) {
                triggerCounter ++;
            }
            if(triggerCounter >= 3) {
                triggered = true;
            }
            if(!triggered) {
                continue;
            }
        }

        waveformPoints[i] = SDL_FPoint{.x = (float)counter / (float)500 * 1000.0f, .y = (float)(val * 500.0 + 500.0)};
        pointsToDraw++;
        prevVal = val;
        counter++;

        if(counter > 1000) {
            for(int j = counter; j < waveformPoints.size(); j++) {
                waveformPoints[j] = SDL_FPoint {.x = (float) j / (float)500 * 1000.0f, .y = 500};
            }
            break;
        }
    }

    #ifdef __EMSCRIPTEN__
    }
    #endif

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);

    SDL_RenderDrawLinesF(renderer, waveformPoints.data(), pointsToDraw);

    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());

    SDL_RenderPresent(renderer);
}

int main(int, char **)
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        std::fprintf(stderr, "Error: Failed to initialize Audio Subsytem\n");
        return 1;
    }

    #ifdef __EMSCRIPTEN__
    Uint32 windowFlags = 0;
    int w = 1000, h = 1000;
    #else
    Uint32 windowFlags = SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE;
    int w = 0, h = 0;
    #endif

    SDL_SetEventFilter(audioEventFilter, nullptr);

    window = SDL_CreateWindow("Music Demo", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h, windowFlags);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);

    SDL_RenderSetLogicalSize(renderer, 1000, 1000);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    // io.ConfigFlags |= ImGuiConfigFlags_IsTouchScreen;
    io.DisplaySize.x = 1000.f;
    io.DisplaySize.y = 1000.f;
    ImGui::StyleColorsDark();

    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

   

#ifndef __EMSCRIPTEN__
    while (!quit)
    {
        runLoop();
    }
#else
        emscripten_set_main_loop(runLoop, -1, 1);
#endif
}