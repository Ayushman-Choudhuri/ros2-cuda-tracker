#pragma once

#include <NvInfer.h>
#include <cuda_runtime_api.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace vision {
    inline void ThrowOnCudaError(cudaError_t status, const char* operation) {
        if (status != cudaSuccess) {
            throw std::runtime_error(std::string(operation) +
                                     " failed: " + cudaGetErrorString(status));
        }
    }

    struct OutputLayout {
        int detection_count = 0;
        int field_count = 0;
        nvinfer1::DataType data_type = nvinfer1::DataType::kFLOAT;
    };


    class TensorRtEngine {
       public:
        explicit TensorRtEngine(const std::string& engine_path);
        ~TensorRtEngine();

        TensorRtEngine(const TensorRtEngine&) = delete;
        TensorRtEngine& operator=(const TensorRtEngine&) = delete;


        void Infer();

        [[nodiscard]] void* GetInputBuffer() const { return input_buffer_; }
        [[nodiscard]] void* GetOutputBuffer() const { return output_buffer_; }
        [[nodiscard]] cudaStream_t GetStream() const { return stream_; }
        [[nodiscard]] const OutputLayout& GetOutputLayout() const { return output_layout_; }

       private:
        void Deserialize(const std::string& engine_path);
        void AllocateDeviceBuffers();
        void BindTensor(int tensor_index);
        void CacheOutputLayout(const nvinfer1::Dims& dims, nvinfer1::DataType data_type);

        std::unique_ptr<nvinfer1::IRuntime> runtime_;
        std::unique_ptr<nvinfer1::ICudaEngine> engine_;
        std::unique_ptr<nvinfer1::IExecutionContext> context_;

        cudaStream_t stream_ = nullptr;

        std::vector<void*> device_buffers_;
        void* input_buffer_ = nullptr;
        void* output_buffer_ = nullptr;
        OutputLayout output_layout_;
    };

}
