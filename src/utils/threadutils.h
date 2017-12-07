#pragma once

#include <thread>
#include <future>
#include <chrono>

template<typename R>
bool is_future_ready(std::future<R> const& f)
{
    return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready; 
}

