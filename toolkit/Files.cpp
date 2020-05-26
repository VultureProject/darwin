#include "Files.hpp"

namespace darwin {
    namespace files_utils {

        std::istream& GetLineSafe(std::istream& is, std::string& t) {
            t.clear();

            // The characters in the stream are read one-by-one using a std::streambuf.
            // That is faster than reading them one-by-one using the std::istream.
            // Code that uses streambuf this way must be guarded by a sentry object.
            // The sentry object performs various tasks,
            // such as thread synchronization and updating the stream state.

            std::istream::sentry se(is, true);
            std::streambuf* sb = is.rdbuf();

            for (;;) {
                int c = sb->sbumpc();
                switch (c) {
                    case '\n':
                        return is;
                    case '\r':
                        if (sb->sgetc() == '\n') {
                            sb->sbumpc();
                        }
                        return is;
                    case EOF:
                        // Also handle the case when the last line has no line ending
                        if (t.empty()) {
                            is.setstate(std::ios::eofbit);
                        }
                        return is;
                    case ' ':
                        break;
                    case '\t':
                        break;
                    default:
                        t += (char)c;
                }
            }
        }

        std::string GetNameFromPath(const std::string& filename) {
            char sep = '/';

        #ifdef _WIN32
            sep = '\\';
        #endif

            size_t i = filename.rfind(sep, filename.length());
            if (i != std::string::npos) {
                return(filename.substr(i+1, filename.length() - i));
            }

            return("");
        }

        void ReplaceExtension(std::string& filename, const std::string& new_extension) {
            std::string::size_type i = filename.rfind('.', filename.length());

            if (i != std::string::npos) {
                if (new_extension.empty())
                    filename = filename.substr(0, i);
                else
                    filename.replace(i+1, new_extension.length(), new_extension);
            }
        }

    }
}
