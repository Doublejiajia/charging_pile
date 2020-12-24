/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-08-24     imgcr       the first version
 */
#ifndef DEVICES_LOAD_DETECTOR_HXX_
#define DEVICES_LOAD_DETECTOR_HXX_

#include <rtthread.h>
#include <array>
#include <variant>
#include <utilities/observable.hxx>
#include <utilities/observer.hxx>
#include <memory>

class LoadDetector {
public:
    LoadDetector(rt_base_t pin);

private:
    rt_base_t pin;
    std::shared_ptr<rt_timer> timer;

    //kInvdThrDurationMs内少于kInvdThrNMin有效信号则无效
    //参数调整:
    const static int
        kPluseFreqHz = 50, //脉冲频率为市电频率
        kDetectWndMs = 200, //检测窗口
        kValidRatioPc = 60, //有效比率
        kArraySize = kDetectWndMs / (1000 / kPluseFreqHz);

    std::array<rt_int64_t, kArraySize> ticks;
    bool fulled = false;
    int rear = 0;
    Observable<std::optional<bool>> state;

public:
    Observer<std::optional<bool>> oState;
};

#endif /* APPLICATIONS2_LOAD_DETECTOR_HXX_ */