#ifndef SVGUTILS_SVG_UTILS_H
#define SVGUTILS_SVG_UTILS_H

#include "svgutils/utils.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stack>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace svg {
/// If your project implements its own streams (e.g. raw_ostream in LLVM)
/// just change this typedef to make use of your stream implementation.
using outstream_t = std::ostream;

/// Base class of all SVG attributes.
struct SVGAttribute final {
  SVGAttribute(const SVGAttribute &) = default;
  SVGAttribute &operator=(const SVGAttribute &) = default;
  const char *getName() const { return name; }
  std::string getValueStr() const;
  const char *cstrOrNull() const;
  double toDouble() const;
  template <typename T> void setValue(T value) { this->value = value; }
  inline friend outstream_t &operator<<(outstream_t &os,
                                        const SVGAttribute &attr) {
    os << attr.name << "=\"";
    attr.writeValue(os);
    os << "\"";
    return os;
  }
  void writeValue(outstream_t &os) const {
    std::visit([&os](auto &&value) { os << value; }, value);
  }

  static SVGAttribute Create(const char *name, const char *value);
  static SVGAttribute Create(const char *name, int64_t value);
  static SVGAttribute Create(const char *name, double value);

private:
  template <typename T> auto castToLegalType(T value) {
    if constexpr (std::is_pointer_v<T>)
      return static_cast<const char *>(value);
    else if constexpr (std::is_integral_v<T>)
      return static_cast<int64_t>(value);
    else if constexpr (std::is_floating_point_v<T>)
      return static_cast<double>(value);
  }

  template <typename T>
  SVGAttribute(const char *name, T value)
      : name(name), value(castToLegalType(value)) {}

  static const char *GetUniqueNameFor(const char *name);

  template <typename DerivedT> friend class SVGWriterBase;
#define SVG_ATTR(NAME, STR, DEFAULT) friend struct NAME;
#include "svg_entities.def"

  const char *name;
  using value_t = std::variant<const char *, int64_t, double>;
  value_t value;
};

#define SVG_ATTR(NAME, STR, DEFAULT)                                           \
  struct NAME {                                                                \
    template <typename T> NAME(T value) : attr(tagName, value) {}              \
    NAME() : attr(tagName, DEFAULT) {}                                         \
    explicit NAME(const SVGAttribute &attr) : NAME() { *this = attr; }         \
    NAME &operator=(const SVGAttribute &attr);                                 \
    operator SVGAttribute() const { return attr; }                             \
    const char *getName() const { return tagName; }                            \
    std::string getValueStr() const { return attr.getValueStr(); }             \
    const char *cstrOrNull() const { return attr.cstrOrNull(); }               \
                                                                               \
  private:                                                                     \
    friend class SVGAttribute;                                                 \
    SVGAttribute attr;                                                         \
    template <typename DerivedTy, typename RetTy>                              \
    friend class SVGAttributeVisitor;                                          \
    static const char *tagName;                                                \
  };
#include "svg_entities.def"

template <typename DerivedTy, typename RetTy = void>
struct SVGAttributeVisitor {
  RetTy visit(const SVGAttribute &attr) {
#define SVG_ATTR(NAME, STR, DEFAULT)                                           \
  if (attr.getName() == NAME::tagName)                                         \
    return static_cast<DerivedTy *>(this)->visit_##NAME(                       \
        static_cast<const NAME>(attr));
#include "svg_entities.def"
    return static_cast<DerivedTy *>(this)->visit_custom_attr(attr);
  }
#define SVG_ATTR(NAME, STR, DEFAULT)                                           \
  RetTy visit_##NAME(const NAME &) {                                           \
    if constexpr (std::is_same_v<void, RetTy>)                                 \
      return;                                                                  \
    else                                                                       \
      return RetTy();                                                          \
  }
#include "svg_entities.def"
  RetTy visit_custom_attr(const SVGAttribute &) {
    if constexpr (std::is_same_v<void, RetTy>)
      return;
    else
      return RetTy();
  }
};

struct SVGWriterError {
  explicit SVGWriterError(const std::string &msg) : msg(msg) {}
  SVGWriterError(const SVGWriterError &) = default;
  SVGWriterError &operator=(const SVGWriterError &) = default;
  SVGWriterError(SVGWriterError &&) = default;
  SVGWriterError &operator=(SVGWriterError &&) = default;

