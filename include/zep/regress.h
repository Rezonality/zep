#pragma once

#include "zep/mcommon/animation/timer.h"

namespace Zep
{

class ZepEditor;
class ZepRegress : public ZepExCommand
{
public:
    ZepRegress(ZepEditor& editor);
    
    static void Register(ZepEditor& editor);
   
    virtual void Tick();
    virtual void Run(const std::vector<std::string>& tokens) override;
    virtual void Notify(std::shared_ptr<ZepMessage> message) override;
    virtual const char* Name() const override;

private:
    timer m_timer;
    bool m_enable = false;
};

}