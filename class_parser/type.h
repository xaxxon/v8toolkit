
#pragma once

#include <functional>
#include <set>
#include <string>
#include <unordered_map>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"
#include <clang/AST/Type.h>
#pragma clang diagnostic pop

namespace v8toolkit::class_parser {

struct WrappedClass;

// represents qualified type
struct TypeInfo {

private:
  static std::string
  convert_simple_typename_to_jsdoc(std::string simple_type_name,
                                   std::string const & = "");

  // Mapping between external template names and default types, such as:
  // template<class T=int> T foo();
  // for the return value type T, this tells it it has a default type of int
  std::unordered_map<std::string, QualType> template_parameter_types;

  // the type cannot be gotten because after template substitution there may not
  // be an actual
  //   Type object for the resulting type.  It is only available as a string.
  //   However, the "plain type" is guaranteed to exist as a Type object

public:
  QualType const type;

  explicit TypeInfo(QualType const &type,
                    std::unordered_map<std::string, QualType> const
                        &template_parameter_types = {});

  ~TypeInfo();

  CXXRecordDecl const *get_plain_type_decl() const;

  TypeInfo get_plain_type() const;

  /// name of actual type
  std::string get_name() const;

  /// name of type without reference or pointers, but keeps "core" constness
  std::string get_plain_name() const;

  /// corresponding javascript type
  std::string get_jsdoc_type_name(std::string const & = "") const;

  // return if the type (or the type being pointed/referred to) is const (not is
  // the pointer const) double * const => false double const * => true
  bool is_const() const;

  /**
   * returns non-const version of this type (may be the same if not const to
   * begin with)
   * @return non-const version of this type
   */
  TypeInfo without_const() const;

  bool is_templated() const;

  void for_each_templated_type(std::function<void(QualType const &)>) const;

  bool is_void() const;

  // returns the root incldues for this type (including any template type
  // parameters it may have)
  std::set<std::string> get_root_includes() const;

  operator QualType() { return this->type; }
  
  WrappedClass * get_wrapped_class() const;
  
};

} // end namespace v8toolkit::class_parser