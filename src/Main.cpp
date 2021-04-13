#include <Windows.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>

LARGE_INTEGER counterEpoch;
LARGE_INTEGER counterFrequency;
FILE* logFile;

float GetElapsed() {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    auto result =
        (t.QuadPart - counterEpoch.QuadPart)
        / (float)counterFrequency.QuadPart;
    return result;
}

#define TIME()\
    fprintf(logFile, "[%f]", GetElapsed());

#define LOC()\
    fprintf(logFile, "[%s:%d]", __FILE__, __LINE__);

#define LOG(level, ...)\
    LOC()\
    TIME()\
    fprintf(logFile, "[%s] ", level);\
    fprintf(logFile, __VA_ARGS__); \
    fprintf(logFile, "\n"); \
    fflush(logFile);

#define FATAL(...)\
    LOG("FATAL", __VA_ARGS__);\
    exit(1);

#define WARN(...) LOG("WARN", __VA_ARGS__);

#define ERR(...) LOG("ERROR", __VA_ARGS__);

#define INFO(...)\
    LOG("INFO", __VA_ARGS__)

#define CHECK(x, ...)\
    if (!x) { FATAL(__VA_ARGS__) }

#define LERROR(x) \
    if (x) { \
        char buffer[1024]; \
        strerror_s(buffer, errno); \
        FATAL(buffer); \
    }

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

#include "jcwk/FileSystem.cpp"

bool done = false;
FILE* file = NULL;
size_t wordLength = sizeof(u32);
size_t bufferLength = 1000;
size_t bufferSize = wordLength * bufferLength;
u32* buffer = NULL;
size_t currentWord = 0;
size_t readWords = 0;

u32 getw() {
    if (currentWord >= readWords) {
        readWords = readFromFile(file, 1000, buffer);
        if (readWords % 4 != 0) {
            FATAL("file not aligned on word boundaries");
        }
        readWords /= 4;
        currentWord = 0;
    }
    done = feof(file);
    return buffer[currentWord++];
}

int main(int argc, char** argv) {
    // NOTE: Initialize logging.
    {
        auto error = fopen_s(&logFile, "LOG", "w");
        if (error) return 1;

        QueryPerformanceCounter(&counterEpoch);
        QueryPerformanceFrequency(&counterFrequency);
    }

    if (argc < 1) {
        return -1;
    }

    file = openFile(argv[1], "rb");
    buffer = (u32*)malloc(bufferSize);
    
    auto magic = getw();
    if (magic != 0x07230203) {
        FATAL("not a spirv file");
    }
    auto version = getw();
    auto generator = getw();
    auto bound = getw();
    auto instructionSchema = getw();

    while (!done) {
        u32 opCode = getw();
        if (done) break;

        u32 opCodeEnumerant = opCode & 0xFFFF;
        u32 wordCount = opCode >> 16;

        if (opCodeEnumerant == 23) {
            auto resultId = getw();
            auto componentType = getw();
            auto componentCount = getw();
            char typePrefix = ' ';
            INFO("type vector: resultId %d, componentType %d, componentCount %d", resultId, componentType, componentCount);
        } else if (opCodeEnumerant == 32) {
            auto resultId = getw();
            auto storage = getw();
            auto typeId = getw();
            INFO("type pointer: resultId %d, storage %d, typeId %d", resultId, storage, typeId);
        } else if (opCodeEnumerant == 59) {
            auto resultType = getw();
            auto resultId = getw();
            auto storageClass = getw();
            if (wordCount > 4) {
                auto initializer = getw();
                INFO("variable: resultId %d, resultType %d, storageClass %d, initializer %d", resultId, resultType, storageClass, initializer);
            } else {
                INFO("variable: resultId %d, resultType %d, storageClass %d", resultId, resultType, storageClass);
            }
        } else if (opCodeEnumerant == 71) {
            auto id = getw();
            auto decoration = getw();
            if ((decoration == 30) && (wordCount > 3)) {
                auto location = getw();
                INFO("%d is at location %d", id, location);
            } else {
                INFO("decorate %d with %d", id, decoration);
            }
        } else if (opCodeEnumerant == 72) {
            auto stype = getw();
            auto member = getw();
            auto decoration = getw();
            INFO("decorate member %d of %d with %d", member, stype, decoration);
        } else {
            for (i32 i = 0; i < (i32)wordCount - 1; i++) {
                opCode = getw();
            }
        }
    }

    free(buffer);

    return 0;
}
