#ifndef DARWIN_FILEMANAGER_HPP
#define DARWIN_FILEMANAGER_HPP

#include <mutex>
#include <thread>
#include <fstream>

#include "tsl/hopscotch_map.h"

/// \namespace darwin
namespace darwin {
    /// \namespace toolkit
    namespace toolkit {

        class FileManager {

        public:
            /// FileManager constructor
            /// \param file the file's path to manage
            /// \param append if we append data to the file
            explicit FileManager(std::string file, bool append);
            ~FileManager();

            /// Open the file
            /// \return true if successful, else false
            bool Open();

            /// Write in the file
            /// \return true if successful, else false
            bool Write(std::string);

            /// Add a value to add in the file
            /// \param val the value to add
            bool operator<<(int val);
            /// Add a string to add in the file
            /// \param str the string to add
            bool operator<<(std::string str);

        private:
            bool app;
            std::string file;
            std::mutex file_mutex;
            std::ofstream file_stream;
        };
    }
}

#endif //DARWIN_FILEMANAGER_HPP
