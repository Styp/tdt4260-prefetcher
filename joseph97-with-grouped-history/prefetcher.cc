/**
 * Copyright (c) 2017 Viet-Hoa Do <viethoad[at]stud.ntnu.no>
 *                    Martin Stypinski <mstypinski[at]gmail.com>
 * All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "interface.hh"

typedef int64_t DAddr;
#define MAX_HISTORY (2 * 1024)

/* ---------------------------------------------------------------- Logging */
#include <cstdio>
#include <cstdarg>

#define LOGD(...) PrintLog(__PRETTY_FUNCTION__, "DEBUG", __VA_ARGS__)
#define LOGE(...) PrintLog(__PRETTY_FUNCTION__, "ERROR", __VA_ARGS__)
#define LOG(...) LOGD(__VA_ARGS__)

void PrintLog(const char* func, const char* tag, const char* format, ...)
{
    static char buffer[1000];

    int len = sprintf(buffer, "%s ", tag);

    va_list args;
    va_start(args, format);
    len += vsprintf(&buffer[len], format, args);
    va_end(args);

    sprintf(&buffer[len], " (%s)\n", func);

    DPRINTF(HWPrefetch, "%s", buffer);
}

/* ---------------------------------------------------------------- History */
#include "grouped_history.hh"

class Callbacks : public GroupedHistoryCallbacks<DAddr>
{
public:
    virtual bool CanMerge(const GroupedHistoryEntry<DAddr>& a,
                          const GroupedHistoryEntry<DAddr>& b)
    {
        //return false;
        if (a.Data != b.Data) return false;
        if (a.LastAddr + BLOCK_SIZE < b.FirstAddr) return false;
        return true;
    }
};

Callbacks historyCallbacks;
GroupedHistory<DAddr> history(historyCallbacks, BLOCK_SIZE, MAX_HISTORY);

/* --------------------------------- Standard hardware prefetcher interface */
Addr prev_addr;

void prefetch_init(void)
{
    LOGD("prefetch_init");
    prev_addr = 0;
}

void prefetch_access(AccessStat stat)
{
    LOGD("prefetch_access: addr = 0x%016x, pc = 0x%016x, miss = %d",
         stat.mem_addr, stat.pc, stat.miss);

    Addr addr = stat.mem_addr & ~((Addr)(BLOCK_SIZE - 1));
if (stat.miss)
{
    if (prev_addr != 0)
    {
        DAddr delta = (DAddr)addr - (DAddr)prev_addr;
        history.Update(stat.time, prev_addr, delta);
        LOGD("history_update: time = %d, addr = 0x%016x, delta = %d", stat.time, prev_addr, delta);
    }

    if (prev_addr != addr) prev_addr = addr;
}

    if (stat.miss)
    {
        Addr pf_addr = addr + BLOCK_SIZE;

        GroupedHistoryEntry<DAddr>* entry = history.Get(addr);
        if (entry != NULL && addr + entry->Data > 0 && addr + entry->Data <= MAX_PHYS_MEM_ADDR)
            pf_addr = addr + entry->Data;

        if (!in_cache(pf_addr))
            issue_prefetch(pf_addr);
    }
}

void prefetch_complete(Addr addr)
{
    LOGD("prefetch_complete: addr = 0x%016x, current_queue_size = %d",
         addr, current_queue_size());
}
