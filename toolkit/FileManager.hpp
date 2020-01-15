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
            explicit FileManager(const std::string& file, bool app=true, bool reopen_on_failure=true, std::size_t nb_retry=3);
            ~FileManager();

            /// Open the file
            /// \param force_reopen If true, close the file and reopen it no matter what
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
            std::size_t _nb_retry;
            std::mutex file_mutex;
            std::ofstream file_stream;
        };
    }
}

#endif //DARWIN_FILEMANAGER_HPP
