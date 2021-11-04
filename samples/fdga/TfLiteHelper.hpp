#pragma once


#include "tensorflow/lite/interpreter.h"
#include "tensorflow/lite/model.h"

class DarwinTfLiteErrorReporter: public tflite::ErrorReporter {
    public:
        ~DarwinTfLiteErrorReporter() = default;
        static DarwinTfLiteErrorReporter* GetInstance() noexcept ;
        int Report(const char* format, va_list args) noexcept override final;
    private:
        DarwinTfLiteErrorReporter() = default;
        static DarwinTfLiteErrorReporter tfErrorReporter;
};

class DarwinTfLiteInterpreterFactory {
    public:
        DarwinTfLiteInterpreterFactory() = default;
        ~DarwinTfLiteInterpreterFactory() = default;

        //No copy, no move
        DarwinTfLiteInterpreterFactory(const DarwinTfLiteInterpreterFactory&) = delete;
        DarwinTfLiteInterpreterFactory& operator=(const DarwinTfLiteInterpreterFactory&) = delete;

        DarwinTfLiteInterpreterFactory(DarwinTfLiteInterpreterFactory&&) = delete;
        DarwinTfLiteInterpreterFactory& operator=(DarwinTfLiteInterpreterFactory&&) = delete;
        
        std::shared_ptr<tflite::Interpreter> GetInterpreter();
        void SetModel(std::shared_ptr<tflite::FlatBufferModel> model);
    private:
        std::shared_ptr<tflite::FlatBufferModel> _model;
};