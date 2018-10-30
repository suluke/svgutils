#include "svgutils/css_utils.h"
#include "svgutils/svg_utils.h"

#include <limits>

using namespace svg;

static const char *mapColorNameToValue(std::string_view name) {
#define CSS_COLOR(NAME, VALUE)                                                 \
  if (name == NAME)                                                            \
    return VALUE;
#include "svgutils/css_colors.def"
  return "#000000";
}

static size_t cvtHexChars(char higher, char lower) {
  size_t result;
  if ('0' <= higher && higher <= '9')
    result = ((size_t)(higher - '0')) << 4;
  else if ('A' <= higher && higher <= 'Z')
    result = ((size_t)(higher - 'A' + 10)) << 4;
  else if ('a' <= higher && higher <= 'z')
    result = ((size_t)(higher - 'a' + 10)) << 4;
  else
    unreachable("Higher hex character not in valid range");

  if ('0' <= lower && lower <= '9')
    result += lower - '0';
  else if ('A' <= lower && lower <= 'Z')
    result += lower - 'A' + 10;
  else if ('a' <= lower && lower <= 'z')
    result += lower - 'a' + 10;
  else
    unreachable("Lower hex character not in valid range");
  return result;
}

static CSSColor parseHexColor(std::string_view hex) {
  assert(hex.front() == '#' &&
         "Expected hex color but first character is not #");
  CSSColor result;
  if (hex.size() == 4) {
    result.r = cvtHexChars(hex[1], hex[1]) / 255.;
    result.g = cvtHexChars(hex[2], hex[2]) / 255.;
    result.b = cvtHexChars(hex[3], hex[3]) / 255.;
  } else if (hex.size() == 5) {
    result.r = cvtHexChars(hex[1], hex[1]) / 255.;
    result.g = cvtHexChars(hex[2], hex[2]) / 255.;
    result.b = cvtHexChars(hex[3], hex[3]) / 255.;
    result.a = cvtHexChars(hex[4], hex[4]) / 255.;
  } else if (hex.size() == 7) {
    result.r = cvtHexChars(hex[1], hex[2]) / 255.;
    result.g = cvtHexChars(hex[3], hex[4]) / 255.;
    result.b = cvtHexChars(hex[5], hex[6]) / 255.;
  } else if (hex.size() == 9) {
    result.r = cvtHexChars(hex[1], hex[2]) / 255.;
    result.g = cvtHexChars(hex[3], hex[4]) / 255.;
    result.b = cvtHexChars(hex[5], hex[6]) / 255.;
    result.a = cvtHexChars(hex[7], hex[8]) / 255.;
  }
  return result;
}

static CSSColor parseColorFromTuple(std::string_view tuple) {
  CSSColor result;
  assert(tuple.front() == '(' && tuple.back() == ')' &&
         "Color tuple should be enclosed in parens '()'");
  int component = 0;
  do {
    // find the start of the component value.
    // NOTE: We don't look fo '+' because that isn't allowed by from_chars
    auto begin = tuple.find_first_of(".0123456789", 1);
    if (begin == std::string_view::npos)
      break;
    // find its end
    auto end = tuple.find_first_of(" ,)", begin);
    std::string_view val = tuple.substr(begin, end - begin);
    // check if this is a percentage
    bool percentage = val.back() == '%';
    if (percentage)
      val = val.substr(0, val.size() - 1);
    // parse the component value from val
    auto c = strview_to_double(val);
    if (c) {
      if (percentage)
        *c /= 100.;
      result[component] = *c;
    } else {
      // exit early on error
      return CSSColor();
    }
    tuple = tuple.substr(end + 1);
    ++component;
  } while (tuple.size() && component < 4);
  return result;
}

void CSSColor::clamp() {
  r = std::min(1., std::max(0., r));
  g = std::min(1., std::max(0., g));
  b = std::min(1., std::max(0., b));
  a = std::min(1., std::max(0., a));
}

