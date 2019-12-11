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
            explicit FileManager(const std::string& file, bool app=true);
            ~FileManager();

            /// Open the file
            /// \param froce_reopen If true, close the file and reopen it no matter what
            /// \return true if successful, else false
            bool Open(bool force_reopen=false);

            /// Write in the file
            /// \return true if successful, else false
            bool Write(const std::string& s);

            /// Add a value to add in the file
            /// \param val the value to add
            bool operator<<(int val);
            /// Add a string to add in the file
            /// \param str the string to add
            bool operator<<(const std::string& str);

            /// Check whether the file is open, accessible and the stream is in a good state.
            /// Also Checks for filbit and badbit just in case.
            /// \return true on success, false otherwise.
            explicit operator bool();

        private:
            bool app;
            std::string file;
            std::mutex file_mutex;
            std::ofstream file_stream;
        };
    }
}

#endif //DARWIN_FILEMANAGER_HPP
