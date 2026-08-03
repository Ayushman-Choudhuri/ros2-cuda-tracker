#include "inference/engine.hpp"

#include <cstdint>
#include <fstream>
#include <stdexcept>

#include "utils/logger.hpp"

namespace vision {
    namespace {

        // Both layouts are read from the two trailing dimensions; batch and channel
        // dimensions may precede those, but never replace them.
        constexpr int kMinTrailingDims = 2;

        LogLevel ToLogLevel(nvinfer1::ILogger::Severity severity) {
            switch (severity) {
                case nvinfer1::ILogger::Severity::kINTERNAL_ERROR:
                    return LogLevel::kFatal;
                case nvinfer1::ILogger::Severity::kERROR:
                    return LogLevel::kError;
                case nvinfer1::ILogger::Severity::kWARNING:
                    return LogLevel::kWarn;
                case nvinfer1::ILogger::Severity::kINFO:
                    return LogLevel::kInfo;
                default:
                    return LogLevel::kDebug;
            }
        }

        class TrtLogger : public nvinfer1::ILogger {
            void log(Severity severity, const char* message) noexcept override {
                LOG_AT(ToLogLevel(severity), "TensorRT") << message;
            }
        };

        TrtLogger& TrtLoggerInstance() {
            static TrtLogger logger;
            return logger;
        }

        size_t BytesPerElement(nvinfer1::DataType data_type) {
            switch (data_type) {
                case nvinfer1::DataType::kHALF:
                    return sizeof(uint16_t);
                case nvinfer1::DataType::kINT8:
                case nvinfer1::DataType::kBOOL:
                    return sizeof(uint8_t);
                default:
                    return sizeof(uint32_t);
            }
        }

        std::vector<char> ReadFile(const std::string& path) {
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            if (!file.is_open()) {
                throw std::runtime_error("Cannot open engine file: " + path);
            }

            const std::streamsize byte_count = file.tellg();
            file.seekg(0, std::ios::beg);

            std::vector<char> contents(static_cast<size_t>(byte_count));
            if (!file.read(contents.data(), byte_count)) {
                throw std::runtime_error("Cannot read engine file: " + path);
            }
            return contents;
        }

    }  // namespace

    TensorRtEngine::TensorRtEngine(const std::string& engine_path) {
        Deserialize(engine_path);
        AllocateDeviceBuffers();
        ThrowOnCudaError(cudaStreamCreate(&stream_), "cudaStreamCreate");

        LOG_INFO("Engine") << device_buffers_.size() << " tensors | input [" << input_layout_.width
                           << "x" << input_layout_.height << "] | output ["
                           << output_layout_.detection_count << ", " << output_layout_.field_count
                           << "] "
                           << (output_layout_.data_type == nvinfer1::DataType::kHALF ? "FP16"
                                                                                     : "FP32");
    }

    TensorRtEngine::~TensorRtEngine() {
        for (void* device_memory : device_buffers_) {
            cudaFree(device_memory);
        }
        if (stream_ != nullptr) {
            cudaStreamDestroy(stream_);
        }
    }

    void TensorRtEngine::Infer() {
        if (!context_->enqueueV3(stream_)) {
            throw std::runtime_error("TensorRT failed to enqueue inference");
        }
    }

    void TensorRtEngine::Deserialize(const std::string& engine_path) {
        const std::vector<char> serialized = ReadFile(engine_path);

        runtime_.reset(nvinfer1::createInferRuntime(TrtLoggerInstance()));
        if (!runtime_) {
            throw std::runtime_error("Failed to create TensorRT runtime");
        }

        engine_.reset(runtime_->deserializeCudaEngine(serialized.data(), serialized.size()));
        if (!engine_) {
            throw std::runtime_error("Failed to deserialize engine (GPU architecture mismatch?): " +
                                     engine_path);
        }

        context_.reset(engine_->createExecutionContext());
        if (!context_) {
            throw std::runtime_error("Failed to create TensorRT execution context");
        }
    }

    void TensorRtEngine::AllocateDeviceBuffers() {
        const int tensor_count = engine_->getNbIOTensors();
        device_buffers_.reserve(static_cast<size_t>(tensor_count));

        for (int tensor_index = 0; tensor_index < tensor_count; ++tensor_index) {
            BindTensor(tensor_index);
        }

        if (input_buffer_ == nullptr || output_buffer_ == nullptr) {
            throw std::runtime_error("Engine does not expose both an input and an output tensor");
        }
    }

    void TensorRtEngine::BindTensor(int tensor_index) {
        const char* tensor_name = engine_->getIOTensorName(tensor_index);
        const nvinfer1::Dims dims = engine_->getTensorShape(tensor_name);
        const nvinfer1::DataType data_type = engine_->getTensorDataType(tensor_name);

        size_t element_count = 1;
        for (int dimension = 0; dimension < dims.nbDims; ++dimension) {
            // A dynamic extent is reported as -1, which would wrap into a request
            // for roughly 2^64 bytes.
            if (dims.d[dimension] < 0) {
                throw std::runtime_error("Tensor " + std::string(tensor_name) +
                                         " has a dynamic shape; a fixed-shape engine is required");
            }
            element_count *= static_cast<size_t>(dims.d[dimension]);
        }

        void* device_memory = nullptr;
        ThrowOnCudaError(cudaMalloc(&device_memory, element_count * BytesPerElement(data_type)),
                         "cudaMalloc for engine tensor");
        device_buffers_.push_back(device_memory);

        // Buffers never move, so binding once here keeps Infer() down to the enqueue.
        if (!context_->setTensorAddress(tensor_name, device_memory)) {
            throw std::runtime_error("Failed to bind device buffer for tensor " +
                                     std::string(tensor_name));
        }

        const bool is_input =
            engine_->getTensorIOMode(tensor_name) == nvinfer1::TensorIOMode::kINPUT;
        if (is_input && input_buffer_ == nullptr) {
            input_buffer_ = device_memory;
            CacheInputLayout(dims);
        } else if (!is_input && output_buffer_ == nullptr) {
            output_buffer_ = device_memory;
            CacheOutputLayout(dims, data_type);
        }
    }

    void TensorRtEngine::CacheInputLayout(const nvinfer1::Dims& dims) {
        if (dims.nbDims < kMinTrailingDims) {
            throw std::runtime_error("Input tensor has fewer than two dimensions");
        }

        // Height then width, for the NCHW layout these engines use.
        input_layout_.height = static_cast<int>(dims.d[dims.nbDims - 2]);
        input_layout_.width = static_cast<int>(dims.d[dims.nbDims - 1]);
    }

    void TensorRtEngine::CacheOutputLayout(const nvinfer1::Dims& dims,
                                           nvinfer1::DataType data_type) {
        if (dims.nbDims < kMinTrailingDims) {
            throw std::runtime_error("Output tensor has fewer than two dimensions");
        }

        output_layout_.data_type = data_type;
        output_layout_.detection_count = static_cast<int>(dims.d[dims.nbDims - 2]);
        output_layout_.field_count = static_cast<int>(dims.d[dims.nbDims - 1]);
    }

}  // namespace vision
