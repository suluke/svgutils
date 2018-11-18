#include "svgutils/svg_writer.h"
namespace svg {
// Rationale for the NAME_name static member:
// We need a way to unique attributes. It's a nice service to client code
// and even allows us to specify default-attributes which can still be
// overridden by the client.
// We don't want to rely on dynamic dispatch for that. So the only other
// alternative we can think of would be enums. However, enums cannot be
// extended without modification of their declaration. The static string
// member approach allows client code to define their own attributes
// outside of svg_entities.def
#define SVG_ATTR(NAME, STR, DEFAULT)                                           \
  NAME &NAME::operator=(const SVGAttribute &attr) {                            \
    if (attr.name != tagName)                                                  \
      svg_unreachable("Tried casting svg attribute to " #NAME                  \
                      " which isn't one");                                     \
    this->attr.value = attr.value;                                             \
    return *this;                                                              \
  }                                                                            \
  const char *NAME::tagName = STR;
#include "svgutils/svg_entities.def"
} // namespace svg

using namespace svg;

std::string SVGAttribute::getValueStr() const {
  std::string s;
  std::visit(
      [&s](auto &&value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, const char *>)
          s = value;
        else
          s = std::to_string(value);
      },
      value);
  return s;
}
const char *SVGAttribute::cstrOrNull() const {
  const char *res;
  std::visit(
      [&res](auto &&value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, const char *>)
          res = value;
        else
          res = nullptr;
      },
      value);
  return res;
}
double SVGAttribute::toDouble() const {
  double res;
  std::visit(
      [&res](auto &&value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, const char *>)
          res = std::stod(value);
        else
          res = value;
      },
      value);
  return res;
}

const char *SVGAttribute::GetUniqueNameFor(const char *name) {
  // string_view performs a deep comparison whereas const char * is pointer
  // comparison
  std::string_view NameView = name;
#define SVG_ATTR(NAME, STR, DEFAULT)                                           \
  if (NameView == NAME::tagName)                                               \
    return NAME::tagName;
#include "svgutils/svg_entities.def"
  return name;
}

SVGAttribute SVGAttribute::Create(const char *name, const char *value) {
  return SVGAttribute(GetUniqueNameFor(name), value);
}
SVGAttribute SVGAttribute::Create(const char *name, int64_t value) {
  return SVGAttribute(GetUniqueNameFor(name), value);
}
SVGAttribute SVGAttribute::Create(const char *name, double value) {
  return SVGAttribute(GetUniqueNameFor(name), value);
}