// Adapted from https://stackoverflow.com/a/49433742/1468532
CSSColor CSSColor::hsl2rgb() const {
  CSSColor rgb;
  rgb.a = a;
  if (g < std::numeric_limits<double>::min())
    rgb.r = rgb.g = rgb.b = b;
  else if (b < std::numeric_limits<double>::min())
    rgb.r = rgb.g = rgb.b = 0.;
  else {
    const double q = b < .5 ? b * (1. + g) : b + g - b * g;
    const double p = 2. * b - q;
    double t[] = {r + 2., r, r - 2.};

    for (int i = 0; i < 3; ++i) {
      if (t[i] < 0.)
        t[i] += 6.;
      else if (t[i] > 6.)
        t[i] -= 6.;

      if (t[i] < 1.)
        rgb[i] = p + (q - p) * t[i];
      else if (t[i] < 3.)
        rgb[i] = q;
      else if (t[i] < 4.)
        rgb[i] = p + (q - p) * (4. - t[i]);
      else
        rgb[i] = p;
    }
  }
  return rgb;
}
CSSColor CSSColor::rgb2hsl() const {
  CSSColor hsl;
  hsl.a = a;
  const double maxRGB = std::max(r, std::max(g, b));
  const double minRGB = std::min(r, std::min(g, b));
  const double delta2 = maxRGB + minRGB;
  hsl.b = delta2 * .5;

  const double delta = maxRGB - minRGB;
  if (delta < std::numeric_limits<double>::min())
    hsl.r = hsl.g = 0.;
  else {
    hsl.g = delta / (hsl.b > .5 ? 2. - delta2 : delta2);
    if (r >= maxRGB) {
      hsl.r = (g - b) / delta;
      if (hsl.r < 0.)
        hsl.r += 6.;
    } else if (g >= maxRGB)
      hsl.r = 2. + (b - r) / delta;
    else
      hsl.r = 4. + (r - g) / delta;
  }
  return hsl;
}

CSSColor CSSColor::parse(std::string_view str) {
  if (str.front() == '#')
    return parseHexColor(str);
  const auto front4 = str.substr(0, 4);
  if (front4 == "rgb(")
    return parseColorFromTuple(str.substr(3));
  if (front4 == "hsl(")
    return parseColorFromTuple(str.substr(3)).hsl2rgb();
  const auto front5 = str.substr(0, 5);
  if (front5 == "rgba(")
    return parseColorFromTuple(str.substr(4));
  if (front5 == "hsla(")
    return parseColorFromTuple(str.substr(4)).hsl2rgb();
  return parse(mapColorNameToValue(str));
}

CSSUnit CSSUnit::parse(std::string_view str) {
  str = strview_trim(str);
  CSSUnit result;
  if (str.back() == '%') {
    result.unit = CSSUnit::PERCENT;
    str = str.substr(0, str.size() - 1);
  } else if (str.size() >= 2) {
    std::string_view back2 = str.substr(str.size() - 2);
    if (back2 == "px" || back2 == "pt" || back2 == "pc" || back2 == "mm" ||
        back2 == "cm" || back2 == "in") {
      if (back2 == "px")
        result.unit = CSSUnit::PX;
      else if (back2 == "pt")
        result.unit = CSSUnit::PT;
      else if (back2 == "pc")
        result.unit = CSSUnit::PC;
      else if (back2 == "mm")
        result.unit = CSSUnit::MM;
      else if (back2 == "cm")
        result.unit = CSSUnit::CM;
      else if (back2 == "in")
        result.unit = CSSUnit::IN;
      str = str.substr(0, str.size() - 2);
    }
  }
  auto valopt = strview_to_double(str);
  if (valopt)
    result.length = *valopt;
  return result;
}

CSSDashArray CSSDashArray::parse(std::string_view str) {
  CSSDashArray result;
  str = strview_trim(str);
  if (str == "none")
    return result;
  std::vector<std::string_view> splits;
  strview_split(str, " ,", splits);
  for (const auto &split : splits)
    result.dashes.emplace_back(CSSUnit::parse(split));
  return result;
}

enum class StyleTracker::Style {
  COLOR,
  BACKGROUND_COLOR,
  FONT_FAMILY,
  FONT_SIZE,
  FONT_WEIGHT,
  FILL,
  STROKE,
  STROKE_WIDTH,
  STROKE_DASHARRAY,
  TRANSFORM,
  TEXT_ANCHOR,
  OPACITY
};

