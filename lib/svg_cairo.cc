#include "svgcairo/svg_cairo.h"
#include <algorithm>
#include <cairo/cairo-ft.h>
#include <cairo/cairo-pdf.h>
#include <cmath>
#include <cstring>

using namespace svg;

static void cairo_deleter(cairo_t *cr) {
  if (!cr)
    return;
  cairo_destroy(cr);
#ifndef NDEBUG
  // FIXME text rendering causes this to assert
  // cairo_debug_reset_static_data();
#endif
}

enum class CairoSVGWriter::TagType {
  NONE = 0,
  CUSTOM,
#define SVG_TAG(NAME, STR, ...) NAME,
#include "svgutils/svg_entities.def"
};

void CairoSVGWriter::initCairo() {
  double w = width ? width : dfltWidth;
  double h = height ? height : dfltHeight;
  switch (fmt) {
  case CairoSVGWriter::PDF:
    surface = OwnedSurface(
        cairo_pdf_surface_create(outfile.c_str(), w / 1.25, h / 1.25),
        cairo_surface_destroy);
    break;
  case CairoSVGWriter::PNG:
    surface =
        OwnedSurface(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h),
                     cairo_surface_destroy);
    break;
  };
  assert(cairo_surface_status(surface.get()) == CAIRO_STATUS_SUCCESS &&
         "Error initializing cairo pdf surface");
  cairo = OwnedCairo(cairo_create(surface.get()), cairo_deleter);
  assert(cairo_status(cairo.get()) == CAIRO_STATUS_SUCCESS &&
         "Error initializing cairo context");
}

CairoSVGWriter::CairoSVGWriter(const fs::path &outfile, OutputFormat fmt)
    : outfile(outfile), fmt(fmt),
      fonts(/*FIXME proper checking of optional*/ *Freetype::Create()),
      currentTag(TagType::NONE) {
  initCairo();
}

CairoSVGWriter::CairoSVGWriter(const fs::path &outfile, OutputFormat fmt,
                               double width, double height)
    : outfile(outfile), fmt(fmt),
      fonts(/*FIXME proper checking of optional*/ *Freetype::Create()),
      width(width), height(height), currentTag(TagType::NONE) {
  initCairo();
}

CairoSVGWriter &
CairoSVGWriter::custom_tag(const char *name,
                           const std::vector<SVGAttribute> &attrs) {
  // We mostly ignore all custom tags
  openTag(TagType::CUSTOM, attrs);
  return *this;
}