  const std::string &what() const { return msg; }

private:
  std::string msg;
};

template <typename T> struct SVGWriterErrorOr {
  SVGWriterErrorOr(T val) : val(val) {}
  SVGWriterErrorOr(const SVGWriterError &err) : val(err) {}

  SVGWriterErrorOr(const SVGWriterErrorOr &) = default;
  SVGWriterErrorOr &operator=(const SVGWriterErrorOr &) = default;
  SVGWriterErrorOr(SVGWriterErrorOr &&) = default;
  SVGWriterErrorOr &operator=(SVGWriterErrorOr &&) = default;

  /// Returns true if an error occurred
  operator bool() const { return std::holds_alternative<SVGWriterError>(val); }
  const T operator->() const {
    assert(!*this && "Unchecked value extraction from SVGWriterErrorOr<>");
    return std::get<T>(val);
  }
  const SVGWriterError &to_error() const {
    assert(
        *this &&
        "Trying to extract error from SVGWriterErrorOr<> in non-error state");
    return std::get<SVGWriterError>(val);
  }
  template <typename U> SVGWriterErrorOr<U> with_value(U val) const {
    if (*this)
      return SVGWriterErrorOr<U>(this->to_error());
    return SVGWriterErrorOr<U>(val);
  }
  SVGWriterErrorOr<void> without_value() const;

private:
  std::variant<SVGWriterError, T> val;
};

template <> struct SVGWriterErrorOr<void> {
  SVGWriterErrorOr() = default;
  SVGWriterErrorOr(const SVGWriterError &err) : err(err) {}

  SVGWriterErrorOr(const SVGWriterErrorOr &) = default;
  SVGWriterErrorOr &operator=(const SVGWriterErrorOr &) = default;
  SVGWriterErrorOr(SVGWriterErrorOr &&) = default;
  SVGWriterErrorOr &operator=(SVGWriterErrorOr &&) = default;

  /// Returns true if an error occurred
  operator bool() const { return !!err; }
  const SVGWriterError &to_error() const {
    assert(
        *this &&
        "Trying to extract error from SVGWriterErrorOr<> in non-error state");
    return *err;
  }
  template <typename U> SVGWriterErrorOr<U> with_value(U val) const {
    if (*this)
      return SVGWriterErrorOr<U>(this->to_error());
    return SVGWriterErrorOr<U>(val);
  }

private:
  std::optional<SVGWriterError> err;
};

template <typename T>
SVGWriterErrorOr<void> SVGWriterErrorOr<T>::without_value() const {
  if (*this)
    return SVGWriterErrorOr<void>(this->to_error());
  return SVGWriterErrorOr<void>{};
}

