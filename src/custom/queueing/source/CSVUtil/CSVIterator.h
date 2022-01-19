#pragma once

#include "custom/queueing/source/CSVUtil/CSVRow.h"


class CSVIterator
{   
    public:
        typedef std::input_iterator_tag     iterator_category;
        typedef CSVRow                      value_type;
        typedef std::size_t                 difference_type;
        typedef CSVRow*                     pointer;
        typedef CSVRow&                     reference;

        CSVIterator(std::istream& str, char csv_sep);
        CSVIterator();

        // Pre Increment
        CSVIterator& operator++();
        // Post increment
        CSVIterator operator++(int);
        CSVRow const& operator*()   const;
        CSVRow const* operator->()  const;

        bool operator==(CSVIterator const& rhs);
        bool operator!=(CSVIterator const& rhs);
    private:
        std::istream*       m_str;
        CSVRow              m_row;
};
