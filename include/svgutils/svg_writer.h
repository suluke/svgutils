#ifndef SVGUTILS_SVG_UTILS_H
#define SVGUTILS_SVG_UTILS_H

#include "svgutils/utils.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
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
struct SVGAttribute {
  SVGAttribute(const SVGAttribute &) = default;
  SVGAttribute &operator=(const SVGAttribute &) = default;
  const char *getName() const { return name; }
  std::string getValue() const;
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

private:
  template <typename T> auto castToLegalType(T value) {
    if constexpr (std::is_pointer_v<T>)
      return static_cast<const char *>(value);
    else if constexpr (std::is_integral_v<T>)
      return static_cast<int64_t>(value);
    else if constexpr (std::is_floating_point_v<T>)
      return static_cast<double>(value);
  }

protected:
  template <typename T>
  SVGAttribute(const char *name, T value)
      : name(name), value(castToLegalType(value)) {}

private:
  template <typename DerivedT> friend class SVGWriterBase;

  const char *name;
  using value_t = std::variant<const char *, int64_t, double>;
  value_t value;
};

#define SVG_ATTR(NAME, STR, DEFAULT)                                           \
  struct NAME : public SVGAttribute {                                          \
    template <typename T> NAME(T value) : SVGAttribute(tagName, value) {}      \
    NAME();                                                                    \
                                                                               \
  private:                                                                     \
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
        static_cast<const NAME &>(attr));
#include "svg_entities.def"
    svg_unreachable("Encountered unknown (foreign) attribute");
  }
#define SVG_ATTR(NAME, STR, DEFAULT)                                           \
  RetTy visit_##NAME(const NAME &) {                                           \
    if constexpr (std::is_same_v<void, RetTy>)                                 \
      return;                                                                  \
    else                                                                       \
      return RetTy();                                                          \
  }
#include "svg_entities.def"
};

