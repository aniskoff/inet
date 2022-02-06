#include "custom/queueing/source/CSVUtil/CSVIterHelpers.h"

void fastForwardCSVIterUntilTime(CSVIterator& csvIter, double timeUntil, int timePos)
{
    while ((atof(((*csvIter)[timePos]).data()) < timeUntil) && csvIter != CSVIterator())
        ++csvIter;
}