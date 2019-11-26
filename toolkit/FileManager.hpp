#ifndef DARWIN_FILEMANAGER_HPP
#define DARWIN_FILEMANAGER_HPP

#include <mutex>
#include <thread>
#include <atomic>
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
            explicit FileManager(std::string& file, bool app=true, bool reopen_on_failure=true);
            ~FileManager();

            /// Open the file
            /// \return true if successful, else false
            bool Open();

            /// Write in the file
            /// \return true if successful, else false
            bool Write(std::string& s);

            /// Add a value to add in the file
            /// \param val the value to add
            bool operator<<(int val);
            /// Add a string to add in the file
            /// \param str the string to add
            bool operator<<(std::string& str);

            /// Set the value of the 'reopen_on_failure' flag.
            /// \param reopen The new value of the flag.
            void SetReOpenOnFailure(bool reopen);

            /// Return whether the manager is currently associated to a file.
            /// \return true if a file is open and associated with this manager, false otherwise.
            bool IsOpen();

        private:
            bool app;
            std::string file;
            std::atomic_bool reopen_on_failure;
            std::mutex file_mutex;
            std::ofstream file_stream;
        };
    }
}

#endif //DARWIN_FILEMANAGER_HPP
