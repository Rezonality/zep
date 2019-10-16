#pragma once

#include <zep/editor.h>

namespace Zep
{

struct ILineWidget
{
    virtual NVec2f GetSize() const = 0; // Required size of the widget
    virtual void MouseDown(const NVec2f& pos, ZepMouseButton button) = 0;
    virtual void MouseUp(const NVec2f& pos, ZepMouseButton button) = 0;
    virtual void MouseMove(const NVec2f& pos) = 0;
    virtual void Draw(const NVec2f& location) = 0;
};

class FloatSlider : public ILineWidget
{
public:
    FloatSlider(ZepEditor& editor)
        : m_editor(editor)
    {

    }
    virtual NVec2f GetSize() const override
    {
        return NVec2f(50.0f, 12.0f);
    }
    virtual void MouseDown(const NVec2f& pos, ZepMouseButton button) override { (void*)&pos, (void*)&button; };
    virtual void MouseUp(const NVec2f& pos, ZepMouseButton button) override { (void*)& pos, (void*)& button; };
    virtual void MouseMove(const NVec2f& pos) override { (void*)&pos; };
    virtual void Draw(const NVec2f& location) override { (void*)& location; };

private:
    ZepEditor& m_editor;
};

} // Zep
