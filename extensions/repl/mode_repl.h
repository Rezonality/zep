#pragma once

#include <zep/mode.h>
#include <future>
#include <memory>
#include <regex>

namespace Zep
{

enum class ReplParseType
{
    SubExpression,
    OuterExpression,
    All
};

// A provider that can handle repl commands
// This is just a default repl that does nothing; if you want to provide a repl 
// you need to register this interface and handle the messages to run the repl.
struct IZepReplProvider
{
    virtual std::string ReplParse(const ZepBuffer& text, ByteIndex cursorOffset, ReplParseType type)
    {
        ZEP_UNUSED(text);
        ZEP_UNUSED(cursorOffset);
        ZEP_UNUSED(type);

        return "<Supply IZepReplProvider>";
    }

    virtual std::string ReplParse(const std::string& text) 
    {
        ZEP_UNUSED(text);
        return "<Supply IZepReplProvider>";
    };
    virtual bool ReplIsFormComplete(const std::string& input, int& depth)
    {
        // The default repl assumes all commands are complete.
        ZEP_UNUSED(input);
        depth = 0;
        return true;
    }
};

class ZepReplExCommand : public ZepExCommand
{
public:
    ZepReplExCommand(ZepEditor& editor, IZepReplProvider* pProvider);

    static void Register(ZepEditor& editor, IZepReplProvider* pProvider);
    
    virtual void Notify(std::shared_ptr<ZepMessage> message) override;
    virtual void Run(const std::vector<std::string>& args) override;
    virtual const char* ExCommandName() const override { return "ZRepl"; }
    virtual const KeyMap* GetKeyMappings(ZepMode&) const override { return &m_keymap; }

private:
    void Prompt();
    void MoveToEnd();

private:
    IZepReplProvider* m_pProvider = nullptr;
    ZepBuffer* m_pReplBuffer = nullptr;
    ZepWindow* m_pReplWindow = nullptr;
    KeyMap m_keymap;
};

class ZepReplEvaluateCommand : public ZepExCommand
{
public:
    ZepReplEvaluateCommand(ZepEditor& editor, IZepReplProvider* pProvider);

    static void Register(ZepEditor& editor, IZepReplProvider* pProvider);
    
    virtual void Notify(std::shared_ptr<ZepMessage> message) override { ZEP_UNUSED(message); }
    virtual void Run(const std::vector<std::string>& args) override;
    virtual const char* ExCommandName() const override { return "ZReplEval"; }
    virtual const KeyMap* GetKeyMappings(ZepMode&) const override { return &m_keymap; }
private:
    IZepReplProvider* m_pProvider = nullptr;
    KeyMap m_keymap;
};


} // namespace Zep
