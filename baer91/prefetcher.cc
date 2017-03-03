/**
 * Copyright (c) 2017 Viet-Hoa Do <viethoad[at]stud.ntnu.no>
 *                    Martin Stypinski <mstypinski[at]gmail.com>
 * All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 *
 * Reference:
 *   [1] Jean-Loup Baer, Tien-Fu Chen, "An Effective On-Chip Preloading Scheme
 *       To Reduce Data Access Penalty", in ACM/IEEE Conference on Super-
 *       -computing, 1991, pp. 176-186
 *   [2] Johnny K. F. Lee, Alan Jay Smith, "Branch prediction strategies
 *       and branch target buffer design", Computer, pp. 6-22, Jan. 1984
 */

#include "interface.hh"

#include <cstdio>
#include <cstdarg>

#include <queue>
#include <deque>
#include <map>
#include <algorithm>

#define MAX_HISTORY 16384

/* ---------------------------------------------------------------- Logging */
#define LOGD(...) PrintLog(__PRETTY_FUNCTION__, __VA_ARGS__)

void PrintLog(const char* func, const char* format, ...)
{
    static char buffer[1000];

    int len = sprintf(buffer, "DEBUG ");
    
    va_list args;
    va_start(args, format);
    len += vsprintf(&buffer[len], format, args);
    va_end(args);

    sprintf(&buffer[len], " (%s)\n", func);

    DPRINTF(HWPrefetch, "%s", buffer);
}

/* ------------------------------------------------ Stride cache prefetcher */
enum { RPT_STATE_INIT, RPT_STATE_TRANSIENT,
       RPT_STATE_STEADY, RPT_STATE_NO_PRED };

struct inst_t
{
    Addr pc;
    Tick last_time;
    
    int is_forward; /* Simple branch prediction */
    
    Addr prev_addr;
    int64_t stride;
    int state;
};

std::map<Addr, inst_t> insts;
std::map<Tick, Addr> pc_history;

void inst_execute(Addr pc, Tick time)
{
    Addr last_pc =
        (pc_history.size() > 0) ? pc_history.rbegin()->second : 0;
    
    /* Adds the PC to the history */
    if (insts.find(pc) != insts.end())
    {
        /* If this instruction has been already in the history,
           updates the last access time */
        
        Tick prev_time = insts[pc].last_time;
        insts[pc].last_time = time;

        pc_history.erase(prev_time);
        pc_history[time] = pc;
    }
    else
    {
        /* If this instruction is not in the history, adds it to history */
        
        if (insts.size() == MAX_HISTORY)
        {
            /* Instruction history is full, removes the outdated entry */
            Addr oldest_pc = pc_history.begin()->second;
            pc_history.erase(pc_history.begin());
            insts.erase(oldest_pc);
        }

        /* Adds new pc to history */
        inst_t inst;

        inst.pc = pc;
        inst.last_time = time;
        
        inst.is_forward = 1;

        inst.prev_addr = 0;
        inst.stride = 0;
        inst.state = RPT_STATE_INIT;
        
        insts[pc] = inst;
        pc_history[time] = pc;
    }

    /* Updates the branch prediction of the previous one */
    if (last_pc != 0)
        insts[last_pc].is_forward = (pc > last_pc);
}

void mem_access(Addr pc, Addr addr, int miss)
{
    inst_t& inst = insts[pc];

    /* Updates reference prediction table */
    int correct = (inst.prev_addr + inst.stride == addr);
    Addr prev_addr = inst.prev_addr;
    
    inst.prev_addr = addr;

    switch (inst.state)
    {
    case RPT_STATE_INIT:
        if (correct) inst.state = RPT_STATE_STEADY;
        else
        {
            inst.state = RPT_STATE_TRANSIENT;
            inst.stride = addr - prev_addr;
        }
        
        break;

    case RPT_STATE_TRANSIENT:
        if (correct) inst.state = RPT_STATE_STEADY;
        else
        {
            inst.state = RPT_STATE_NO_PRED;
            inst.stride = addr - prev_addr;
        }
        
        break;

    case RPT_STATE_NO_PRED:
        if (correct) inst.state = RPT_STATE_TRANSIENT;
        else
        {
            inst.state = RPT_STATE_TRANSIENT;
            inst.stride = addr - prev_addr;
        }
        
        break;

    case RPT_STATE_STEADY:
        if (!correct) inst.state = RPT_STATE_INIT;
        
        break;
    }
}

void stride_prefetch(Addr pc)
{
    inst_t& inst = insts[pc];

    /* If the execution is likely to go forward, tries to prefetch
       the next RPT entry stride. Otherwise, ignores it */
    if (!inst.is_forward) return;

    /* Gets the next entry */
    std::map<Addr, inst_t>::iterator it = insts.find(pc);
    ++it;
    if (it == insts.end()) return;

    inst_t& next_inst = it->second;

    /* If it is in steady state, prefetches */
    if (next_inst.state == RPT_STATE_STEADY)
    {
        Addr pf_addr = next_inst.prev_addr + next_inst.stride;
        if (!in_cache(pf_addr) && !in_mshr_queue(pf_addr))
        {
            LOGD("issue_prefetch: addr = 0x%016x, pc=0x%016x, "
                 "current_queue_size = %d",
                 pf_addr, pc, current_queue_size());
            issue_prefetch(pf_addr);
        }
    }
}

/* --------------------------------- Standard hardware prefetcher interface */
void prefetch_init(void)
{
    LOGD("prefetch_init");
}

void prefetch_access(AccessStat stat)
{
    LOGD("prefetch_access: addr = 0x%016x, pc = 0x%016x, miss = %d",
         stat.mem_addr, stat.pc, stat.miss);

    Addr addr = stat.mem_addr + BLOCK_SIZE;
    addr &= ~(Addr)(BLOCK_SIZE - 1);

    inst_execute(stat.pc, stat.time);
    mem_access(stat.pc, addr, stat.miss);
    stride_prefetch(stat.pc);
}

void prefetch_complete(Addr addr)
{
    LOGD("prefetch_complete: addr = 0x%016x, current_queue_size = %d",
         addr, current_queue_size());
}
