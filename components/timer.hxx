/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-12-29     imgcr       the first version
 */

#ifndef COMPONENTS_TIMER_HXX_
#define COMPONENTS_TIMER_HXX_

#include <rtthread.h>
#include <memory>
#include <utilities/signals.hxx>



class Timer {
public:
    Timer(rt_tick_t time, const char* name, rt_uint8_t flags = RT_TIMER_FLAG_PERIODIC);
    virtual void start();
    virtual void stop();
    bool isRunning();
protected:
    virtual void run();
public:
    Signals<void()> onRun = {};
private:
    std::shared_ptr<rt_timer> timer;
    bool running = false;
    rt_uint8_t flags;
};

#endif
