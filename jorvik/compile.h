#pragma once

#include <memory>
#include <utility>
#include <cstdint>
#include <vector>
#include <map>
#include <future>

#include "utils/file/file.h"
#include "meta_tags.h"
#include "jorvik.h"

namespace Mgfx
{

// Messages during compile steps
enum class CompileMessageType
{
    Warning,
    Error,
    Info
};

struct CompileMessage
{
    CompileMessage() {};
    CompileMessage(CompileMessageType type, const fs::path& path)
        :filePath(path),
        text("Unknown issue compiling: " + path.string())
    {
    }

    CompileMessage(CompileMessageType type, const fs::path& path, const std::string& message)
        :filePath(path),
        text(message)
    {
    }

    CompileMessage(CompileMessageType type, const fs::path& path, const std::string& message, int line)
        :filePath(path),
        text(message),
        line(line)
    {
    }

    std::string text;
    fs::path filePath;
    int32_t line = -1;
    std::pair<int32_t, int32_t> range = std::make_pair(-1, -1);
    CompileMessageType msgType = CompileMessageType::Error;
};

enum class CompileState
{
    Invalid,
    Valid
};

class CompileResult
{
public:
    virtual ~CompileResult() {};

    // Basic stuff returned by the compile
    fs::path path;                          // Vector ?
    std::shared_ptr<MetaTags> spTags;
    std::vector<std::shared_ptr<CompileMessage>> messages;
    CompileState state = CompileState::Invalid;
};

enum class CompileResultType
{
    LastValid,
    Latest
};

// An interface on an object that can be compiled; typically a file
struct ICompile
{
    // Source path for this compile 
    virtual fs::path GetSourcePath() { return fs::path(); }

    // Can be Async; called to schedule or directly run a compile
    virtual std::future<std::shared_ptr<CompileResult>> Compile() = 0;

    // Always called on the main thread; called to allow the client to process the result
    virtual void ProcessCompileResult(std::shared_ptr<CompileResult> spResult) = 0;

    virtual std::shared_ptr<CompileResult> GetCompileResult() const = 0;
};

void compile_queue(ICompile* compile);
bool compile_tick();
void compile_cancel(ICompile* pAsset);

// Broadcast from the compiler?
struct CompilerMessage : JorvikMessage
{
    CompilerMessage(ICompile* pObject)
        : pCompiled(pObject)
    {
    }
    ICompile* pCompiled;
};


} // Mgfx
