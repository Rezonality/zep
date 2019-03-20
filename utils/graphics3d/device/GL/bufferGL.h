#pragma once

namespace Mgfx
{

// A helper to draw dynamic data into a geoemtry buffer in opengl
// The idea is to gain some parallelism between writing buffer data and the hardware reading it.
// Really, this is just a simple wrapper around the GL API.
// Conforms to IDeviceBuffer, since DX12 was added to do the same thing.
// The difference here is that GL cannot permanently map the buffer, so calling UnMap is important 
// (as the sample code does)
class BufferGL : public IDeviceBuffer
{
public:
    BufferGL(DeviceGL* pDevice, uint32_t size, uint32_t flags);
    ~BufferGL();

    // Return a pointer to a buffer, with num elements of size typesize free, and the offset 
    // where the buffer was mapped (for later rendering)
    virtual void* Map(uint32_t num, uint32_t typeSize, uint32_t& offset) override;
    virtual void UnMap() override;
    virtual void EnsureSize(uint32_t size) override;
    virtual void Upload() override;
    virtual uint32_t GetByteSize() const override { return m_size; }
    virtual void Bind() const override;
    virtual void UnBind() const override;

private:
    uint32_t GetBufferID() const { return m_bufferID; }
    uint32_t GetBufferType() const;

public:
    DeviceGL* m_pDevice = nullptr;
    uint32_t m_offset = 0;
    uint32_t m_bufferID = 0;
    uint32_t m_lastOffset = 0;
    uint32_t m_lastSize = 0;
    uint32_t m_size = 0;
    uint32_t m_flags = 0;
    bool m_mapped = false;
    bool m_reset = true;
};

} // namespace Mgfx
