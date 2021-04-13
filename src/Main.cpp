#include <Windows.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

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

const u32 OpName = 5;
const u32 OpTypeVoid = 2;
const u32 OpTypeFloat = 22;
const u32 OpTypeVector = 23;
const u32 OpTypeMatrix = 24;
const u32 OpTypePointer = 32;
const u32 OpVariable = 59;
const u32 OpDecorate = 71;
const u32 OpDecorateMember = 72;

const u32 DecorationLocation = 30;

bool done = false;
FILE* file = NULL;
size_t wordLength = sizeof(u32);
size_t bufferLength = 1000;
size_t bufferSize = wordLength * bufferLength;
u32* buffer = NULL;
size_t currentWord = 0;
size_t readWords = 0;

u32* typeData = NULL;
u32* stringData = NULL;

struct { u32 key; u32 value; }* locationToVariable = NULL;
struct { u32 key; u32 value; }* variableToTypePointer = NULL;
struct { u32 key; u32 value; }* typePointerToType = NULL;
struct { u32 key; size_t value; }* typeToData = NULL;
struct { u32 key; size_t value; }* idToString = NULL;

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

        switch (opCodeEnumerant) {
            case OpName:
            {
                auto targetId = getw();
                hmput(idToString, targetId, arrlen(stringData));
                auto s = (char*)(buffer + currentWord);
                INFO("name %d %.*s", targetId, (wordCount - 2)*4, s);
                for (u32 i = 2; i < wordCount; i++) {
                    auto word = getw();
                    arrput(stringData, word);
                }
            }
            break;
            case OpTypeVector:
            case OpTypeFloat:
            {
                // Supported types.
                auto resultId = getw();
                hmput(typeToData, resultId, arrlen(typeData));
                arrput(typeData, opCodeEnumerant);
                for (u32 i = 2; i < wordCount; i++) {
                    auto word = getw();
                    arrput(typeData, word);
                }
                INFO("type %d", resultId);
            }
            break;
            case OpTypePointer: {
                auto resultId = getw();
                auto storage = getw();
                auto typeId = getw();
                INFO("type* %d -> typeId %d", resultId, typeId);
                hmput(typePointerToType, resultId, typeId);
            }
            break;
            case OpVariable: {
                auto resultType = getw();
                auto resultId = getw();
                auto storageClass = getw();
                if (wordCount > 4) {
                    auto initializer = getw();
                    INFO("var %d: %d = initializer %d", resultId, resultType, initializer);
                } else {
                    INFO("var %d: %d", resultId, resultType);
                }
                hmput(variableToTypePointer, resultId, resultType);
            }
            break;
            case OpDecorate: {
                auto id = getw();
                auto decoration = getw();
                if ((decoration == DecorationLocation) && (wordCount > 3)) {
                    auto location = getw();
                    hmput(locationToVariable, location, id);
                    INFO("%d is at location %d", id, location);
                } else {
                    INFO("decorate %d with %d", id, decoration);
                }
            }
            break;
            case OpDecorateMember: {
                auto stype = getw();
                auto member = getw();
                auto decoration = getw();
                INFO("decorate member %d of %d with %d", member, stype, decoration);
            }
            break;
            default:
                for (i32 i = 0; i < (i32)wordCount - 1; i++) {
                    opCode = getw();
                }
        }
    }

    auto outFile = openFile("out", "w");
    for (u32 i = 0; i < hmlen(locationToVariable); i++) {
        auto locationVariablePair = locationToVariable[i];
        auto location = locationVariablePair.key;
        auto variable = locationVariablePair.value;
        auto typePointer = hmget(variableToTypePointer, variable);
        auto type = hmget(typePointerToType, typePointer);
        auto data = hmget(typeToData, type);

        fprintf(outFile, "struct Vertex {\n");
        if (OpTypeVector == typeData[data]) {
            auto componentType = typeData[++data];
            auto componentData = hmget(typeToData, componentType);
            if (OpTypeFloat != typeData[componentData]) {
                FATAL("unsupported component type");
            }
            auto componentCount = typeData[++data];
            auto stringIdx = hmget(idToString, variable);
            auto string = (char*)(stringData + stringIdx);
            fprintf(outFile, "    Vec%d %s;\n", componentCount, string);
        }
        fprintf(outFile, "};\n");
    }

    return 0;
}
