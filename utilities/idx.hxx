/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-01-12     imgcr       the first version
 */
#ifndef UTILITIES_IDX_HXX_
#define UTILITIES_IDX_HXX_

#include <map>
#include <list>
#include <memory>
#include <functional>
#include "err.hxx"
#include <rtthread.h>
#include "thread_local.hxx"
#include <algorithm>

//NOTE: 目前阻止部分交叠申请

template <class T, class Owner, class Addr, Addr Null, Addr Max>
class Idx { //NOTE: 改变指针要调用dispose
public:
    Idx(Addr addr): addr(addr) {
        field[this] = {};
        rt_kprintf("outer ptr: %08x\n", field[this].outer.get());
    }

    Idx(): addr(Null) { //空Idx
        field[this] = {};
    }

    ~Idx() {
        dispose();
        field.erase(this);
    }

    T* operator->() {
        return &getRef();
    }

    T& operator*() {
        return getRef();
    }

    void operator=(Addr addr) {
        dispose();
        this->addr = addr;
    }

    void operator=(const Idx<T, Owner, Addr, Null, Max>& other) {
        dispose();
        addr = other.addr;
    }

    void operator=(nullptr_t) {
        dispose();
        addr = Null;
    }

    bool operator==(const nullptr_t& n) {
        return addr == Null;
    }

    bool operator==(const Idx<T, Owner, Addr, Null, Max>& other) {
        return addr == other.addr;
    }

    T& getRef() { //TODO: 判断地址是否有效
        auto& f = field[this];
        auto outer = f.outer;
        auto& current = f.current;

        //TODO: 判断是否已求值

        if(addr == Null) throw std::out_of_range{"nullptr"};
        if(addr + sizeof(T) - 1 > Max) throw std::out_of_range{"out of addr space"};
        if(!outer) throw std::runtime_error{"ctx not provided"};

        if(!current) { //未访问过成员
            rt_kprintf("cur not accessed yet\n");
            //1. 判断地址是否已存在, 即判断addr
            auto ownerSpecKvp = ownerSpecs.find(outer);
            if(ownerSpecKvp == ownerSpecs.end()) {
                rt_kprintf("owner specs not created yet\n");
                ownerSpecs[outer] = {};
            }

            auto& ownerSpec =  ownerSpecs[outer];
            auto& range = ownerSpec.range;
            auto& count = ownerSpec.count;

            rt_kprintf("range: ");
            printContainer(range);

            rt_kprintf("count: ");
            printContainer(count);

            auto dataKvp = ownerSpec.data.find(addr);
            if(dataKvp != ownerSpec.data.end()) {
                auto& data = dataKvp->second;
                //如果找到, 则需要校验, 判断size是否满足要求
                auto foundIt = std::find(range.begin(), range.end(), addr);
                auto dist = std::distance(range.begin(), foundIt);
                auto nextIt = foundIt;
                ++nextIt;
                auto rangeSize = *nextIt - addr;
                if(rangeSize != sizeof(T)) throw std::runtime_error{"range size mismatch"};
                auto countIt = ownerSpec.count.begin();
                std::advance(countIt, dist);
                auto& countVal = *countIt;
                if(countVal <= 0) throw std::runtime_error{"range count error"};
                current = data.current;
                ++countVal;
                return *(T*)(void*)current.get();
            }

            rt_kprintf("addr not found\n");
            //没找到则创建
            //裂开range、count链表
            auto front = std::lower_bound(range.begin(), range.end(), addr);
            auto rightAddr = addr + sizeof(T);
            if(*front != addr) {
                //需要裂开, 但是得判断与原有的有没有交叉，有的话抛异常

                if(rightAddr > *front) throw not_implemented{"cross detected"};

                //判断是否落在引用计数大于0的范围内
                auto countFront = count.begin();
                std::advance(countFront, std::distance(range.begin(), front) - 1);
                if(*countFront != 0) throw not_implemented{"cross detected"};
                ++countFront;

                if(rightAddr == *front) {
                    //右边不需要裂开
                    range.insert(front, addr);
                    count.insert(countFront, 1);
                } else {
                    //右边需要裂开
                    range.insert(front, addr);
                    count.insert(countFront, 1);
                    range.insert(front, rightAddr);
                    count.insert(countFront, 0);
                }

            } else {
                //左边不需要裂开, 如果不需要裂开, 且计数 > 0, 需要特殊处理

                rt_kprintf("left no need to split\n");

                auto back = front;
                ++back;

                if(rightAddr > *back) throw not_implemented{"cross detected"};
                auto countback = count.begin();
                std::advance(countback, std::distance(range.begin(), back) - 1);
                if(*countback != 0) throw not_implemented{"cross detected"};

                ++*countback; //自身计数+1

                ++countback;

                if(rightAddr != *back) {
                    //右边需要裂开
                    range.insert(back, rightAddr);
                    count.insert(countback, 0);
                }
            }

            rt_kprintf("range: ");
            printContainer(range);

            rt_kprintf("count: ");
            printContainer(count);

            auto data = std::shared_ptr<rt_uint8_t[]>(new rt_uint8_t[sizeof(T)]);
            auto backup = std::shared_ptr<rt_uint8_t[]>(new rt_uint8_t[sizeof(T)]);

            outer->read(addr, data.get(), sizeof(T));
            memcpy(backup.get(), data.get(), sizeof(T));
            current = data;

            rt_kprintf("current ptr: %08x\n", data.get());

            auto& curData = ownerSpec.data[addr];

            curData.current = data;
            curData.backup = backup;
            return *(T*)(void*)current.get();
        }

        return *(T*)(void*)current.get();
    }