CairoSVGWriter &CairoSVGWriter::content(const char *text) {
  closeTag();
  // Reference:
  // https://github.com/Distrotech/cairo/blob/17ef4acfcb64d1c525910a200e60d63087953c4c/src/cairo.c#L3197
  if (parents.top() == TagType::CUSTOM)
    return *this;
  assert(parents.size() && parents.top() == TagType::text &&
         "Content is only supported in text nodes at the moment");
  if (text == nullptr)
    return *this;

  if (cairo_status(cairo.get()))
    svg_unreachable("Cairo is in an invalid state");

  // Collect the style information
  CSSUnit cssFontSize = styles.getFontSize();
  double fontSize = convertCSSWidth(cssFontSize);
  CSSColor color = styles.getFill();
  std::string fontPattern(styles.getFontFamily());
  CSSTextAnchor anchor = styles.getTextAnchor();

  FT_Face ftFont = fonts.getFace(fontPattern.c_str());
  if (!ftFont)
    svg_unreachable("Error loading font");

  // Set up cairo font properties
  cairo_font_face_t *cairoFont =
      cairo_ft_font_face_create_for_ft_face(ftFont, 0);
  cairo_set_font_face(cairo.get(), cairoFont);
  cairo_set_font_size(cairo.get(), fontSize);
  cairo_set_source_rgba(cairo.get(), color.r, color.g, color.b, color.a);
  int textlen = std::strlen(text);
  double x, y;
  cairo_get_current_point(cairo.get(), &x, &y);

  cairo_scaled_font_t *scaled_font = cairo_get_scaled_font(cairo.get());
  if (cairo_scaled_font_status(scaled_font))
    svg_unreachable("Error getting scaled font");

  // Convert text to glyphs+clusters
  auto glyphs = std::unique_ptr<cairo_glyph_t, decltype(&cairo_glyph_free)>(
      nullptr, cairo_glyph_free);
  auto clusters =
      std::unique_ptr<cairo_text_cluster_t, decltype(&cairo_text_cluster_free)>(
          nullptr, cairo_text_cluster_free);
  int num_glyphs = 0;
  int num_clusters = 0;
  cairo_text_cluster_flags_t cluster_flags;
  {
    cairo_glyph_t *glyphs_raw = nullptr;
    cairo_text_cluster_t *clusters_raw = nullptr;
    cairo_status_t status = cairo_scaled_font_text_to_glyphs(
        scaled_font, x, y, text, textlen, &glyphs_raw, &num_glyphs,
        &clusters_raw, &num_clusters, &cluster_flags);
    if (status)
      svg_unreachable("Failed to convert text to glyphs");

    if (num_glyphs == 0)
      return *this;

    glyphs = {glyphs_raw, cairo_glyph_free};
    clusters = {clusters_raw, cairo_text_cluster_free};
  }

  if (anchor != CSSTextAnchor::START) {
    cairo_text_extents_t textextents;
    cairo_scaled_font_glyph_extents(scaled_font, glyphs.get(), num_glyphs,
                                    &textextents);
    if (cairo_status(cairo.get()))
      svg_unreachable("Failed to retrieve glyph extents");
    if (anchor == CSSTextAnchor::MIDDLE)
      for (int i = 0; i < num_glyphs; ++i)
        glyphs.get()[i].x -= textextents.x_advance / 2;
    else if (anchor == CSSTextAnchor::END)
      for (int i = 0; i < num_glyphs; ++i)
        glyphs.get()[i].x -= textextents.x_advance;
    else
      svg_unreachable("Invalid text anchor");
  }

  // Render the text
  cairo_show_text_glyphs(cairo.get(), text, textlen, glyphs.get(), num_glyphs,
                         clusters.get(), num_clusters, cluster_flags);
  if (cairo_status(cairo.get()))
    svg_unreachable("Error drawing text glyphs");
  // Render stroke
  CSSUnit cssStrokeWidth = styles.getStrokeWidth();
  double strokeWidth = convertCSSWidth(cssStrokeWidth);
  CSSColor strokeColor = styles.getStroke();
  if (strokeWidth != 0. && strokeColor) {
    cairo_glyph_path(cairo.get(), glyphs.get(), num_glyphs);
    applyCSSStroke(false);
  }

  // Move the drawing pencil to the end of the text
  cairo_glyph_t *last_glyph = &glyphs.get()[num_glyphs - 1];
  cairo_text_extents_t extents;
  cairo_glyph_extents(cairo.get(), last_glyph, 1, &extents);
  if (cairo_status(cairo.get()))
    svg_unreachable("Failed to retrieve extents of the last glyph");

  x = last_glyph->x + extents.x_advance;
  y = last_glyph->y + extents.y_advance;
  cairo_move_to(cairo.get(), x, y);

  return *this;
}

CairoSVGWriter &CairoSVGWriter::comment(const char *comment) { return *this; }

CairoSVGWriter &CairoSVGWriter::enter() {
  assert(currentTag != TagType::NONE && "Cannot enter without root tag");
  parents.push(currentTag);
  currentTag = TagType::NONE;
  return *this;
}
CairoSVGWriter &CairoSVGWriter::leave() {
  assert(parents.size() && "Cannot leave: No parent tag");
  currentTag = parents.top();
  parents.pop();
  return *this;
}
CairoSVGWriter &CairoSVGWriter::finish() {
  while (parents.size())
    leave();
  closeTag();
  switch (fmt) {
  case PDF:
    cairo_show_page(cairo.get());
    break;
  case PNG:
    cairo_surface_write_to_png(surface.get(), outfile.c_str());
    break;
  }
  return *this;
}

void CairoSVGWriter::closeTag() {
  if (currentTag != TagType::NONE) {
    switch (currentTag) {
    case TagType::svg:
      cairo_pop_group_to_source(cairo.get());
      cairo_paint(cairo.get());
      break;
    default:
      break;
    }
    styles.pop();
    currentTag = TagType::NONE;
    assert(cairo_status(cairo.get()) == CAIRO_STATUS_SUCCESS &&
           "Cairo is in an invalid state");
  }
}

void CairoSVGWriter::openTag(TagType T,
                             const CairoSVGWriter::AttrContainer &attrs) {
  if (T != TagType::svg && !width && !height) {
    width = dfltWidth;
    height = dfltHeight;
  }
  closeTag();
  currentTag = T;
  styles.push(attrs);
}

