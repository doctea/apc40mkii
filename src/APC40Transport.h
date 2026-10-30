#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

class APC40Transport {
public:
    virtual ~APC40Transport() = default;
    virtual bool open() = 0;
    virtual void close() = 0;
    virtual bool isConnected() const = 0;
    virtual bool sendMessage(const uint8_t* data, std::size_t size) = 0;
    virtual void poll() {}

    std::function<void(const uint8_t*, std::size_t)> onMessage;
    std::function<void(bool)> onConnectionChanged;
};