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

            /// Start the file manager
            /// \return true if successful start, else false
            bool Start();

            /// Add a value to add in the file
            /// \param val the value to add
            void operator<<(int val);
            /// Add a character to add in the file
            /// \param ch the character to add
            void operator<<(char ch);
            /// Add a c string to add in the file
            /// \param str the c string to add
            void operator<<(char* str);
            /// Add a string to add in the file
            /// \param str the string to add
            void operator<<(std::string str);

        private:
            /// Open the file
            /// \return true if successful, else false
            bool Open();
            /// Write in the file
            void Write();
            /// Extract data to write in the file from the buffer
            /// \return the string to write in file
            std::string ExtractBuffer();

        private:
            bool app;
            bool is_stop; // Thread loop variable

            std::string file, buf;
            std::iostream lol;
            std::fstream file_stream;

            std::thread thread;
            std::condition_variable cv;
            std::mutex file_mutex, buf_mutex;
        };
    }
}

#endif //DARWIN_FILEMANAGER_HPP
