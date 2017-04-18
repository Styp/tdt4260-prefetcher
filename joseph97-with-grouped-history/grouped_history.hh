/**
 * Copyright (c) 2017 Viet-Hoa Do <viethoad[at]stud.ntnu.no>
 *                    Martin Stypinski <mstypinski[at]gmail.com>
 * All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#pragma once

#include <stdint.h>
#include <map>
#include <sstream>
#include <iomanip>

#ifndef ADDR
#  define ADDR uint64_t
#endif /* ADDR */

#ifndef TICK
#  define TICK int64_t
#endif /* TICK */

#ifndef LOG
#  include <cstdio>
#
#  define LOG(...) printf(__VA_ARGS__); printf("\n")
#  define LOGD(...) LOG(__VA_ARGS__)
#  define LOGE(...) LOG(__VA_ARGS__)
#endif /* LOG */

#ifndef MAXOF
#   define MAXOF(a, b) (((a) > (b)) ? (a) : (b))
#endif /* MAXOF */

template <typename T>
struct GroupedHistoryEntry
{
    ADDR FirstAddr;
    ADDR LastAddr;
    TICK LastAccess;
    T Data;
};

template <typename T>
class GroupedHistoryCallbacks
{
public:
    virtual bool CanMerge(const GroupedHistoryEntry<T>& a,
                          const GroupedHistoryEntry<T>& b) = 0;
};

template <typename T>
class GroupedHistory
{
private:
    typedef GroupedHistoryEntry<T> Entry;
    typedef std::multimap<TICK, Entry*> EntryByTimeMap;
    typedef typename EntryByTimeMap::iterator EntryByTimeMapIterator;
    typedef std::map<ADDR, Entry*> EntryByAddrMap;
    typedef typename EntryByAddrMap::iterator EntryByAddrMapIterator;

private:
    GroupedHistoryCallbacks<T>& mCallbacks;
    int mBlockSize;
    int mCapacity;

    EntryByTimeMap mEntryByTime;
    EntryByAddrMap mEntryByAddr;

public:
    GroupedHistory<T>(GroupedHistoryCallbacks<T>& callbacks,
                      int blockSize, int capacity)
        : mCallbacks(callbacks), mBlockSize(blockSize), mCapacity(capacity)
    { }

    void Update(TICK accessTime, ADDR addr, const T& data)
    {
        EntryByAddrMapIterator byAddrIt, byAddrPrevIt;

        // Checks whether this address is inside an existing entry in history
        // If yes, tries to either merge them or split the old one into two
        // and insert the new one in between.
        byAddrIt = FindEntryByAddr(addr);

        if (byAddrIt != mEntryByAddr.end())
        {
            Entry *prevEntry = byAddrIt->second;

            Entry newEntry;
            newEntry.FirstAddr = addr;
            newEntry.LastAddr = addr;
            newEntry.LastAccess = accessTime;
            newEntry.Data = data;

            if (mCallbacks.CanMerge(*prevEntry, newEntry))
            {
                UpdateLastAccess(prevEntry->FirstAddr, accessTime);
            }
            else
            {
                // Splits the previous entry into two parts
                if (addr + mBlockSize <= prevEntry->LastAddr)
                    AddNewEntry(addr + mBlockSize,
                                prevEntry->LastAddr,
                                prevEntry->LastAccess,
                                prevEntry->Data);

                if (prevEntry->FirstAddr <= addr - mBlockSize)
                    prevEntry->LastAddr = mBlockSize;
                else RemoveEntry(prevEntry->FirstAddr);
            }
        }

        // If the entry cannot be merged with an existing one, creates new one
        byAddrIt = FindEntryByAddr(addr);
        if (byAddrIt == mEntryByAddr.end())
        {
            AddNewEntry(addr, addr, accessTime, data);

            // Tries to merge with the previous entry
            byAddrIt = mEntryByAddr.find(addr);
            byAddrPrevIt = byAddrIt;

            if (byAddrPrevIt != mEntryByAddr.begin())
            {
                --byAddrPrevIt;

                if (mCallbacks.CanMerge(*(byAddrPrevIt->second), *(byAddrIt->second)))
                {
                    byAddrPrevIt->second->LastAddr = byAddrIt->second->LastAddr;
                    UpdateLastAccess(byAddrPrevIt->second->FirstAddr, accessTime);
                    RemoveEntry(byAddrIt->second->FirstAddr);
                }
            }

            // Tries to merge with the next entry
            byAddrPrevIt = FindEntryByAddr(addr);
            byAddrIt = byAddrPrevIt;
            ++byAddrIt;

            if (byAddrIt != mEntryByAddr.end())
            {
                if (mCallbacks.CanMerge(*(byAddrPrevIt->second), *(byAddrIt->second)))
                {
                    byAddrPrevIt->second->LastAddr = byAddrIt->second->LastAddr;
                    UpdateLastAccess(byAddrPrevIt->second->FirstAddr, accessTime);
                    RemoveEntry(byAddrIt->second->FirstAddr);
                }
            }
        }

        // Removes old entry
        while (mEntryByTime.size() > mCapacity)
        {
            RemoveEntry(mEntryByTime.begin()->second->FirstAddr);
        }
    }

