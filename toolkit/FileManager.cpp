#include "FileManager.hpp"

#include <thread>
#include <string.h>
#include "base/Logger.hpp"

/// \namespace darwin
namespace darwin {
    /// \namespace toolkit
    namespace toolkit{

        FileManager::FileManager(std::string& file, bool app)
                : app{app}, file{file} {}

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
            }catch (std::fstream::failure& e) {
                std::cerr << "Error when opening file..." << e.what();
                return false;
            }
            return file_stream.is_open();
        }

        bool FileManager::Write(std::string s){
            if(!Open()){
                return false;
            }
            try {
                std::lock_guard<std::mutex> lock(file_mutex);
                file_stream << s << std::flush;
            }catch (std::ofstream::failure& e) {
                std::cerr << "Exception when writing in file..." << e.what();
                return false;
            }

            return true;
        }

        bool FileManager::operator<<(std::string& str) {
            return Write(str);
        }

        bool FileManager::operator<<(int val) {
            return Write(std::to_string(val));
        }

        FileManager::~FileManager() {
            file_stream.close();
        }
    }
}
