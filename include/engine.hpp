#pragma once

#include <NvInfer.h>
#include <cuda_runtime_api.h>

#include <string>
#include <vector>

class Engine {
   public:
    explicit Engine(const std::string& engine_path);
    ~Engine();

    // Non-copyable: Engine owns raw GPU memory and TRT pointers.
    // Copying would duplicate the pointer values, causing double-free on destruction.
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    // Run one forward pass. Call before reading from GetOutputBuffer().
    void Infer();

    bool IsLoaded() const;

    // GPU buffer pointers — write input here, read output after Infer().
    void* GetInputBuffer() const;
    void* GetOutputBuffer() const;
    cudaStream_t GetStream() const;

    // YOLOv10 output shape: [1, num_detections, num_fields]
    int GetOutputNumDetections() const;
    int GetOutputNumFields() const;
    nvinfer1::DataType GetOutputDataType() const;

   private:
    // Step 1: read .engine file from disk and build TRT objects.
    void Load(const std::string& engine_path);

    // Step 2: allocate one GPU buffer per input/output tensor.
    void AllocateBuffers();

    // TensorRT requires three objects:
    //   runtime  — TRT library handle, used only for deserialization
    //   engine   — the compiled model loaded from the .engine file
    //   context  — holds per-inference state; used to launch GPU kernels
    nvinfer1::IRuntime* runtime_{nullptr};
    nvinfer1::ICudaEngine* engine_{nullptr};
    nvinfer1::IExecutionContext* context_{nullptr};

    cudaStream_t stream_{nullptr};  // CUDA stream for async GPU work

    // Each tensor (input or output) gets a name and a GPU memory pointer.
    struct TensorBuffer {
        std::string name;
        void* gpu_ptr{nullptr};
    };
    std::vector<TensorBuffer> tensors_;

    // Convenience pointers into tensors_ — first input and first output.
    void* input_buffer_{nullptr};
    void* output_buffer_{nullptr};

    // Cached output shape extracted from the engine at load time.
    int output_num_detections_{0};
    int output_num_fields_{0};
    nvinfer1::DataType output_dtype_{nvinfer1::DataType::kFLOAT};
};