    Entry* Get(ADDR addr)
    {
        EntryByAddrMapIterator it = FindEntryByAddr(addr);
        if (it != mEntryByAddr.end()) return it->second;
        return NULL;
    }

    void Print()
    {
        std::stringstream os;

        os << "History" << std::endl;

        for (EntryByAddrMapIterator it = mEntryByAddr.begin();
             it != mEntryByAddr.end(); ++it)
        {
            Entry* entry = it->second;
            os << "[0x" << std::setfill('0') << std::setw(16) << std::hex << entry->FirstAddr
               << ", 0x" << std::setfill('0') << std::setw(16) << std::hex << entry->LastAddr
               << "] lastAccess = " << entry->LastAccess << ", data = " << entry->Data << std::endl;
        }

        LOGD("%s", os.str().c_str());
    }

private:
    void AddNewEntry(ADDR firstAddr, ADDR lastAddr, TICK lastAccess, const T& data)
    {
        if (firstAddr > lastAddr) return;

        if (mEntryByAddr.find(firstAddr) != mEntryByAddr.end())
        {
            LOGE("Duplicate entry (firstAddr = 0x%016x)", firstAddr);
            return;
        }

        Entry* entry = new Entry();
        entry->FirstAddr = firstAddr;
        entry->LastAddr = lastAddr;
        entry->LastAccess = lastAccess;
        entry->Data = data;

        mEntryByTime.insert(std::pair<TICK, Entry*>(lastAccess, entry));
        mEntryByAddr[firstAddr] = entry;
    }

    EntryByTimeMapIterator FindEntryByTimeIterator(Entry* entry)
    {
        std::pair<EntryByTimeMapIterator, EntryByTimeMapIterator> mmIt =
            mEntryByTime.equal_range(entry->LastAccess);

        for (EntryByTimeMapIterator it = mmIt.first; it != mmIt.second; ++it)
            if (it->second == entry)
                return it;

        return mEntryByTime.end();
    }

    EntryByAddrMapIterator FindEntryByAddr(ADDR addr)
    {
        EntryByAddrMapIterator it = mEntryByAddr.upper_bound(addr);
        if (mEntryByAddr.size() > 0 && it != mEntryByAddr.begin())
        {
            --it;
            if (it->second->FirstAddr <= addr && it->second->LastAddr >= addr) return it;
        }

        return mEntryByAddr.end();
    }

    void UpdateLastAccess(ADDR firstAddr, TICK lastAccess)
    {
        EntryByAddrMapIterator byAddrIt = mEntryByAddr.find(firstAddr);
        if (byAddrIt == mEntryByAddr.end())
        {
            LOGE("Entry not found (firstAddr = 0x%016x)", firstAddr);
            return;
        }

        Entry* entry = byAddrIt->second;

        EntryByTimeMapIterator byTimeIt = FindEntryByTimeIterator(entry);
        if (byTimeIt == mEntryByTime.end())
        {
            LOGE("Entry not found (firstAddr = 0x%016x, lastAccess = %d)",
                 firstAddr, entry->LastAccess);
            return;
        }

        mEntryByTime.erase(byTimeIt);

        entry->LastAccess = lastAccess;
        mEntryByTime.insert(std::pair<TICK, Entry*>(lastAccess, entry));
    }

    void RemoveEntry(ADDR firstAddr)
    {
        EntryByAddrMapIterator byAddrIt = mEntryByAddr.find(firstAddr);
        if (byAddrIt == mEntryByAddr.end())
        {
            LOGE("Entry not found (firstAddr = 0x%016x)", firstAddr);
            return;
        }

        Entry* entry = byAddrIt->second;

        EntryByTimeMapIterator byTimeIt = FindEntryByTimeIterator(entry);
        if (byTimeIt == mEntryByTime.end())
        {
            LOGE("Entry not found (firstAddr = 0x%016x, lastAccess = %d)",
                 firstAddr, entry->LastAccess);
            return;
        }

        mEntryByAddr.erase(byAddrIt);
        mEntryByTime.erase(byTimeIt);
        delete entry;
    }
};
