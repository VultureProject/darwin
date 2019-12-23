#include "FileManager.hpp"

#include <thread>
#include <string>
#include <unistd.h>
#include "base/Logger.hpp"

/// \namespace darwin
namespace darwin {
    /// \namespace toolkit
    namespace toolkit{

        FileManager::FileManager(const std::string& file, bool app, bool reopen_on_failure)
                : app{app}, file{file}, reopen_on_failure{reopen_on_failure} {}

        bool FileManager::Open(bool force_reopen) {
            if (not force_reopen && (*this))
                return true;

            std::lock_guard<std::mutex> lock(file_mutex);
            file_stream.close();

            if (app)
                file_stream.open(file, std::ios_base::in | std::ios_base::out | std::ios_base::app);
            else
                file_stream.open(file, std::ios_base::in | std::ios_base::out | std::ios_base::trunc);

            return true && (*this); // Little trick to trigger the bool operator instead of cast
        }

        bool FileManager::Write(const std::string& s){
            if(!Open()){
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

        bool FileManager::operator<<(const std::string& str) {
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

        FileManager::operator bool() {
            return file_stream.is_open() && file_stream.good() && file_stream && (access(file.c_str(), F_OK) != -1);
        }

        FileManager::~FileManager() {
            file_stream.close();
        }
    }
}
