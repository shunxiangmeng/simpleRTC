#pragma once
#include <stdint.h>
#include <memory>
#include <array>

#include "socket_util.h"

namespace infra {

class EventDriver {
public:

    EventDriver();

    virtual ~EventDriver();

    virtual bool Wait(int64_t wait_duration /*ms*/, bool process_io = false);

    virtual void WakeUp();

private:


private:
    
    std::array<int, 2> fd_;

};
    


}