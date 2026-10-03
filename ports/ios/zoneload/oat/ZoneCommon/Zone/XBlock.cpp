#include "XBlock.h"

XBlock::XBlock(std::string name, const unsigned index, const XBlockType type)
    : m_name(std::move(name)),
      m_index(index),
      m_type(type),
      m_buffer(nullptr, [](uint8_t*) {}),
      m_buffer_size(0u)
{
}

void XBlock::Alloc(const size_t blockSize)
{
    if (blockSize > 0)
    {
        m_buffer = decltype(m_buffer)(new uint8_t[blockSize](),
                                      [](uint8_t* buffer)
                                      {
                                          delete[] buffer;
                                      });
        m_buffer_size = blockSize;
    }
    else
    {
        m_buffer.reset();
        m_buffer_size = 0;
    }
}

void XBlock::UseExternalBuffer(uint8_t* buffer, const size_t blockSize)
{
    m_buffer = decltype(m_buffer)(buffer, [](uint8_t*) {});
    m_buffer_size = buffer ? blockSize : 0u;
}
