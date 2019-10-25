#include "FileManager.hpp"

#include <thread>
#include <string.h>
#include "base/Logger.hpp"

/// \namespace darwin
namespace darwin {
    /// \namespace toolkit
    namespace toolkit{

        FileManager::FileManager(std::string file, bool app=true)
                : app{app}, file{std::move(file)} {}

        bool FileManager::Open() {
            if(file_stream){
                return true;
            }

            if(app){
                file_stream.open(file, std::fstream::in | std::fstream::out | std::fstream::app);
            }
            else{
                file_stream.open(file, std::fstream::in | std::fstream::out);
            }

            return !!file_stream;
        }

        void FileManager::Write(){
            std::mutex mtx;
            std::unique_lock<std::mutex> lock(mtx);

            while (!is_stop) {
                // Wait to have add to write
                cv.wait(lock);

                // Check if the file is Open/Can be opened
                if(!Open()){
                    // In case error when open the file, stop the loop
                    is_stop = true;
                    break;
                }

                std::unique_lock<std::mutex> file_lock(file_mutex);
                // Write the buffer in file
                file_stream << ExtractBuffer();
                file_lock.unlock();
            }
        }

        std::string FileManager::ExtractBuffer() {
            std::unique_lock<std::mutex> lock(buf_mutex);

            std::size_t size = buf.size();
            std::string res(buf.substr(size));
            buf.erase(size);

            lock.unlock();
            return res;
        }

        void FileManager::operator<<(std::string str) {
            buf += str;
            cv.notify_one();
        }

        void FileManager::operator<<(int val) {
            buf += std::to_string(val);
            cv.notify_one();
        }

        void FileManager::operator<<(char ch) {
            buf += ch;
            cv.notify_one();
        }

        void FileManager::operator<<(char *str) {
            buf += str;
            cv.notify_one();
        }

        bool FileManager::Start(){
            if(!Open()){
                return false;
            }

            try {
                is_stop = false;
                thread = std::thread(&FileManager::Write, this);
            } catch (const std::system_error &e) {
                is_stop = true;
                return false;
            }

            return true;
        }

        FileManager::~FileManager() {
            is_stop = true;
            cv.notify_one();
            thread.join();
            file_stream.close();
        }
    }
}
