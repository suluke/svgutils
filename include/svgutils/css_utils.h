#ifndef SVGUTILS_CSS_UTILS_H
#define SVGUTILS_CSS_UTILS_H

#include <cassert>
#include <list>
#include <map>
#include <string_view>
#include <vector>

namespace svg {
struct SVGAttribute;

struct CSSColor {
  double r = 0.;
  double g = 0.;
  double b = 0.;
  double a = 1.;

  double &operator[](size_t idx) {
    assert(idx >= 0 && idx < 4 &&
           "Tried accessing out-of-range color component");
    return idx == 0 ? r : (idx == 1 ? g : (idx == 2 ? b : a));
  }
  const double &operator[](size_t idx) const {
    assert(idx >= 0 && idx < 4 &&
           "Tried accessing out-of-range color component");
    return idx == 0 ? r : (idx == 1 ? g : (idx == 2 ? b : a));
  }
  void clamp();
  CSSColor hsl2rgb() const;
  CSSColor rgb2hsl() const;

  static CSSColor parseColor(std::string_view str);
};

struct CSSRule {
  const char *property;
  std::string value;
};

class StyleTracker {
public:
  StyleTracker();
  ~StyleTracker();
  using AttrContainer = std::vector<SVGAttribute>;
  void push(const AttrContainer &attrs);
  void pop();
  CSSColor getFillColor() const;

  struct StyleDiff;
  enum class Style;

private:
  std::list<StyleDiff> Cascade; // std::list does not relocate
  std::map<Style, std::string_view> CurrentStyle;
};
} // namespace svg
#endif // SVGUTILS_CSS_UTILS_H
