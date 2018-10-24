#include "svgutils/svg_utils.h"
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
  NAME::NAME() : SVGAttribute(tagName, DEFAULT) {}                             \
  const char *NAME::tagName = STR;
#include "svgutils/svg_entities.def"
} // namespace svg

using namespace svg;

std::string SVGAttribute::getValue() const {
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