    void dispose() {

        rt_kprintf("disposeing\n");

        auto& f = field[this];
        auto outer = f.outer;
        auto& current = f.current;

        if(!current) return;
        rt_kprintf("current ptr: %08x\n", current.get());

        auto& ownerSpec =  ownerSpecs[outer];

        //减少引用计数、合并未引用的range
        auto& range = ownerSpec.range;
        auto& count = ownerSpec.count;

        rt_kprintf("range: ");
        printContainer(range);

        rt_kprintf("count: ");
        printContainer(count);

        auto found = std::find(range.begin(), range.end(), addr);
        auto countIt = count.begin();
        std::advance(countIt, std::distance(range.begin(), found));
        auto beforeIt = countIt;
        --beforeIt;

        auto afterIt = countIt;
        ++afterIt;

        --*countIt;
        if(*countIt <= 0) {
            //准备合并
            if(*afterIt == 0) {
                //合并后面
                auto afterRangeIt = found;
                ++afterRangeIt;

                rt_kprintf("erase range value %d\n", *afterRangeIt);
                rt_kprintf("erase count @%d\n", std::distance(count.begin(), afterIt));
                range.erase(afterRangeIt);
                count.erase(afterIt);
            }

            if(countIt != count.begin()) {

                if(*beforeIt == 0) {
                    auto beforeRangeIt = found;

                    rt_kprintf("erase range value %d\n", *beforeRangeIt);
                    rt_kprintf("erase count @%d\n", std::distance(count.begin(), countIt));

                    range.erase(beforeRangeIt);
                    count.erase(countIt);
                }
            }

            //判断是否满足写回条件
            auto& data = ownerSpec.data[addr];
            if(memcmp(data.current.get(), data.backup.get(), sizeof(T))) {
                outer->write(addr, data.current.get(), sizeof(T));
            }

            ownerSpec.data.erase(addr);
        }

        rt_kprintf("range: ");
        printContainer(range);

        rt_kprintf("count: ");
        printContainer(count);

        current = nullptr;
    }

    template<class U>
    void printContainer(U& t) {
        rt_kprintf("{");
        for(const auto& val: t) {
            rt_kprintf("%d, ", val);
        }
        rt_kprintf("}\n");
    }

private:
    Addr addr;


    struct IdxField {
        std::shared_ptr<Owner> outer = Owner::get();
        std::shared_ptr<rt_uint8_t[]> current = nullptr;
    };

    struct OwnerSpec {
        std::list<Addr> range = {0, Max + 1};
        std::list<std::size_t> count = {0};

        struct Data {
            std::shared_ptr<rt_uint8_t[]> current = nullptr;
            std::shared_ptr<rt_uint8_t[]> backup = nullptr;
        };

        std::map<Addr, Data> data = {};
    };

private:
    static std::map<Idx*, IdxField> field;
    static std::map<std::shared_ptr<Owner>, OwnerSpec> ownerSpecs;
};

template <class T, class Owner, class Addr, Addr Null, Addr Max>
std::map<Idx<T, Owner, Addr, Null, Max>*, typename Idx<T, Owner, Addr, Null, Max>::IdxField> Idx<T, Owner, Addr, Null, Max>::field = {};

template <class T, class Owner, class Addr, Addr Null, Addr Max>
std::map<std::shared_ptr<Owner>, typename Idx<T, Owner, Addr, Null, Max>::OwnerSpec> Idx<T, Owner, Addr, Null, Max>::ownerSpecs = {};


#endif /* UTILITIES_IDX_HXX_ */