package main

import (
	"encoding/binary"
	"fmt"
	"os"
	"runtime"
	"strconv"

	"github.com/AllenDang/cimgui-go/backend"
	"github.com/AllenDang/cimgui-go/backend/sdlbackend"
	"github.com/AllenDang/cimgui-go/examples/common"
	"github.com/AllenDang/cimgui-go/imgui"
	"github.com/AllenDang/cimgui-go/imguizmo"
	"github.com/AllenDang/cimgui-go/implot"
	"github.com/AllenDang/cimgui-go/utils"
)

var currentBackend backend.Backend[sdlbackend.SDLWindowFlags]

var (
	audioSamples []float64
	sampleRate   uint32
	markers      []int64
)

type WavHeader struct {
	ChunkID       [4]byte
	ChunkSize     uint32
	Format        [4]byte
	Subchunk1ID   [4]byte
	Subchunk1Size uint32
	AudioFormat   uint16
	NumChannels   uint16
	SampleRate    uint32
	ByteRate      uint32
	BlockAlign    uint16
	BitsPerSample uint16
}

func init() {
	runtime.LockOSThread()
}

func main() {
	if len(os.Args) < 2 {
		fmt.Println("expected file")
	}
	if len(os.Args) < 2 {
		fmt.Println("Usage: program <wav_file>")
		os.Exit(1)
	}

	if err := loadWavFile(os.Args[1]); err != nil {
		fmt.Printf("Error loading WAV file: %v\n", err)
		os.Exit(1)
	}
	common.Initialize()

	currentBackend, _ = backend.CreateBackend(sdlbackend.NewSDLBackend())
	currentBackend.SetAfterCreateContextHook(common.AfterCreateContext)
	currentBackend.SetBeforeDestroyContextHook(common.BeforeDestroyContext)

	currentBackend.SetBgColor(imgui.NewVec4(0.45, 0.55, 0.6, 1.0))

	currentBackend.CreateWindow("Hello from cimgui-go", 1200, 900)

	currentBackend.SetDropCallback(func(p []string) {
		fmt.Printf("drop triggered: %v", p)
	})

	currentBackend.SetCloseCallback(func() {
		fmt.Println("window is closing")
	})

	// currentBackend.SetIcons(common.Image())

	// currentBackend.Run(common.Loop)

	currentBackend.Run(func() {
		showWaveForm()
	})
}

func loadWavFile(filename string) error {
	file, err := os.OpenFile(filename, os.O_RDONLY, 0644)
	if err != nil {
		return fmt.Errorf("failed to open file: %v", err)
	}
	defer file.Close()

	var header WavHeader
	if err := binary.Read(file, binary.LittleEndian, &header); err != nil {
		return fmt.Errorf("failed to read WAV header: %v", err)
	}

	// Verify it's a WAV file
	if string(header.ChunkID[:]) != "RIFF" || string(header.Format[:]) != "WAVE" {
		return fmt.Errorf("not a valid WAV file")
	}

	// Find data chunk
	dataChunk := make([]byte, 8)
	for {
		if _, err := file.Read(dataChunk); err != nil {
			return fmt.Errorf("failed to find data chunk: %v", err)
		}
		if string(dataChunk[:4]) == "data" {
			break
		}
		size := binary.LittleEndian.Uint32(dataChunk[4:])
		file.Seek(int64(size), 1)
	}

	// Read samples
	audioSamples = make([]float64, 0)
	sampleRate = header.SampleRate

	for {
		var sample int16
		err := binary.Read(file, binary.LittleEndian, &sample)
		if err != nil {
			break
		}
		// Normalize to [-1, 1]
		audioSamples = append(audioSamples, float64(sample)/32768.0)
	}

	return nil
}

func showWaveForm() {
	// imgui.ClearSizeCallbackPool()
	// imguizmo.BeginFrame()

	// var barValues []int64
	// for i := 0; i < 10; i++ {
	// 	barValues = append(barValues, int64(i+1))
	// }

	// basePos := imgui.MainViewport().Pos()
	// imgui.SetNextWindowPosV(imgui.NewVec2(basePos.X+400, basePos.Y+60), imgui.CondOnce, imgui.NewVec2(0, 0))
	// imgui.SetNextWindowSizeV(imgui.NewVec2(500, 300), imgui.CondOnce)
	// imgui.Begin("Plot window")
	// if implot.BeginPlotV("Plot", imgui.NewVec2(-1, -1), 0) {
	// 	implot.PlotBarsS64PtrInt("Bar", utils.SliceToPtr(barValues), int32(len(barValues)))
	// 	implot.PlotLineS64PtrInt("Line", utils.SliceToPtr(barValues), int32(len(barValues)))
	// 	implot.EndPlot()
	// }
	// imgui.End()
	imgui.ClearSizeCallbackPool()
	imguizmo.BeginFrame()

	basePos := imgui.MainViewport().Pos()
	imgui.SetNextWindowPosV(imgui.NewVec2(basePos.X+400, basePos.Y+60), imgui.CondOnce, imgui.NewVec2(0, 0))
	imgui.SetNextWindowSizeV(imgui.NewVec2(800, 400), imgui.CondOnce)

	imgui.Begin("Waveform Viewer")

	if implot.BeginPlotV("Waveform", imgui.NewVec2(-1, -1), 0) {
		// Convert audio samples to plot data
		plotData := make([]float32, len(audioSamples))
		xAxis := make([]float32, len(audioSamples))

		for i := range audioSamples {
			plotData[i] = float32(audioSamples[i])
			xAxis[i] = float32(i)
		}
		// Plot waveform
		// implot.PlotLineFloatPtrInt("Waveform",
		// 	utils.SliceToPtr(xAxis),
		// 	utils.SliceToPtr(plotData),
		// 	int32(len(plotData)),
		// 	0, 0)
		implot.PlotLineFloatPtrInt("Waveform",
			utils.SliceToPtr(plotData),
			int32(len(audioSamples)))

		// Handle plot interactions
		// if implot.IsPlotHovered() && imgui.IsMouseClicked(0) {
		if implot.IsPlotHovered() {
			mousePos := implot.GetPlotMousePos()
			sampleIndex := int64(mousePos.X)
			if sampleIndex >= 0 && sampleIndex < int64(len(audioSamples)) {
				markers = append(markers, sampleIndex)
				fmt.Printf("Clicked at sample: %d\n", sampleIndex)
			}
		}

		// Draw markers
		for i, marker := range markers {
			// markerX := []float64{float64(marker)}
			markerY := []float64{-1.0, 1.0}
			implot.PlotLineS64PtrInt(fmt.Sprintf("#%d", strconv.Itoa(i)), &marker, 1)
			// implot.PlotLineFloatPtrInt("##Marker",
			// 	utils.SliceToPtr([]float64{float64(marker), float64(marker)}),
			// 	utils.SliceToPtr(markerY),
			// 	2, 0, 0)
		}

		implot.EndPlot()
	}

	// Display markers information
	imgui.Text(fmt.Sprintf("Sample Rate: %d Hz", sampleRate))
	imgui.Text(fmt.Sprintf("Total Samples: %d", len(audioSamples)))
	if len(markers) > 0 {
		imgui.Text("Markers:")
		for i, marker := range markers {
			imgui.Text(fmt.Sprintf("Marker %d: Sample %d (%.3f seconds)",
				i+1, marker, float64(marker)/float64(sampleRate)))
		}
	}

	imgui.End()
}