/// Base implementation of a writer for svg documents.
/// Allows overriding most member functions using CRTP.
template <typename DerivedTy> class SVGWriterBase {
public:
  SVGWriterBase(outstream_t &output) : outstream(&output) {}

  using RetTy = SVGWriterErrorOr<DerivedTy *>;
#define SVG_TAG(NAME, STR)                                                     \
  template <typename... attrs_t> RetTy NAME(attrs_t... attrs) {                \
    openTag(STR, std::forward<attrs_t>(attrs)...);                             \
    return static_cast<DerivedTy *>(this);                                     \
  }                                                                            \
  template <typename container_t> RetTy NAME(const container_t &attrs) {       \
    std::vector<SVGAttribute> attrsVec(attrs.begin(), attrs.end());            \
    static_cast<DerivedTy *>(this)->openTag(STR, attrsVec);                    \
    return static_cast<DerivedTy *>(this);                                     \
  }                                                                            \
  RetTy NAME(const std::vector<SVGAttribute> &attrs) {                         \
    static_cast<DerivedTy *>(this)->openTag(STR, attrs);                       \
    return static_cast<DerivedTy *>(this);                                     \
  }
#include "svg_entities.def"

  template <typename... attrs_t>
  RetTy custom_tag(const char *name, attrs_t... attrs) {
    openTag(name, std::forward<attrs_t>(attrs)...);
    return static_cast<DerivedTy *>(this);
  }
  template <typename container_t>
  RetTy custom_tag(const char *name, const container_t &attrs) {
    std::vector<SVGAttribute> attrsVec(attrs.begin(), attrs.end());
    static_cast<DerivedTy *>(this)->openTag(name, attrsVec);
    return static_cast<DerivedTy *>(this);
  }
  RetTy custom_tag(const char *name, const std::vector<SVGAttribute> &attrs) {
    static_cast<DerivedTy *>(this)->openTag(name, attrs);
    return static_cast<DerivedTy *>(this);
  }

  RetTy content(const char *text) {
    static_cast<DerivedTy *>(this)->closeTag();
    output() << text;
    return static_cast<DerivedTy *>(this);
  }
  RetTy comment(const char *comment) {
    static_cast<DerivedTy *>(this)->closeTag();
    output() << "<!-- " << comment << " -->";
    return static_cast<DerivedTy *>(this);
  }

  RetTy enter() {
    assert(currentTag && "Cannot enter without root tag");
    parents.push(currentTag);
    currentTag = nullptr;
    return static_cast<DerivedTy *>(this);
  }
  RetTy leave() {
    assert(parents.size() && "Cannot leave: No parent tag");
    static_cast<DerivedTy *>(this)->closeTag();
    currentTag = parents.top();
    parents.pop();
    return static_cast<DerivedTy *>(this);
  }
  RetTy finish() {
    while (parents.size())
      static_cast<DerivedTy *>(this)->leave();
    static_cast<DerivedTy *>(this)->closeTag();
    return static_cast<DerivedTy *>(this);
  }

protected:
  template <typename container_t> void writeAttrs(const container_t &attrs) {
    std::set<const char *> keys;
    for (const auto &attr : attrs) {
      assert(!keys.count(attr.getName()) && "Duplicate attribute key");
      output() << " " << attr;
      keys.insert(attr.getName());
    }
  }
  template <typename... attrs_t>
  void openTag(const char *tagname, attrs_t... attrs) {
    std::vector<SVGAttribute> attrVec({std::forward<attrs_t>(attrs)...});
    static_cast<DerivedTy *>(this)->openTag(tagname, attrVec);
  }

  template <typename container_t>
  void openTag(const char *tagname, const container_t &attrs) {
    static_cast<DerivedTy *>(this)->closeTag();
    output() << "<" << tagname;
    static_cast<DerivedTy *>(this)->writeAttrs(attrs);
    output() << ">";
    currentTag = tagname;
  }
  void closeTag() {
    if (currentTag) {
      output() << "</" << currentTag << ">";
      currentTag = nullptr;
    }
  }
  std::stack<const char *> parents;
  const char *currentTag = nullptr;
  outstream_t *outstream;
  outstream_t &output() const {
    assert(outstream && "No output stream set up");
    return *outstream;
  }
};

/// Simple instantiation of the SVGWriterBase template. Implements neither
/// formatting nor any other smart features.
struct SVGWriter : public SVGWriterBase<SVGWriter> {
  SVGWriter(outstream_t &os) : SVGWriterBase<SVGWriter>(os) {}
};

/// An interface describing the minimum feature set of svg document
/// writers.
struct WriterConcept {
  virtual ~WriterConcept() = default;
  using RetTy = SVGWriterErrorOr<void>;

#define SVG_TAG(NAME, STR, ...)                                                \
  template <typename... attrs_t> void NAME(attrs_t... attrs) {                 \
    std::vector<SVGAttribute> attrsVec({std::forward<attrs_t>(attrs)...});     \
    NAME(attrsVec);                                                            \
  }                                                                            \
  virtual RetTy NAME(const std::vector<SVGAttribute> &attrs) = 0;
#include "svg_entities.def"
  virtual RetTy custom_tag(const char *tag,
                           const std::vector<SVGAttribute> &attrs) = 0;
  virtual RetTy enter() = 0;
  virtual RetTy leave() = 0;
  virtual RetTy content(const char *) = 0;
  virtual RetTy comment(const char *) = 0;
  virtual RetTy finish() = 0;
};

