
#include "clang/AST/Type.h"

#include "qual_type_wrapper.h"

namespace v8toolkit::class_wrapper {

QualTypeWrapper::QualTypeWrapper() = default;

QualType & QualTypeWrapper::get() const {
    return *this->qual_type;
}

QualTypeWrapper::~QualTypeWrapper() = default;

QualTypeWrapper::QualTypeWrapper(QualType const & qual_type) :
    qual_type(std::make_unique<QualType>(qual_type))
{}

QualTypeWrapper::QualTypeWrapper(QualTypeWrapper const & other) :
    qual_type(std::make_unique<QualType>(other.get())) {}

QualType const * QualTypeWrapper::operator->() const {
    return this->qual_type.get();
}

QualType * QualTypeWrapper::operator->() {
    return this->qual_type.get();
}

QualType const & QualTypeWrapper::operator*() const {
    return *this->qual_type;
}

QualType & QualTypeWrapper::operator*() {
    return *this->qual_type;
}


QualTypeWrapper & QualTypeWrapper::operator=(QualTypeWrapper const & other) {
    this->qual_type = std::make_unique<QualType>(*other);
    return *this;
}

QualTypeWrapper & QualTypeWrapper::operator=(QualType const & qual_type) {
    this->qual_type = std::make_unique<QualType>(qual_type);
    return *this;
}


} // end namespace v8toolkit::class_wrapper