#ifndef SVGUTILS_CSS_UTILS_H
#define SVGUTILS_CSS_UTILS_H

#include <cassert>
#include <iostream>
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

  friend inline std::ostream &operator<<(std::ostream &os,
                                         const CSSColor &col) {
    os << '#';
    for (unsigned i = 0; i < 4; ++i) {
      unsigned val8 = static_cast<unsigned>(col[i] * 255);
      char upper = val8 >> 4;
      char lower = val8 & 0xf;
      char c1 = (upper < 10 ? '0' : 'a' - 10) + upper;
      char c2 = (lower < 10 ? '0' : 'a' - 10) + lower;
      os << c1 << c2;
    }
    return os;
  }

  static CSSColor parse(std::string_view str);
};

struct CSSUnit {
  enum Unit { PERCENT, PX, PT, PC, MM, CM, IN } unit = PX;
  double length = 0;
  static CSSUnit parse(std::string_view str);

  friend inline std::ostream &operator<<(std::ostream &os,
                                         const CSSUnit &unit) {
    os << unit.length;
    switch (unit.unit) {
    case PERCENT:
      os << "%";
      break;
    case PX:
      os << "px";
      break;
    case PT:
      os << "pt";
      break;
    case PC:
      os << "pc";
      break;
    case MM:
      os << "mm";
      break;
    case CM:
      os << "cm";
      break;
    case IN:
      os << "in";
      break;
    }
    return os;
  }
};

struct CSSRule {
  const char *property;
  std::string value;
};

struct CSSDashArray {
  std::vector<CSSUnit> dashes;
  static CSSDashArray parse(std::string_view str);
};

class StyleTracker {
public:
  StyleTracker();
  StyleTracker(StyleTracker &&);
  StyleTracker &operator=(StyleTracker &&);
  ~StyleTracker();
  using AttrContainer = std::vector<SVGAttribute>;
  void push(const AttrContainer &attrs);
  void pop();
  CSSColor getFillColor() const;
  CSSColor getStrokeColor() const;
  CSSUnit getStrokeWidth() const;
  CSSDashArray getStrokeDasharray() const;

  struct StyleDiff;
  enum class Style;

private:
  std::list<StyleDiff> Cascade; // std::list does not relocate
  std::map<Style, std::string_view> CurrentStyle;
};
} // namespace svg
#endif // SVGUTILS_CSS_UTILS_H