/// Base implementation of a writer for svg documents.
/// Allows overriding most member functions using CRTP.
template <typename DerivedTy> class SVGWriterBase {
public:
  SVGWriterBase(outstream_t &output) : outstream(&output) {}

#define SVG_TAG(NAME, STR, ...)                                                \
  template <typename... attrs_t> DerivedTy &NAME(attrs_t... attrs) {           \
    openTag(STR __VA_OPT__(, ) __VA_ARGS__, std::forward<attrs_t>(attrs)...);  \
    return *static_cast<DerivedTy *>(this);                                    \
  }                                                                            \
  template <typename container_t> DerivedTy &NAME(const container_t &attrs) {  \
    std::vector<SVGAttribute> attrsExt({__VA_ARGS__});                         \
    for (const SVGAttribute &Att : attrs)                                      \
      attrsExt.emplace_back(Att);                                              \
    static_cast<DerivedTy *>(this)->openTag(STR, attrsExt);                    \
    return *static_cast<DerivedTy *>(this);                                    \
  }
#include "svg_entities.def"

  DerivedTy &content(const char *text) {
    static_cast<DerivedTy *>(this)->closeTag();
    output() << text;
    return *static_cast<DerivedTy *>(this);
  }

  DerivedTy &enter() {
    assert(currentTag && "Cannot enter without root tag");
    parents.push(currentTag);
    currentTag = nullptr;
    return *static_cast<DerivedTy *>(this);
  }
  DerivedTy &leave() {
    assert(parents.size() && "Cannot leave: No parent tag");
    static_cast<DerivedTy *>(this)->closeTag();
    currentTag = parents.top();
    parents.pop();
    return *static_cast<DerivedTy *>(this);
  }
  DerivedTy &finish() {
    while (parents.size())
      static_cast<DerivedTy *>(this)->leave();
    static_cast<DerivedTy *>(this)->closeTag();
    return *static_cast<DerivedTy *>(this);
  }

private:
  using attrmap_t = std::map<const char *, SVGAttribute>;
  void uniqueAttrs(attrmap_t &attrMap) {}
  template <typename... attrs_t>
  void uniqueAttrs(attrmap_t &attrMap, const SVGAttribute &attr,
                   attrs_t... attrs) {
    attrMap.insert_or_assign(attr.getName(), attr);
    uniqueAttrs(attrMap, std::forward<attrs_t>(attrs)...);
  }

protected:
  template <typename container_t> void writeAttrs(const container_t &attrs) {
    for (const auto &attr : attrs)
      output() << " " << attr;
  }
  template <typename... attrs_t>
  void openTag(const char *tagname, attrs_t... attrs) {
    attrmap_t attrMap;
    uniqueAttrs(attrMap, std::forward<attrs_t>(attrs)...);
    std::vector<SVGAttribute> attrVec;
    for (const auto &KeyValuePair : attrMap)
      attrVec.emplace_back(KeyValuePair.second);
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

/// SVG document writer with several formating features.
struct SVGFormattedWriter : public SVGWriterBase<SVGFormattedWriter> {
  using self_t = SVGFormattedWriter;
  using base_t = SVGWriterBase<self_t>;

  SVGFormattedWriter(outstream_t &os) : base_t(os) {}
  SVGFormattedWriter(outstream_t &os, char indentChar, size_t indentWidth)
      : base_t(os), indentChar(indentChar), indentWidth(indentWidth) {}

  self_t &enter() {
    this->base_t::enter();
    this->base_t::output() << "\n";
    ++indent;
    wasEntered.top() = true;
    return *this;
  }
  self_t &leave() {
    this->base_t::leave();
    --indent;
    return *this;
  }
  self_t &content(const char *text) {
    closeTag();
    writeIndent();
    this->base_t::output() << text << "\n";
    return *this;
  }

private:
  static inline outstream_t &repeat(outstream_t &os, char c, size_t count) {
    for (size_t i = 0; i < count; ++i)
      os << c;
    return os;
  }
  void writeIndent() {
    repeat(this->base_t::output(), indentChar, indentWidth * indent);
  }

  friend base_t;
  template <typename container_t>
  void openTag(const char *tagname, container_t attrs) {
    closeTag();
    writeIndent();
    this->base_t::output() << "<" << tagname;
    this->base_t::writeAttrs(attrs);
    this->base_t::output() << ">";
    this->base_t::currentTag = tagname;
    wasEntered.push(false);
  }
  void closeTag() {
    if (this->base_t::currentTag) {
      if (wasEntered.top())
        writeIndent();
      this->base_t::output() << "</" << this->base_t::currentTag << ">";
      this->base_t::currentTag = nullptr;
      this->base_t::output() << "\n";
      wasEntered.pop();
    }
  }

  char indentChar = ' ';
  size_t indentWidth = 2;
  size_t indent = 0;
  std::stack<bool> wasEntered;
};

/// An interface describing the minimum feature set of svg document
/// writers.
struct WriterConcept {
  virtual ~WriterConcept() = default;

#define SVG_TAG(NAME, STR, ...)                                                \
  template <typename... attrs_t> void NAME(attrs_t... attrs) {                 \
    std::vector<SVGAttribute> attrsVec({std::forward<attrs_t>(attrs)...});     \
    NAME(attrsVec);                                                            \
  }                                                                            \
  virtual void NAME(const std::vector<SVGAttribute> &attrs) = 0;
#include "svg_entities.def"
  virtual void enter() = 0;
  virtual void leave() = 0;
  virtual void content(const char *) = 0;
  virtual void finish() = 0;
};

/// Helper class to convert from virtual dispatch to template member
/// functions.
template <typename WriterTy> struct WriterModel : public virtual WriterConcept {
  template <typename... args_t>
  WriterModel(args_t &&... args) : Writer(std::forward<args_t>(args)...) {}
#define SVG_TAG(NAME, STR, ...)                                                \
  void NAME(const std::vector<SVGAttribute> &attrs) override {                 \
    Writer.NAME(attrs);                                                        \
  }
#include "svg_entities.def"
  void enter() override { Writer.enter(); }
  void leave() override { Writer.leave(); }
  void content(const char *text) override { Writer.content(text); }
  void finish() override { Writer.finish(); }

protected:
  WriterTy Writer;
};

/// Base class for clients that want to extend the capabilities of
/// the svg document writers above without losing the flexibility of
/// deciding which exact writer to use.
template <typename DerivedT> struct ExtendableWriter {
  ExtendableWriter(std::unique_ptr<WriterConcept> Writer)
      : Writer(std::move(Writer)) {}
#define SVG_TAG(NAME, STR, ...)                                                \
  template <typename... attrs_t> DerivedT &NAME(attrs_t... attrs) {            \
    std::vector<SVGAttribute> attrsVec({std::forward<attrs_t>(attrs)...});     \
    Writer->NAME(attrsVec);                                                    \
    return *static_cast<DerivedT *>(this);                                     \
  }                                                                            \
  template <typename container_t> DerivedT &NAME(const container_t &attrs) {   \
    std::vector<SVGAttribute> attrsVec(attrs.begin(), attrs.end());            \
    Writer->NAME(attrsVec);                                                    \
    return *static_cast<DerivedT *>(this);                                     \
  }
#include "svg_entities.def"
  DerivedT &enter() {
    Writer->enter();
    return *static_cast<DerivedT *>(this);
  }
  DerivedT &leave() {
    Writer->leave();
    return *static_cast<DerivedT *>(this);
  }
  DerivedT &content(const char *text) {
    Writer->content(text);
    return *static_cast<DerivedT *>(this);
  }
  DerivedT &finish() {
    Writer->finish();
    return *static_cast<DerivedT *>(this);
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
