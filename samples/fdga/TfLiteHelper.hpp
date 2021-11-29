#pragma once


#include "tensorflow/lite/interpreter.h"
#include "tensorflow/lite/model.h"

///
/// \brief Subclass of tflite::ErrorReporter to log TF lite errors to the darwin
///        logger
/// 
///
class DarwinTfLiteErrorReporter: public tflite::ErrorReporter {
    public:
        ~DarwinTfLiteErrorReporter() = default;
        static DarwinTfLiteErrorReporter* GetInstance() noexcept ;
        int Report(const char* format, va_list args) noexcept override final;
    private:
        DarwinTfLiteErrorReporter() = default;
        static DarwinTfLiteErrorReporter tfErrorReporter;
};

///
/// \brief Class that will hold a pointer to the model and 
///        dispatch thread_local tflite interpreters
/// 
///
class DarwinTfLiteInterpreterFactory {
    public:
        DarwinTfLiteInterpreterFactory() = default;
        ~DarwinTfLiteInterpreterFactory() = default;

        //No copy, no move
        DarwinTfLiteInterpreterFactory(const DarwinTfLiteInterpreterFactory&) = delete;
        DarwinTfLiteInterpreterFactory& operator=(const DarwinTfLiteInterpreterFactory&) = delete;

        DarwinTfLiteInterpreterFactory(DarwinTfLiteInterpreterFactory&&) = delete;
        DarwinTfLiteInterpreterFactory& operator=(DarwinTfLiteInterpreterFactory&&) = delete;

        ///
        /// \brief TF lite interpreters are *NOT* thread-safe, to ensure thread safety in darwin, 
        ///        this function returns a pointer to a thread_local tflite::interpreter
        ///        This method must be called AFTER Factory::SetModel
        ///        In case of an error (model not set), it kills its process and returns a nullptr
        /// 
        /// \return std::shared_ptr<tflite::Interpreter> pointer to a thread_local allocated interpreter, may be null if no model is set
        ///
        std::shared_ptr<tflite::Interpreter> GetInterpreter();

        ///
        /// \brief Static instance of the tf lite reporter used to log errors in darwin
        /// 
        ///
        void SetModel(std::shared_ptr<tflite::FlatBufferModel> model);
    private:
        std::shared_ptr<tflite::FlatBufferModel> _model;
};