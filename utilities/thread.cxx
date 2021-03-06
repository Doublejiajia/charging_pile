/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-01-27     imgcr       the first version
 */

#include "thread.hxx"
#include <stdexcept>

using namespace std;

void Thread::run(void *p) {
    try {
        onRun();
    } catch(const exception& e) {
        rt_kprintf("\033[31m[%s]{%s}%s\n\033[0m", rt_thread_self()->name, typeid(e).name(), e.what());
    }

}