/// Helper class to convert from virtual dispatch to template member
/// functions.
template <typename WriterTy> struct WriterModel : public virtual WriterConcept {
  using RetTy = WriterConcept::RetTy;

  template <typename... args_t>
  WriterModel(args_t &&... args) : Writer(std::forward<args_t>(args)...) {}
#define SVG_TAG(NAME, STR, ...)                                                \
  RetTy NAME(const std::vector<SVGAttribute> &attrs) override {                \
    return Writer.NAME(attrs).without_value();                                 \
  }
#include "svg_entities.def"
  RetTy custom_tag(const char *tag,
                   const std::vector<SVGAttribute> &attrs) override {
    return Writer.custom_tag(tag, attrs).without_value();
  }
  RetTy enter() override { return Writer.enter().without_value(); }
  RetTy leave() override { return Writer.leave().without_value(); }
  RetTy content(const char *text) override {
    return Writer.content(text).without_value();
  }
  RetTy comment(const char *comment) override {
    return Writer.comment(comment).without_value();
  }
  RetTy finish() override { return Writer.finish().without_value(); }
  WriterTy &getWriter() { return Writer; }

protected:
  WriterTy Writer;
};

/// Base class for clients that want to extend the capabilities of
/// the svg document writers above without losing the flexibility of
/// deciding which exact writer to use.
template <typename DerivedTy> struct ExtendableWriter {
  ExtendableWriter(std::unique_ptr<WriterConcept> Writer)
      : Writer(std::move(Writer)) {}

  using RetTy = SVGWriterErrorOr<DerivedTy *>;
#define SVG_TAG(NAME, STR, ...)                                                \
  template <typename... attrs_t> RetTy NAME(attrs_t... attrs) {                \
    std::vector<SVGAttribute> attrsVec({std::forward<attrs_t>(attrs)...});     \
    return Writer->NAME(attrsVec).with_value(static_cast<DerivedTy *>(this));  \
  }                                                                            \
  template <typename container_t> RetTy NAME(const container_t &attrs) {       \
    std::vector<SVGAttribute> attrsVec(attrs.begin(), attrs.end());            \
    return Writer->NAME(attrsVec).with_value(static_cast<DerivedTy *>(this));  \
  }                                                                            \
  RetTy NAME(const std::vector<SVGAttribute> &attrs) {                         \
    return Writer->NAME(attrs).with_value(static_cast<DerivedTy *>(this));     \
  }
#include "svg_entities.def"
  template <typename... attrs_t>
  RetTy custom_tag(const char *name, attrs_t... attrs) {
    std::vector<SVGAttribute> attrsVec({std::forward<attrs_t>(attrs)...});
    return Writer->custom_tag(name, attrsVec)
        .with_value(static_cast<DerivedTy *>(this));
  }
  template <typename container_t>
  RetTy custom_tag(const char *name, const container_t &attrs) {
    std::vector<SVGAttribute> attrsVec(attrs.begin(), attrs.end());
    return Writer->custom_tag(name, attrsVec)
        .with_value(static_cast<DerivedTy *>(this));
  }
  RetTy custom_tag(const char *name, const std::vector<SVGAttribute> &attrs) {
    return Writer->custom_tag(name, attrs)
        .with_value(static_cast<DerivedTy *>(this));
  }
  RetTy enter() {
    return Writer->enter().with_value(static_cast<DerivedTy *>(this));
  }
  RetTy leave() {
    return Writer->leave().with_value(static_cast<DerivedTy *>(this));
  }
  RetTy content(const char *text) {
    return Writer->content(text).with_value(static_cast<DerivedTy *>(this));
  }
  RetTy comment(const char *text) {
    return Writer->comment(text).with_value(static_cast<DerivedTy *>(this));
  }
  RetTy finish() {
    return Writer->finish().with_value(static_cast<DerivedTy *>(this));
  }
  template <typename NewWriterT> NewWriterT &continueAs(NewWriterT &NewWriter) {
    static_cast<ExtendableWriter<NewWriterT> &>(NewWriter).Writer =
        std::move(this->Writer);
    return NewWriter;
  }

  // FIXME not sure if this is a gcc bug
  //~ protected:
  //~ template<typename DTy;
  //~ friend struct ExtendableWriter<DTy>;
  std::unique_ptr<WriterConcept> Writer;
};
} // namespace svg
#endif // SVGUTILS_SVG_UTILS_H