struct StyleTracker::StyleDiff {
  std::map<Style, std::string> styles;
  inline void extend(StyleDiff &&diff) {
    styles.swap(diff.styles);
    styles.merge(diff.styles);
  }
  inline void extend(const StyleDiff &diff) {
    for (const auto &KeyValuePair : diff.styles)
      styles.insert_or_assign(KeyValuePair.first, KeyValuePair.second);
  }
};

namespace {
struct StyleParser
    : public SVGAttributeVisitor<StyleParser, StyleTracker::StyleDiff> {
  using StyleDiff = StyleTracker::StyleDiff;
  static StyleDiff parseStyles(const StyleTracker::AttrContainer &attrs);
  StyleDiff visit_color(const svg::color &attr);
  StyleDiff visit_font_family(const svg::font_family &attr);
  StyleDiff visit_font_size(const svg::font_size &attr);
  StyleDiff visit_fill(const svg::fill &attr);
  StyleDiff visit_stroke(const svg::stroke &attr);
  StyleDiff visit_stroke_width(const svg::stroke_width &attr);
  StyleDiff visit_stroke_dasharray(const svg::stroke_dasharray &attr);
  StyleDiff visit_text_anchor(const svg::text_anchor &attr);
};

StyleTracker::StyleDiff StyleParser::visit_color(const svg::color &attr) {
  StyleDiff diff;
  diff.styles[StyleTracker::Style::COLOR] = attr.getValue();
  return diff;
}
StyleTracker::StyleDiff
StyleParser::visit_font_family(const svg::font_family &attr) {
  StyleDiff diff;
  diff.styles[StyleTracker::Style::FONT_FAMILY] = attr.getValue();
  return diff;
}
StyleTracker::StyleDiff
StyleParser::visit_font_size(const svg::font_size &attr) {
  StyleDiff diff;
  diff.styles[StyleTracker::Style::FONT_SIZE] = attr.getValue();
  return diff;
}
StyleTracker::StyleDiff StyleParser::visit_fill(const svg::fill &attr) {
  StyleDiff diff;
  diff.styles[StyleTracker::Style::FILL] = attr.getValue();
  return diff;
}
StyleTracker::StyleDiff StyleParser::visit_stroke(const svg::stroke &attr) {
  StyleDiff diff;
  diff.styles[StyleTracker::Style::STROKE] = attr.getValue();
  return diff;
}
StyleTracker::StyleDiff
StyleParser::visit_stroke_width(const svg::stroke_width &attr) {
  StyleDiff diff;
  diff.styles[StyleTracker::Style::STROKE_WIDTH] = attr.getValue();
  return diff;
}
StyleTracker::StyleDiff
StyleParser::visit_stroke_dasharray(const svg::stroke_dasharray &attr) {
  StyleDiff diff;
  diff.styles[StyleTracker::Style::STROKE_DASHARRAY] = attr.getValue();
  return diff;
}
StyleTracker::StyleDiff
StyleParser::visit_text_anchor(const svg::text_anchor &attr) {
  StyleDiff diff;
  diff.styles[StyleTracker::Style::TEXT_ANCHOR] = attr.getValue();
  return diff;
}

StyleTracker::StyleDiff
StyleParser::parseStyles(const StyleTracker::AttrContainer &attrs) {
  StyleTracker::StyleDiff result;
  StyleParser parser;
  for (const SVGAttribute &Attr : attrs)
    result.extend(parser.visit(Attr));
  return result;
}
} // namespace

