#include "engine.hpp"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

class TrtLogger : public nvinfer1::ILogger {
    void log(Severity severity, const char* msg) noexcept override {
        if (severity <= Severity::kWARNING) {
            std::cerr << "[TRT] " << msg << '\n';
        }
    }
};

TrtLogger gLogger;

size_t BytesPerElement(nvinfer1::DataType dtype) {
    switch (dtype) {
        case nvinfer1::DataType::kHALF:
            return 2;
        case nvinfer1::DataType::kINT8:
            return 1;
        case nvinfer1::DataType::kINT32:
            return 4;
        case nvinfer1::DataType::kBOOL:
            return 1;
        default:
            return 4;  // kFLOAT
    }
}

}  // namespace

Engine::Engine(const std::string& engine_path) {
    Load(engine_path);
    AllocateBuffers();
    cudaStreamCreate(&stream_);
}

Engine::~Engine() {
    for (auto& tensor_binding : tensors_) {
        cudaFree(tensor_binding.gpu_ptr);
    }
    if (stream_) {
        cudaStreamDestroy(stream_);
    }
    delete context_;
    delete engine_;
    delete runtime_;
}

bool Engine::IsLoaded() const {
    return context_ != nullptr;
}

void* Engine::GetInputBuffer() const {
    return input_buffer_;
}
void* Engine::GetOutputBuffer() const {
    return output_buffer_;
}
cudaStream_t Engine::GetStream() const {
    return stream_;
}
int Engine::GetOutputNumDetections() const {
    return output_num_detections_;
}
int Engine::GetOutputNumFields() const {
    return output_num_fields_;
}
nvinfer1::DataType Engine::GetOutputDataType() const {
    return output_dtype_;
}

void Engine::Load(const std::string& engine_path) {
    std::ifstream file(engine_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open engine file: " + engine_path);
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> data(size);
    if (!file.read(data.data(), size)) {
        throw std::runtime_error("Cannot read engine file: " + engine_path);
    }

    runtime_ = nvinfer1::createInferRuntime(gLogger);
    if (!runtime_) {
        throw std::runtime_error("Failed to create TRT runtime");
    }

    // deserializeCudaEngine reconstructs the compiled model from the binary blob.
    engine_ = runtime_->deserializeCudaEngine(data.data(), data.size());
    if (!engine_) {
        throw std::runtime_error("Failed to deserialize engine");
    }

    context_ = engine_->createExecutionContext();
    if (!context_) {
        throw std::runtime_error("Failed to create execution context");
    }
}

void Engine::AllocateBuffers() {
    int num_tensors = engine_->getNbIOTensors();
    tensors_.resize(num_tensors);

    for (int tensor_idx = 0; tensor_idx < num_tensors; ++tensor_idx) {
        const char* name = engine_->getIOTensorName(tensor_idx);
        nvinfer1::Dims dims = engine_->getTensorShape(name);
        nvinfer1::DataType dtype = engine_->getTensorDataType(name);

        size_t num_elements = 1;
        for (int dim_idx = 0; dim_idx < dims.nbDims; ++dim_idx) {
            num_elements *= static_cast<size_t>(dims.d[dim_idx]);
        }

        tensors_[tensor_idx].name = name;
        cudaMalloc(&tensors_[tensor_idx].gpu_ptr, num_elements * BytesPerElement(dtype));

        if (engine_->getTensorIOMode(name) == nvinfer1::TensorIOMode::kINPUT) {
            if (!input_buffer_) {
                input_buffer_ = tensors_[tensor_idx].gpu_ptr;
            }
        } else {
            // First output: YOLOv10 NMS-free head, shape [1, num_det, num_fields]
            if (!output_buffer_) {
                output_buffer_ = tensors_[tensor_idx].gpu_ptr;
                output_dtype_ = dtype;
                if (dims.nbDims >= 3) {
                    output_num_detections_ = dims.d[1];
                    output_num_fields_ = dims.d[2];
                } else if (dims.nbDims == 2) {
                    output_num_detections_ = dims.d[0];
                    output_num_fields_ = dims.d[1];
                }
            }
        }
    }

    const char* dtype_str = (output_dtype_ == nvinfer1::DataType::kHALF) ? "FP16" : "FP32";
    std::cout << "[Engine] Tensors: " << num_tensors << " | Output shape: ["
              << output_num_detections_ << ", " << output_num_fields_ << "] " << dtype_str << "\n";
}

void Engine::Infer() {
    for (auto& tensor_binding : tensors_) {
        context_->setTensorAddress(tensor_binding.name.c_str(), tensor_binding.gpu_ptr);
    }
    context_->enqueueV3(stream_);
}
