#pragma once

#include <memory>

#include "clang_fwd.h"


namespace v8toolkit::class_wrapper {


class QualTypeWrapper {
private:
    std::unique_ptr<QualType> qual_type;

public:
    QualTypeWrapper();
    QualTypeWrapper(QualType const & qual_type);

    ~QualTypeWrapper();

    QualTypeWrapper(QualTypeWrapper const & other);

    QualTypeWrapper(QualTypeWrapper &&) = default;

    QualTypeWrapper & operator=(QualTypeWrapper const & other);

    QualTypeWrapper & operator=(QualType const & qual_type);

    QualTypeWrapper & operator=(QualTypeWrapper &&) = delete;

    QualType * operator->();

    QualType & operator*();

    QualType const * operator->() const;

    QualType const & operator*() const;

    QualType & get() const;
};

} // end namespace v8toolkit::class_wrapper

using namespace v8toolkit::class_wrapper;
