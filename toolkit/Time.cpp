#include "Time.hpp"
#include <time.h>

namespace darwin {
    namespace time_utils {
        std::string GetTime(){
            char str_time[256];
            time_t rawtime;
            struct tm * timeinfo;
            std::string res;

            time(&rawtime);
            timeinfo = localtime(&rawtime);
            strftime(str_time, sizeof(str_time), "%F%Z%T%z", timeinfo);
            res = str_time;

            return res;
        }
    }
}
