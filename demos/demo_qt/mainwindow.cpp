#define NOMINMAX
#include <mutils/chibi/chibi.h>
#include <mutils/time/time_provider.h>
#define _MATH_DEFINES_DEFINED
#include "chibi/eval.h"

#include <QCommandLineParser>
#include <QMenu>
#include <QMenuBar>
#include <QPainter>
#include <QWidget>

#include "zep/buffer.h"
#include "zep/editor.h"
#include "zep/mode_standard.h"
#include "zep/mode_vim.h"
#include "zep/regress.h"
#include "zep/tab_window.h"
#include "zep/theme.h"
#include "zep/window.h"

#include "orca/mode_orca.h"

// Repl example
#include "config_app.h"
#include "mainwindow.h"
#include "repl/mode_repl.h"
#include "zep/qt/zepwidget_qt.h"

#include "demo_common.h"

using namespace Zep;
using namespace MUtils;

const std::string shader = R"R(
#version 330 core

uniform mat4 Projection;

// Coordinates  of the geometry
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec2 in_tex_coord;
layout(location = 2) in vec4 in_color;

// Outputs to the pixel shader
out vec2 frag_tex_coord;
out vec4 frag_color;

void main()
{
    gl_Position = Projection * vec4(in_position.xyz, 1.0);
    frag_tex_coord = in_tex_coord;
    frag_color = in_color;
}
)R";

namespace
{
Chibi scheme;
}

MainWindow::MainWindow()
{
    QCommandLineParser parser;
    parser.setApplicationDescription("Test helper");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("file", QCoreApplication::translate("file", "file to load."));

    parser.process(*qApp);

    chibi_init(scheme, qApp->applicationDirPath().toStdString());

    auto* pWidget = new ZepWidget_Qt(this, qApp->applicationDirPath().toStdString(), DemoFontPtSize);

    // Register our extensions
    ZepMode_Orca::Register(pWidget->GetEditor());
    ZepRegressExCommand::Register(pWidget->GetEditor());
    ZepReplExCommand::Register(pWidget->GetEditor(), this);

    const QStringList args = parser.positionalArguments();
    if (args.size() > 0)
    {
        pWidget->GetEditor().InitWithFileOrDir(args[0].toStdString());
    }
    else
    {
        pWidget->GetEditor().InitWithText("Shader.vert", shader);
    }

    setContentsMargins(2, 2, 2, 2);

    auto menu = new QMenuBar();
    //auto pFile = menu->addMenu("File");
    auto pSettings = menu->addMenu("Settings");
    {
        auto pMode = pSettings->addMenu("Editor Mode");
        {
            auto pVim = pMode->addAction("Vim");
            auto pStandard = pMode->addAction("Standard");

            auto pCurrentWindow = pWidget->GetEditor().GetActiveTabWindow()->GetActiveWindow();
            assert(pCurrentWindow);
            const auto& buffer = pCurrentWindow->GetBuffer();

            bool enabledVim = strcmp(buffer.GetMode()->Name(), Zep::ZepMode_Vim::StaticName()) == 0;
            pVim->setCheckable(true);
            pStandard->setCheckable(true);
            pVim->setChecked(enabledVim);
            pStandard->setChecked(!enabledVim);

            connect(pVim, &QAction::triggered, this, [pWidget, pVim, pStandard]() {
                pVim->setChecked(true);
                pStandard->setChecked(false);
                pWidget->GetEditor().SetGlobalMode(ZepMode_Vim::StaticName());
            });
            connect(pStandard, &QAction::triggered, this, [pWidget, pStandard, pVim]() {
                pVim->setChecked(false);
                pStandard->setChecked(true);
                pWidget->GetEditor().SetGlobalMode(ZepMode_Standard::StaticName());
            });
        }
        auto pTheme = pSettings->addMenu("Theme");
        {
            auto pDark = pTheme->addAction("Dark");
            auto pLight = pTheme->addAction("Light");

            bool enabledDark = pWidget->GetEditor().GetTheme().GetThemeType() == ThemeType::Dark ? true : false;
            pDark->setCheckable(true);
            pLight->setCheckable(true);
            pDark->setChecked(enabledDark);
            pLight->setChecked(!enabledDark);

            connect(pDark, &QAction::triggered, this, [pWidget, pDark, pLight]() {
                pDark->setChecked(true);
                pLight->setChecked(false);
                pWidget->GetEditor().GetTheme().SetThemeType(ThemeType::Dark);
            });
            connect(pLight, &QAction::triggered, this, [pWidget, pLight, pDark]() {
                pDark->setChecked(false);
                pLight->setChecked(true);
                pWidget->GetEditor().GetTheme().SetThemeType(ThemeType::Light);
            });
        }
    }

    setMenuBar(menu);
    setCentralWidget(pWidget);

    TimeProvider::Instance().StartThread();
}

MainWindow::~MainWindow()
{
    TimeProvider::Instance().EndThread();
}

std::string MainWindow::ReplParse(const std::string& str)
{
    auto ret = chibi_repl(scheme, NULL, str);
    ret = RTrim(ret);
    return ret;
}

std::string MainWindow::ReplParse(ZepBuffer& buffer, const GlyphIterator& cursorOffset, ReplParseType type)
{
    ZEP_UNUSED(cursorOffset);
    ZEP_UNUSED(type);

    auto ret = chibi_repl(scheme, NULL, buffer.GetWorkingBuffer().string());
    ret = RTrim(ret);
    return ret;
}

bool MainWindow::ReplIsFormComplete(const std::string& str, int& indent)
{
    int count = 0;
    for (auto& ch : str)
    {
        if (ch == '(')
            count++;
        if (ch == ')')
            count--;
    }

    if (count < 0)
    {
        indent = -1;
        return false;
    }
    else if (count == 0)
    {
        return true;
    }

    int count2 = 0;
    indent = 1;
    for (auto& ch : str)
    {
        if (ch == '(')
            count2++;
        if (ch == ')')
            count2--;
        if (count2 == count)
        {
            break;
        }
        indent++;
    }
    return false;
}