static double convertCSSLength(const CSSUnit &unit,
                               CairoSVGWriter::OutputFormat fmt) {
  double len = unit.length;
  if (unit.unit == CSSUnit::PX)
    len = len;
  else if (unit.unit == CSSUnit::PT)
    len *= 1.25;
  else if (unit.unit == CSSUnit::PC)
    len *= 15.;
  else if (unit.unit == CSSUnit::MM)
    len *= 3.543307;
  else if (unit.unit == CSSUnit::CM)
    len *= 35.43307;
  else if (unit.unit == CSSUnit::IN)
    len *= 90.;
  else
    svg_unreachable("Encountered unexpected css unit");
  switch (fmt) {
  case CairoSVGWriter::PDF:
    // cairo pdf units are pt (= 1/72in)
    len /= 1.25;
    break;
  case CairoSVGWriter::PNG:
    break;
  };

  return len;
}

double CairoSVGWriter::convertCSSWidth(const CSSUnit &unit) const {
  if (unit.unit == CSSUnit::PERCENT)
    return unit.length / 100. * getWidth();
  return convertCSSLength(unit, fmt);
}
double CairoSVGWriter::convertCSSHeight(const CSSUnit &unit) const {
  if (unit.unit == CSSUnit::PERCENT)
    return unit.length / 100. * getHeight();
  return convertCSSLength(unit, fmt);
}

/// Utility function to extract a CSS unit from the value of an
/// SVGAttribute
static CSSUnit CSSUnitFrom(const SVGAttribute &attr) {
  if (const char *cstr = attr.cstrOrNull())
    return CSSUnit::parse(cstr);
  CSSUnit res;
  res.length = attr.toDouble();
  return res;
}

void CairoSVGWriter::applyCSSStroke(bool preserve) {
  // Render stroke
  CSSUnit cssStrokeWidth = styles.getStrokeWidth();
  double strokeWidth = convertCSSWidth(cssStrokeWidth);
  CSSColor strokeColor = styles.getStroke();
  if (strokeWidth != 0. && strokeColor) {
    // Apply styles
    const CSSDashArray strokeDasharray = styles.getStrokeDasharray();
    std::vector<double> dashes;
    for (const CSSUnit &len : strokeDasharray.dashes)
      dashes.emplace_back(convertCSSWidth(len));
    // FIXME look up what percentages mean in stroke-width
    cairo_set_dash(cairo.get(), dashes.data(), dashes.size(), 0.);
    cairo_set_line_width(cairo.get(), strokeWidth);
    cairo_set_source_rgba(cairo.get(), strokeColor.r, strokeColor.g,
                          strokeColor.b, strokeColor.a);
    if (preserve)
      cairo_stroke_preserve(cairo.get());
    else
      cairo_stroke(cairo.get());
  } else if (!preserve)
    cairo_new_path(cairo.get());
}

void CairoSVGWriter::applyCSSFill(bool preserve) {
  CSSColor bg = styles.getFill();
  if (!bg) {
    if (!preserve)
      cairo_new_path(cairo.get());
    return;
  }
  cairo_set_source_rgba(cairo.get(), bg.r, bg.g, bg.b, bg.a);
  if (preserve)
    cairo_fill_preserve(cairo.get());
  else
    cairo_fill(cairo.get());
}

void CairoSVGWriter::applyCSSFillAndStroke(bool preserve) {
  applyCSSStroke(true);
  applyCSSFill(preserve);
}

#define SVG_TAG(NAME, STR, ...)                                                \
  CairoSVGWriter &CairoSVGWriter::NAME(const AttrContainer &attrs) {           \
    openTag(TagType::NAME, attrs);                                             \
    NAME##_impl(attrs);                                                        \
    return *this;                                                              \
  }
#include "svgutils/svg_entities.def"

