#include "common/common.h"

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include <map>
#include <string>

static FILE *generation_log_ = NULL;
const static size_t kBufferSize = 128;

void
InitGenerationLog(std::map<std::string,uint32_t>& gen_map) {
    CHECK(!generation_log_);
    generation_log_ = fopen("/dev/shm/ntfa.gen", "a+");
    CHECK(generation_log_);
    char handle[kBufferSize];
    while (NULL != fgets(handle, kBufferSize, generation_log_)) {
        size_t new_line = strlen(handle) - 1;
        handle[new_line] = '\0';
        gen_map[handle]++;
    }
    return;
}

void
LogGeneration(const std::string& handle) {
    const char *buf = handle.c_str();
    CHECK(0 <= fprintf(generation_log_, "%s\n", buf));
    CHECK(0 == fflush(generation_log_));
    CHECK(0 == fsync(fileno(generation_log_)));
    return;
}
