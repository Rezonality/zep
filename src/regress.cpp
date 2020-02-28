#include  <random>
#include  <iterator>

#include "zep/editor.h"
#include "zep/window.h"
#include "zep/tab_window.h"
#include "zep/regress.h"
#include "zep/mcommon/animation/timer.h"

namespace Zep
{

template<typename Iter, typename RandomGenerator>
Iter select_randomly(Iter start, Iter end, RandomGenerator& g) {
    std::uniform_int_distribution<> dis(0, (int)std::distance(start, end) - 1);
    std::advance(start, dis(g));
    return start;
}

template<typename Iter>
Iter select_randomly(Iter start, Iter end) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    return select_randomly(start, end, gen);
}

ZepRegress::ZepRegress(ZepEditor& editor)
    : ZepExCommand(editor)
{
    timer_start(m_timer);
}

void ZepRegress::Register(ZepEditor& editor)
{
    editor.RegisterExCommand(std::make_shared<ZepRegress>(editor));
}

const char* ZepRegress::Name() const
{
    return "ZRegress";
}

void ZepRegress::Run(const std::vector<std::string>& tokens)
{
    ZEP_UNUSED(tokens);
    m_enable = !m_enable;
    if (m_enable)
    {
        GetEditor().RegisterCallback(this);
    }
    else
    {
        GetEditor().UnRegisterCallback(this);
    }
}

void ZepRegress::Notify(std::shared_ptr<ZepMessage> message)
{
    if (message->messageId == Msg::Tick)
    {
        Tick();
    }
}

void ZepRegress::Tick()
{
    if (!m_enable)
    {
        return;
    }

    auto seconds = timer_get_elapsed_seconds(m_timer);
    if (seconds < .1f)
    {
        return;
    }
    timer_restart(m_timer);

    float fRand1 = rand() / (float)RAND_MAX;
    float fRand2 = rand() / (float)RAND_MAX;

    auto pTab = GetEditor().GetActiveTabWindow();
    auto pActiveWindow = pTab->GetActiveWindow();
    auto& windows = pTab->GetWindows();

    if (fRand1 > .5f)
    {
        if (windows.size() > 1)
        {
            pTab->RemoveWindow(*select_randomly(windows.begin(), windows.end()));
        }
    }
    else if (windows.size() < 10)
    {
        pTab->AddWindow(&pActiveWindow->GetBuffer(), pActiveWindow, fRand2 > .5f ? RegionLayoutType::HBox : RegionLayoutType::VBox);
    }
    GetEditor().RequestRefresh();
}

} // Zep