void CairoSVGWriter::a_impl(const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::altGlyph_impl(const CairoSVGWriter::AttrContainer &attrs) {
}
void CairoSVGWriter::altGlyphDef_impl(
    const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::altGlyphItem_impl(
    const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::animate_impl(const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::animateColor_impl(
    const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::animateMotion_impl(
    const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::animateTransform_impl(
    const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::circle_impl(const CairoSVGWriter::AttrContainer &attrs) {
  CSSUnit cx, cy, r;
  struct AttrParser : public SVGAttributeVisitor<AttrParser> {
    AttrParser(CSSUnit &cx, CSSUnit &cy, CSSUnit &r) : cx(cx), cy(cy), r(r) {}
    void visit_cx(const svg::cx &x) { cx = CSSUnitFrom(x); }
    void visit_cy(const svg::cy &y) { cy = CSSUnitFrom(y); }
    void visit_r(const svg::r &radius) { r = CSSUnitFrom(radius); }
    CSSUnit &cx;
    CSSUnit &cy;
    CSSUnit &r;
  } attrParser(cx, cy, r);
  for (const SVGAttribute &Attr : attrs)
    attrParser.visit(Attr);
  cairo_arc(cairo.get(), convertCSSWidth(cx), convertCSSHeight(cy),
            convertCSSWidth(r), 0., 2 * M_PI);
  applyCSSFillAndStroke(false);
}
void CairoSVGWriter::clipPath_impl(const CairoSVGWriter::AttrContainer &attrs) {
}
void CairoSVGWriter::color_profile_impl(
    const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::cursor_impl(const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::defs_impl(const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::desc_impl(const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::discard_impl(const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::ellipse_impl(const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::feBlend_impl(const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::feColorMatrix_impl(
    const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::feComponentTransfer_impl(
    const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::feComposite_impl(
    const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::feConvolveMatrix_impl(
    const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::feDiffuseLighting_impl(
    const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::feDisplacementMap_impl(
    const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::feDistantLight_impl(
    const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::feDropShadow_impl(
    const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::feFlood_impl(const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::feFuncA_impl(const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::feFuncB_impl(const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::feFuncG_impl(const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::feFuncR_impl(const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::feGaussianBlur_impl(
    const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::feImage_impl(const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::feMerge_impl(const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::feMergeNode_impl(
    const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::feMorphology_impl(
    const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::feOffset_impl(const CairoSVGWriter::AttrContainer &attrs) {
}
void CairoSVGWriter::fePointLight_impl(
    const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::feSpecularLighting_impl(
    const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::feSpotLight_impl(
    const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::feTile_impl(const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::feTurbulence_impl(
    const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::filter_impl(const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::font_impl(const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::font_face_impl(
    const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::font_face_format_impl(
    const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::font_face_name_impl(
    const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::font_face_src_impl(
    const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::font_face_uri_impl(
    const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::foreignObject_impl(
    const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::g_impl(const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::glyph_impl(const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::glyphRef_impl(const CairoSVGWriter::AttrContainer &attrs) {
}
void CairoSVGWriter::hatch_impl(const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::hatchpath_impl(
    const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::hkern_impl(const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::image_impl(const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::line_impl(const CairoSVGWriter::AttrContainer &attrs) {
  CSSUnit x1, y1, x2, y2;
  struct AttrParser : public SVGAttributeVisitor<AttrParser> {
    AttrParser(CSSUnit &x1, CSSUnit &y1, CSSUnit &x2, CSSUnit &y2)
        : x1(x1), y1(y1), x2(x2), y2(y2) {}
    void visit_x1(const svg::x1 &x) { x1 = CSSUnitFrom(x); }
    void visit_y1(const svg::y1 &y) { y1 = CSSUnitFrom(y); }
    void visit_x2(const svg::x2 &x) { x2 = CSSUnitFrom(x); }
    void visit_y2(const svg::y2 &y) { y2 = CSSUnitFrom(y); }
    CSSUnit &x1;
    CSSUnit &y1;
    CSSUnit &x2;
    CSSUnit &y2;
  } attrParser(x1, y1, x2, y2);
  for (const SVGAttribute &Attr : attrs)
    attrParser.visit(Attr);
  cairo_move_to(cairo.get(), convertCSSWidth(x1), convertCSSHeight(y1));
  cairo_line_to(cairo.get(), convertCSSWidth(x2), convertCSSHeight(y2));
  applyCSSStroke(false);
}
void CairoSVGWriter::linearGradient_impl(
    const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::marker_impl(const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::mask_impl(const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::mesh_impl(const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::meshgradient_impl(
    const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::meshpatch_impl(
    const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::meshrow_impl(const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::metadata_impl(const CairoSVGWriter::AttrContainer &attrs) {
}
void CairoSVGWriter::missing_glyph_impl(
    const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::mpath_impl(const CairoSVGWriter::AttrContainer &attrs) {}

// Grammar for SVG paths: https://www.w3.org/TR/SVG11/paths.html#PathDataBNF
static std::string_view ExtractPathArg(/* inout */ std::string_view &args) {
  constexpr const char *Error = "";
  if (args.empty())
    return Error;
  assert(!std::isspace(args.front()) && "Expected trimmed argument string");
  // Numbers must start with +, -, . or digit
  if (!std::isdigit(args.front()) && args.front() != '+' &&
      args.front() != '-' && args.front() != '.')
    return Error;
  // args.begin() != args.end() because !args.empty()
  std::string_view::iterator End = args.begin();
  bool hasDigit = false;
  if (*End == '+' || *End == '-') {
    ++End; // Move past +|-
    if (End == args.end())
      return Error; // Cannot end after +|-
  }
  if (*End != '.') {
    // consume leading digits
    while (End != args.end() && std::isdigit(*End)) {
      hasDigit = true;
      ++End;
    }
    if (!hasDigit)
      return Error; // We might have skipped past +|- and no '.' was found. By
                    // now there must have been a digit.
  }
  if (End != args.end() && *End == '.') {
    ++End; // Move past '.'
    // consume decimal digits
    while (End != args.end() && std::isdigit(*End)) {
      hasDigit = true;
      ++End;
    }
    if (!hasDigit)
      return Error; // '.' requires digit either before or after
  }
  if (End != args.end() && (*End == 'e' || *End == 'E')) {
    ++End; // Move past e|E
    if (End == args.end())
      return Error; // Cannot stop after e|E
    if (*End == '+' || *End == '-')
      ++End;
    if (End == args.end() || !std::isdigit(*End))
      return Error; // Cannot stop after +|-
    hasDigit = false;
    while (End != args.end() && std::isdigit(*End)) {
      hasDigit = true;
      ++End;
    }
    if (!hasDigit)
      return Error; // No digit in exponent
  }

  std::string_view argStr = args.substr(0, End - args.begin());
  if (End == args.end())
    args.remove_prefix(args.size());
  // If the value is delimited by a comma or space, the delimiter should
  // be dropped as well
  else if (std::isspace(*End) || *End == ',')
    args.remove_prefix((End - args.begin()) + 1);
  else
    args.remove_prefix(End - args.begin());
  args = strview_trim(args);
  argStr = strview_trim(argStr);
  return argStr;
}

static bool ExtractPathArgs(/* inout */ std::string_view &argsStr) {
  return true;
}

template <typename... args_t>
static bool ExtractPathArgs(/* inout */ std::string_view &argsStr,
                            /* out */ std::string_view &arg1,
                            args_t &... argn) {
  arg1 = ExtractPathArg(argsStr);
  if (arg1.empty())
    return false;
  return ExtractPathArgs(argsStr, argn...);
}

CairoSVGWriter::PathErrorOr<CairoSVGWriter::ControlPoint>
CairoSVGWriter::CairoExecuteCubicBezier(std::string_view args, bool rel) {
  if (args.empty())
    return PathError("No arguments given to C/c command");
  std::string_view cp1X, cp1Y, cp2X, cp2Y, destX, destY;
  ControlPoint CP;
  while (args.size()) {
    if (!ExtractPathArgs(args, cp1X, cp1Y, cp2X, cp2Y, destX, destY))
      return PathError("Not enough arguments given to C/c command");
    auto x1 = strview_to_double(cp1X);
    auto y1 = strview_to_double(cp1Y);
    auto x2 = strview_to_double(cp2X);
    auto y2 = strview_to_double(cp2Y);
    auto x3 = strview_to_double(destX);
    auto y3 = strview_to_double(destY);
    if (!(x1 && y1 && x2 && y2 && x3 && y3))
      return PathError("Invalid arguments given to C/c command");
    if (rel) {
      double x0, y0;
      cairo_get_current_point(cairo.get(), &x0, &y0);
      CP = {x0 + *x2, y0 + *y2};
      cairo_rel_curve_to(cairo.get(), *x1, *y1, *x2, *y2, *x3, *y3);
    } else {
      CP = {*x2, *y2};
      cairo_curve_to(cairo.get(), *x1, *y1, *x2, *y2, *x3, *y3);
    }
  }
  return CP;
}
CairoSVGWriter::PathErrorOr<CairoSVGWriter::ControlPoint>
CairoSVGWriter::CairoExecuteSmoothCubicBezier(
    std::string_view args, bool rel,
    const std::optional<ControlPoint> &PrevCP) {
  if (args.empty())
    return PathError("No arguments given to S/s command");
  std::string_view x2, y2, x3, y3;
  ControlPoint CP;
  while (args.size()) {
    if (!ExtractPathArgs(args, x2, y2, x3, y3))
      return PathError("Not enough arguments given to S/s command");
    // TODO
  }
  return CP;
}
CairoSVGWriter::PathErrorOr<CairoSVGWriter::ControlPoint>
CairoSVGWriter::CairoExecuteQuadraticBezier(std::string_view args, bool rel) {
  if (args.empty())
    return PathError("No arguments given to Q/q command");
  std::string_view cpX, cpY, destX, destY;
  ControlPoint CP;
  while (args.size()) {
    if (!ExtractPathArgs(args, cpX, cpY, destX, destY))
      return PathError("Not enough arguments given to Q/q command");
    double x0, y0;
    cairo_get_current_point(cairo.get(), &x0, &y0);
    auto x3 = strview_to_double(destX);
    auto y3 = strview_to_double(destY);
    // the quadratic curve's control point
    auto qx = strview_to_double(cpX);
    auto qy = strview_to_double(cpY);
    if (!(x3 && y3 && qx && qy))
      return PathError("Invalid arguments given to Q/q command");
    // https://stackoverflow.com/a/3162732/1468532
    double x1 = x0 + 2. / 3. * (*qx - x0);
    double y1 = y0 + 2. / 3. * (*qy - y0);
    double x2 = *x3 + 2. / 3. * (*qx - *x3);
    double y2 = *y3 + 2. / 3. * (*qy - *y3);
    if (rel) {
      CP = {x0 + x2, y0 + y2};
      cairo_rel_curve_to(cairo.get(), x1, y1, x2, y2, *x3, *y3);
    } else {
      CP = {x2, y2};
      cairo_curve_to(cairo.get(), x1, y1, x2, y2, *x3, *y3);
    }
  }
  return CP;
}
CairoSVGWriter::PathErrorOr<CairoSVGWriter::ControlPoint>
CairoSVGWriter::CairoExecuteSmoothQuadraticBezier(
    std::string_view args, bool rel,
    const std::optional<ControlPoint> &PrevCP) {
  if (args.empty())
    return PathError("No arguments given to T/t command");
  std::string_view x3, y3;
  ControlPoint CP;
  while (args.size()) {
    if (!ExtractPathArgs(args, x3, y3))
      return PathError("Not enough arguments given to T/t command");
    // TODO
  }
  return CP;
}
CairoSVGWriter::PathErrorOrVoid
CairoSVGWriter::CairoExecuteArc(std::string_view args, bool rel) {
  std::string_view rx, ry, largeArcFlag, sweepFlag, x, y;
  while (args.size()) {
    if (!ExtractPathArgs(args, rx, ry, largeArcFlag, sweepFlag, x, y))
      return PathError{"Not enough arguments given to A/a command"};
    // TODO
  }
  return {};
}

CairoSVGWriter::PathErrorOrVoid
CairoSVGWriter::CairoExecuteHLine(std::string_view length, bool rel) {
  double X, Y;
  cairo_get_current_point(cairo.get(), &X, &Y);
  while (length.size()) {
    std::string_view lenStr;
    if (!ExtractPathArgs(length, lenStr))
      return PathError{"No argument given to H/h command"};
    CSSUnit lenCss = CSSUnit::parse(lenStr);
    double len = convertCSSWidth(lenCss);
    if (rel)
      cairo_rel_line_to(cairo.get(), len, 0);
    else
      cairo_line_to(cairo.get(), len, Y);
  }
  return {};
}
CairoSVGWriter::PathErrorOrVoid
CairoSVGWriter::CairoExecuteVLine(std::string_view length, bool rel) {
  double X, Y;
  cairo_get_current_point(cairo.get(), &X, &Y);
  while (length.size()) {
    std::string_view lenStr;
    if (!ExtractPathArgs(length, lenStr))
      return PathError{"No argument given to V/v command"};
    CSSUnit lenCss = CSSUnit::parse(lenStr);
    double len = convertCSSHeight(lenCss);
    if (rel)
      cairo_rel_line_to(cairo.get(), 0, len);
    else
      cairo_line_to(cairo.get(), X, len);
  }
  return {};
}

CairoSVGWriter::PathErrorOrVoid
CairoSVGWriter::CairoExecuteLineTo(std::string_view points, bool rel) {
  while (points.size()) {
    std::string_view xStr, yStr;
    if (!ExtractPathArgs(points, xStr, yStr))
      return PathError{"Not enough arguments given to L/l command"};
    CSSUnit xCss = CSSUnit::parse(xStr);
    CSSUnit yCss = CSSUnit::parse(yStr);
    double x = convertCSSWidth(xCss);
    double y = convertCSSHeight(yCss);
    if (rel)
      cairo_rel_line_to(cairo.get(), x, y);
    else
      cairo_line_to(cairo.get(), x, y);
  }
  return {};
}
CairoSVGWriter::PathErrorOrVoid
CairoSVGWriter::CairoExecuteMoveTo(std::string_view points, bool rel) {
  if (points.empty())
    return PathError{"No arguments given to M/m command"};
  std::string_view xStr, yStr;
  if (!ExtractPathArgs(points, xStr, yStr))
    return PathError("Not enough arguments given to M/m command");
  CSSUnit xCss = CSSUnit::parse(xStr);
  CSSUnit yCss = CSSUnit::parse(yStr);
  double x = convertCSSWidth(xCss);
  double y = convertCSSHeight(yCss);
  if (rel)
    cairo_rel_move_to(cairo.get(), x, y);
  else
    cairo_move_to(cairo.get(), x, y);
  CairoExecuteLineTo(points, rel);
  return {};
}

CairoSVGWriter::PathErrorOrVoid
CairoSVGWriter::CairoExecutePath(const char *pathRaw) {
  std::string_view path = strview_trim(pathRaw);
  const char commands[] = "MmLlHhVvCcSsQqTtAaZz";
  // Invariant: There is always non-whitespace content in path and path is
  // trimmed
  while (path.size()) {
    auto cmdpos = path.find_first_of(commands);
    if (cmdpos == path.npos)
      return PathError{"Did not find any valid commands"};
    char cmd = path[cmdpos];
    path.remove_prefix(cmdpos + 1);
    auto argsEnd = path.find_first_of(commands);
    std::string_view args = path.substr(0, argsEnd);
    args = strview_trim(args);
    if (argsEnd == path.npos)
      path.remove_prefix(path.size());
    else
      path.remove_prefix(argsEnd);
    const bool rel = std::tolower(cmd) == cmd;
    PathErrorOrVoid err;
    std::optional<ControlPoint> PrevCP;
    switch (std::tolower(cmd)) {
    case 'm':
      err = CairoExecuteMoveTo(args, rel);
      PrevCP = std::nullopt;
      break;
    case 'l':
      err = CairoExecuteLineTo(args, rel);
      PrevCP = std::nullopt;
      break;
    case 'h':
      err = CairoExecuteHLine(args, rel);
      PrevCP = std::nullopt;
      break;
    case 'v':
      err = CairoExecuteVLine(args, rel);
      PrevCP = std::nullopt;
      break;
    case 'c': {
      auto errOrCP = CairoExecuteCubicBezier(args, rel);
      if (errOrCP)
        err = errOrCP.to_error();
      else
        PrevCP = *errOrCP;
      break;
    }
    case 's': {
      auto errOrCP = CairoExecuteSmoothCubicBezier(args, rel, PrevCP);
      if (errOrCP)
        err = errOrCP.to_error();
      else
        PrevCP = *errOrCP;
      break;
    }
    case 'q': {
      auto errOrCP = CairoExecuteQuadraticBezier(args, rel);
      if (errOrCP)
        err = errOrCP.to_error();
      else
        PrevCP = *errOrCP;
      break;
    }
    case 't': {
      auto errOrCP = CairoExecuteSmoothQuadraticBezier(args, rel, PrevCP);
      if (errOrCP)
        err = errOrCP.to_error();
      else
        PrevCP = *errOrCP;
      break;
    }
    case 'a':
      err = CairoExecuteArc(args, rel);
      PrevCP = std::nullopt;
      break;
    case 'z':
      cairo_close_path(cairo.get());
      PrevCP = std::nullopt;
      break;
    default:
      svg_unreachable("Encountered unknown command");
    }
    if (err)
      return err;
  }
  return {};
}

void CairoSVGWriter::path_impl(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::path, attrs);
  std::optional<SVGAttribute> pathDesc;
  struct AttrParser : public SVGAttributeVisitor<AttrParser> {
    AttrParser(std::optional<SVGAttribute> &pathDesc) : pathDesc(pathDesc) {}
    void visit_d(const svg::d &desc) {
      pathDesc = static_cast<SVGAttribute>(desc);
    }
    std::optional<SVGAttribute> &pathDesc;
  } attrParser(pathDesc);
  for (const SVGAttribute &Attr : attrs)
    attrParser.visit(Attr);
  if (!pathDesc)
    return;
  if (auto err = CairoExecutePath(pathDesc->cstrOrNull()))
    svg_unreachable(err.to_error().what().c_str());
  applyCSSFillAndStroke(false);
}
void CairoSVGWriter::pattern_impl(const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::polygon_impl(const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::polyline_impl(const CairoSVGWriter::AttrContainer &attrs) {
}
void CairoSVGWriter::radialGradient_impl(
    const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::rect_impl(const CairoSVGWriter::AttrContainer &attrs) {
  CSSUnit x, y, width = styles.getWidth(), height = styles.getHeight();
  struct AttrParser : public SVGAttributeVisitor<AttrParser> {
    AttrParser(CSSUnit &x, CSSUnit &y) : x(x), y(y) {}
    void visit_x(const svg::x &xAttr) { x = CSSUnitFrom(xAttr); }
    void visit_y(const svg::y &yAttr) { y = CSSUnitFrom(yAttr); }
    CSSUnit &x;
    CSSUnit &y;
  } attrParser(x, y);
  for (const SVGAttribute &Attr : attrs)
    attrParser.visit(Attr);
  cairo_rectangle(cairo.get(), convertCSSWidth(x), convertCSSHeight(y),
                  convertCSSWidth(width), convertCSSHeight(height));
  applyCSSFillAndStroke(false);
}
void CairoSVGWriter::script_impl(const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::set_impl(const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::solidcolor_impl(
    const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::stop_impl(const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::style_impl(const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::svg_impl(const CairoSVGWriter::AttrContainer &attrs) {
  if (!width && !height) {
    // We're allowed to read the document dimension from the svg tag.
    // If this isn't possible, fall back to defaults __and do not try again__.
    CSSUnit w, h;
    struct DimFinder : public SVGAttributeVisitor<DimFinder> {
      DimFinder(CSSUnit &w, CSSUnit &h) : w(w), h(h) {}
      void visit_width(const svg::width &width) { w = CSSUnitFrom(width); }
      void visit_height(const svg::height &height) { h = CSSUnitFrom(height); }
      CSSUnit &w;
      CSSUnit &h;
    } findDims(w, h);
    for (const SVGAttribute &attr : attrs)
      findDims.visit(attr);
    if (!w.length && !h.length) {
      width = dfltWidth;
      height = dfltHeight;
    } else {
      if (w.unit == CSSUnit::PERCENT)
        width = w.length / 100 * getWidth();
      else
        width =
            convertCSSLength(w, PNG); // Use any format that uses px as units
      if (h.unit == CSSUnit::PERCENT)
        height = h.length / 100 * getHeight();
      else
        height =
            convertCSSLength(h, PNG); // Use any format that uses px as units
    }
    // Default dimensions might have changed, so re-init Cairo
    initCairo();
  }
  cairo_push_group(cairo.get());
}
void CairoSVGWriter::switch__impl(const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::symbol_impl(const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::text_impl(const CairoSVGWriter::AttrContainer &attrs) {
  CSSUnit x, y;
  struct AttrParser : public SVGAttributeVisitor<AttrParser> {
    AttrParser(CSSUnit &x, CSSUnit &y) : x(x), y(y) {}
    void visit_x(const svg::x &xAttr) { x = CSSUnitFrom(xAttr); }
    void visit_y(const svg::y &yAttr) { y = CSSUnitFrom(yAttr); }
    CSSUnit &x;
    CSSUnit &y;
  } attrParser(x, y);
  for (const SVGAttribute &Attr : attrs)
    attrParser.visit(Attr);
  cairo_move_to(cairo.get(), convertCSSWidth(x), convertCSSHeight(y));
}
void CairoSVGWriter::textPath_impl(const CairoSVGWriter::AttrContainer &attrs) {
}
void CairoSVGWriter::title_impl(const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::tref_impl(const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::tspan_impl(const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::unknown_impl(const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::use_impl(const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::view_impl(const CairoSVGWriter::AttrContainer &attrs) {}
void CairoSVGWriter::vkern_impl(const CairoSVGWriter::AttrContainer &attrs) {}
