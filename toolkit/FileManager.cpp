#include "FileManager.hpp"

#include <thread>
#include <string.h>
#include "base/Logger.hpp"

/// \namespace darwin
namespace darwin {
    /// \namespace toolkit
    namespace toolkit{

        FileManager::FileManager(std::string& file, bool app, bool reopen_on_failure)
                : app{app}, file{file}, reopen_on_failure{reopen_on_failure} {}

        bool FileManager::Open() {
            if(file_stream.is_open()){
                return true;
            }

            file_stream.exceptions(std::ifstream::failbit | std::ifstream::badbit);
            try {
                if(app){
                    file_stream.open(file, std::fstream::app);
                }
                else{
                    file_stream.open(file);
                }
            } catch (std::fstream::failure& e) {
                std::cerr << "Error when opening file..." << e.what();
                return false;
            }
            return file_stream.is_open();
        }

        bool FileManager::Write(std::string& s){
            if(not this->IsOpen()){
                if (not this->reopen_on_failure or this->Open())
                return false;
            }
            try {
                std::lock_guard<std::mutex> lock(file_mutex);
                file_stream << s << std::flush;
            } catch (std::ofstream::failure& e) {
                std::cerr << "Exception when writing in file..." << e.what();
                return false;
            }
            return true;
        }

        bool FileManager::operator<<(std::string& str) {
            return Write(str);
        }

        bool FileManager::operator<<(int val) {
            std::string val_str = std::to_string(val);
            return Write(val_str);
        }

        bool FileManager::IsOpen() {
            return this->file_stream.is_open();
        }

        void FileManager::SetReOpenOnFailure(bool reopen) {
            reopen_on_failure = reopen;
        }

        FileManager::~FileManager() {
            file_stream.close();
        }
    }
}
