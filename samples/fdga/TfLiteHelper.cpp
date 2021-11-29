#include "TfLiteHelper.hpp"
#include "base/Logger.hpp"

#include <cstdarg>
#include <csignal>
#include <unistd.h>

#include "tensorflow/lite/kernels/register.h"
#include "tensorflow/lite/interpreter_builder.h"

///
/// \brief acquire pointer to model to be used for prediction
/// 
/// \param model pointer to model to be used for prediction
///
void DarwinTfLiteInterpreterFactory::SetModel(std::shared_ptr<tflite::FlatBufferModel> model){
    this->_model = model;
}

///
/// \brief TF lite interpreters are *NOT* thread-safe, to ensure thread safety in darwin, 
///        this function returns a pointer to a thread_local tflite::interpreter
///        This method must be called AFTER Factory::SetModel
///        In case of an error (model not set), it kills its process and returns a nullptr
/// 
/// \return std::shared_ptr<tflite::Interpreter> pointer to a thread_local allocated interpreter, may be null if no model is set
///
std::shared_ptr<tflite::Interpreter> DarwinTfLiteInterpreterFactory::GetInterpreter(){
    static thread_local std::shared_ptr<tflite::Interpreter> interpreter;
    if( ! interpreter){
        if( ! _model) {
            // this case is not possible with the actual workflow, we just make sure that it stays that way in the future
            DARWIN_LOGGER;
            DARWIN_LOG_CRITICAL("DarwinTfLiteInterpreterFactory::GetInterpreter:: Trying to get a TFLite Interpreter before setting the model, exiting");
            kill(getpid(), SIGTERM);
            return nullptr;
        }
        tflite::ops::builtin::BuiltinOpResolver resolver;
        tflite::InterpreterBuilder i_builder(*_model, resolver);
        std::unique_ptr<tflite::Interpreter> interp_unique;
        i_builder(&interp_unique);
        interpreter = std::move(interp_unique);
    }
    return interpreter;
};

///
/// \brief Static instance of the tf lite reporter used to log errors in darwin
/// 
///
DarwinTfLiteErrorReporter DarwinTfLiteErrorReporter::tfErrorReporter;

DarwinTfLiteErrorReporter* DarwinTfLiteErrorReporter::GetInstance() noexcept {
    return &tfErrorReporter;
}

int DarwinTfLiteErrorReporter::Report(const char* format, va_list args) noexcept {
    DARWIN_LOGGER;
	char buf[64];
	int n = std::vsnprintf(buf, sizeof(buf), format, args);
    if (n < 0){
		DARWIN_LOG_ERROR("Tensorflow Error : Error occured but there was an error encoding it");
        return n;
    }
	// Static buffer large enough?
	if (static_cast<size_t>(n) < sizeof(buf)) {
		DARWIN_LOG_ERROR("Tensorflow Error : " + std::string(buf, n));
        return 0;
	}

	// Static buffer too small
	std::string s(n + 1, 0);
	n = std::vsnprintf(const_cast<char*>(s.data()), s.size(), format, args);
    if (n < 0){
		DARWIN_LOG_ERROR("Tensorflow Error : Error occured but there was an error encoding it");
        return n;
    }
    DARWIN_LOG_ERROR("Tensorflow Error : " + s);
	return 0;
}
