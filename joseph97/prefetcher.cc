/**
 * Copyright (c) 2017 Viet-Hoa Do <viethoad[at]stud.ntnu.no>
 *                    Martin Stypinski <mstypinski[at]gmail.com>
 * All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 *
 * Reference:
 *   Doug Joseph and Dirk Grunwald, "Prefetching using Markov Predictors",
 *     Preceedings of the 24th Annual International Symposium on Computer
 *     Architecture (ISCA '97), p. 252-263, June 1997, Denver, Colorado, USA
 */

#include "interface.hh"

#include <deque>
#include <queue>
#include <algorithm>
#include <map>

#include <cstdio>
#include <cstdarg>

#define MAX_NODE   32768
#define MAX_FANOUT 4

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

/* ------------------------------------------------------ Markov-like model */
struct node_t
{
    Addr addr;
    int count; /* Count in history */
    std::deque<Addr> next_misses;
};

std::queue<Addr> miss_history;
std::map<Addr, node_t> nodes;

void model_add_miss(Addr addr)
{
    Addr last_miss_addr = (miss_history.size() > 0) ? miss_history.back() : 0;
    
    /* Removes the outdated history entry */
    if (miss_history.size() == MAX_NODE)
    {
        Addr outdated_addr = miss_history.front();
        node_t& node = nodes[outdated_addr];

        /* If this is the last entry, remove it from the model */
        --node.count;
        if (node.count == 0)
            nodes.erase(outdated_addr);

        /* Removes the history entry */
        miss_history.pop();
    }

    /* Adds new miss access to history */
    miss_history.push(addr);

    /* Creates new model node if it does not exist and increases the count */
    if (nodes.find(addr) == nodes.end())
    {
        node_t node;
        node.addr = addr;
        node.count = 1;

        nodes[addr] = node;
    }
    else ++nodes[addr].count;

    /* Adds the new address to the top of the last miss address prediction */
    if (last_miss_addr != 0)
    {
        LOGD("addr: 0x%016x, last_miss_addr: 0x%016x", addr, last_miss_addr);
        node_t& last_node = nodes[last_miss_addr];
        std::deque<Addr>& next_misses = last_node.next_misses;

        {
            std::deque<Addr>::iterator it = std::find(
                next_misses.begin(), next_misses.end(), addr);
            if (it != next_misses.end())
                next_misses.erase(it);
            if (next_misses.size() == MAX_FANOUT)
                next_misses.pop_front();
        }

        next_misses.push_back(addr);
        if (nodes[last_miss_addr].next_misses.size() != next_misses.size())
            LOGD("ERROR");
        else LOGD("last_miss_node.next_misses.size = %d", next_misses.size());
    }
}

void model_prefetch(Addr addr)
{
    std::map<Addr, node_t>::iterator it = nodes.find(addr);
    if (it == nodes.end()) return;
    node_t& node = it->second;

    LOGD("model_prefetch: addr = 0x%016x, node_count = %d, predict_count = %d",
         addr, nodes.size(), node.next_misses.size());
    if (node.next_misses.size() > 0)
    {
        Addr pf_addr = node.next_misses.back();
        if (!in_cache(pf_addr) && !in_mshr_queue(pf_addr))
        {
            LOGD("issue_prefetch: addr = 0x%016x", addr);
            issue_prefetch(pf_addr);
        }
    }
}

/* ------------------------------------------ Prefetcher standard interface */
void prefetch_init(void)
{
}

void prefetch_access(AccessStat stat)
{
    Addr addr = stat.mem_addr;
    addr &= ~(Addr)(BLOCK_SIZE - 1);

    LOGD("prefetch_access: addr = 0x%016x, miss = %d, current_queue_size = %d",
         addr, stat.miss, current_queue_size());
    
    if (stat.miss)
    {
        model_add_miss(addr);
        model_prefetch(addr);
    }
}

void prefetch_complete(Addr addr)
{
    LOGD("prefetch_complete: addr = 0x%016x, current_queue_size = %d",
         addr, current_queue_size());
}