StyleTracker::StyleTracker() {
  StyleDiff initialStyles;
  initialStyles.styles = {
      {Style::COLOR, "black"},
      {Style::BACKGROUND_COLOR, "white"},
      {Style::FILL, "transparent"},
      {Style::FONT_FAMILY, "serif"},
      {Style::FONT_SIZE, "12px"},
      {Style::FONT_WEIGHT, "normal"},
      {Style::OPACITY, "1"},
      {Style::STROKE, "black"},
      {Style::STROKE_DASHARRAY, "none"},
      {Style::STROKE_WIDTH, "1px"},
      {Style::TEXT_ANCHOR, "start"},
      {Style::TRANSFORM, ""},
  };
  Cascade.emplace_back(std::move(initialStyles));
  for (const auto &KeyValuePair : Cascade.back().styles)
    CurrentStyle[KeyValuePair.first] = KeyValuePair.second;
}
StyleTracker::StyleTracker(StyleTracker &&o) : StyleTracker() {
  *this = std::move(o);
}
StyleTracker &StyleTracker::operator=(StyleTracker &&o) {
  Cascade = std::move(o.Cascade);
  CurrentStyle.clear();
  for (const StyleDiff &Diff : Cascade)
    for (const auto &KeyValuePair : Diff.styles)
      CurrentStyle[KeyValuePair.first] = KeyValuePair.second;
  return *this;
}
StyleTracker::~StyleTracker() = default;

void StyleTracker::push(const AttrContainer &attrs) {
  Cascade.emplace_back(StyleParser::parseStyles(attrs));
  for (const auto &KeyValuePair : Cascade.back().styles)
    CurrentStyle[KeyValuePair.first] = KeyValuePair.second;
}

void StyleTracker::pop() {
  StyleDiff diff = Cascade.back();
  Cascade.pop_back();
  // Reverse-iterate over parents and replace all styles referencing
  // a style from `diff` with a parent style
  auto It = Cascade.rbegin();
  while (diff.styles.size() && It != Cascade.rend()) {
    const StyleDiff &parent = *(It++);
    for (const auto &KeyValuePair : parent.styles)
      if (diff.styles.count(KeyValuePair.first)) {
        diff.styles.erase(KeyValuePair.first);
        CurrentStyle[KeyValuePair.first] = KeyValuePair.second;
      }
  }
  // If some styles remain in `diff.styles` that means there are no parent
  // style definitions and we completely erase them
  for (const auto &KeyValuePair : diff.styles)
    CurrentStyle.erase(KeyValuePair.first);
}

CSSColor StyleTracker::getColor() const {
  if (CurrentStyle.count(Style::COLOR))
    return CSSColor::parse(CurrentStyle.at(Style::COLOR));
  return CSSColor();
}
CSSColor StyleTracker::getFill() const {
  if (CurrentStyle.count(Style::FILL))
    return CSSColor::parse(CurrentStyle.at(Style::FILL));
  return getColor();
}
CSSColor StyleTracker::getStroke() const {
  if (CurrentStyle.count(Style::STROKE))
    return CSSColor::parse(CurrentStyle.at(Style::STROKE));
  return getColor();
}
CSSUnit StyleTracker::getStrokeWidth() const {
  if (CurrentStyle.count(Style::STROKE_WIDTH))
    return CSSUnit::parse(CurrentStyle.at(Style::STROKE_WIDTH));
  return CSSUnit();
}
CSSDashArray StyleTracker::getStrokeDasharray() const {
  if (CurrentStyle.count(Style::STROKE_DASHARRAY))
    return CSSDashArray::parse(CurrentStyle.at(Style::STROKE_DASHARRAY));
  return CSSDashArray();
}
std::string_view StyleTracker::getFontFamily() const {
  if (CurrentStyle.count(Style::FONT_FAMILY))
    return CurrentStyle.at(Style::FONT_FAMILY);
  return std::string_view();
}
CSSUnit StyleTracker::getFontSize() const {
  if (CurrentStyle.count(Style::FONT_SIZE))
    return CSSUnit::parse(CurrentStyle.at(Style::FONT_SIZE));
  return CSSUnit();
}
CSSTextAnchor StyleTracker::getTextAnchor() const {
  if (CurrentStyle.count(Style::TEXT_ANCHOR)) {
    auto anchorStr = CurrentStyle.at(Style::TEXT_ANCHOR);
    if (anchorStr == "start")
      return CSSTextAnchor::START;
    else if (anchorStr == "middle")
      return CSSTextAnchor::MIDDLE;
    else if (anchorStr == "end")
      return CSSTextAnchor::END;
  }
  return CSSTextAnchor::START;
}
