/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-01-19     imgcr       the first version
 */

#include <components/persistent_storage.hxx>
#include <config/app.hxx>
#include <Lock.h>
#include "current_limiter.hxx"

using namespace std;
using namespace rtthread;
using namespace Things::Decos;

CurrentLimiter::CurrentLimiter(outer_t* outer): Base(outer), mutex(kMutex) {

    //每个端口一个定时器
    for(auto& timer: timers) {
        timer = make_shared<Timer>(kDuration, kTimer, RT_TIMER_FLAG_ONE_SHOT);
    }

    inited.onChanged += [this](auto value) {
        if(!value) return;
        for(auto i = 0u; i < Config::Bsp::kPortNum; i++) {
            auto charger = getInfo(i).charger;
            auto timer = timers[i];

            charger->multimeterChannel->current += [this, charger, timer, i](auto value) {
                auto guard = Lock(mutex);
                auto storage = Preset::PersistentStorage::get();
                auto params = storage->make<Params>();
                if(!value) return;

                rt_kprintf("port%d current: %dmA\n", i, *value);
                if(*value > params->maxCurrentMiA) {
                    if(!timer->isRunning() && charger->stateStore->oState.value() == State::Charging) timer->start();
                } else {
                    if(timer->isRunning()) timer->stop();
                }
            };

            timer->onRun += [charger, i, this]() {
                auto guard = Lock(mutex);
                if(charger->stateStore->oState.value() == State::Charging) {
                    charger->stop();
                    this->outer->onCurrentLimit(i);
                }
            };
        }
    };
}

void CurrentLimiter::init() {
    inited = true;
}

void CurrentLimiter::config(int currentLimit, int uploadThr, int fuzedThr, int noloadCurrThr) {
    auto params = Preset::PersistentStorage::get()->make<Params>();
    params->maxCurrentMiA = currentLimit;
}

