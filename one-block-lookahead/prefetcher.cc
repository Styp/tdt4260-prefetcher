/**
 * Copyright (c) 2017 Viet-Hoa Do <viethoad[at]stud.ntnu.no>
 *                    Martin Stypinski <mstypinski[at]gmail.com>
 * All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 *
 * Reference:
 */

#include "interface.hh"

#include <cstdio>
#include <cstdarg>

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

/* --------------------------------- Standard hardware prefetcher interface */
void prefetch_init(void)
{
    LOGD("prefetch_init");
}

void prefetch_access(AccessStat stat)
{
    LOGD("prefetch_access: addr = 0x%016x, pc = 0x%016x, miss = %d",
         stat.mem_addr, stat.pc, stat.miss);

    Addr pf_addr = stat.mem_addr + BLOCK_SIZE;
    pf_addr &= ~(Addr)(BLOCK_SIZE - 1);

    if (stat.miss && !in_cache(pf_addr))
    {
        LOGD("issue_prefetch: addr = 0x%016x, pc=0x%016x, "
             "current_queue_size = %d",
             pf_addr, stat.pc, current_queue_size());
        issue_prefetch(pf_addr);
    }
}

void prefetch_complete(Addr addr)
{
    LOGD("prefetch_complete: addr = 0x%016x, current_queue_size = %d",
         addr, current_queue_size());
}
