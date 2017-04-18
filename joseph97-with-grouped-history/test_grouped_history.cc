/**
 * Copyright (c) 2017 Viet-Hoa Do <viethoad[at]stud.ntnu.no>
 *                    Martin Stypinski <mstypinski[at]gmail.com>
 * All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <iostream>
#include <stdint.h>

#include "grouped_history.hh"

class Callbacks : public GroupedHistoryCallbacks<int>
{
public:
    virtual bool CanMerge(const GroupedHistoryEntry<int>& a,
                          const GroupedHistoryEntry<int>& b)
    { return (a.Data == b.Data); }
};

Callbacks historyCallbacks;
GroupedHistory<int> history(historyCallbacks, 1, 2);

int main()
{
    history.Update(0, 3, 0); history.Print();
    history.Update(1, 5, 0); history.Print();
    history.Update(2, 7, 1); history.Print();
    history.Update(3, 9, 2); history.Print();
    history.Update(4, 6, 1); history.Print();
    history.Update(5, 6, 0); history.Print();

    return 0;
}
