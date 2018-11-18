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

CairoSVGWriter &CairoSVGWriter::content(const char *text) {
  closeTag();
  // Reference:
  // https://github.com/Distrotech/cairo/blob/17ef4acfcb64d1c525910a200e60d63087953c4c/src/cairo.c#L3197
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
  if (!width && !height) {
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

CairoSVGWriter &CairoSVGWriter::a(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::a, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::altGlyph(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::altGlyph, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::altGlyphDef(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::altGlyphDef, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::altGlyphItem(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::altGlyphItem, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::animate(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::animate, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::animateColor(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::animateColor, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::animateMotion(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::animateMotion, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::animateTransform(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::animateTransform, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::circle(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::circle, attrs);
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
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::clipPath(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::clipPath, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::color_profile(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::color_profile, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::cursor(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::cursor, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::defs(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::defs, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::desc(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::desc, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::discard(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::discard, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::ellipse(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::ellipse, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feBlend(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::feBlend, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feColorMatrix(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::feColorMatrix, attrs);
  return *this;
}
CairoSVGWriter &CairoSVGWriter::feComponentTransfer(
    const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::feComponentTransfer, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feComposite(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::feComposite, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feConvolveMatrix(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::feConvolveMatrix, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feDiffuseLighting(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::feDiffuseLighting, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feDisplacementMap(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::feDisplacementMap, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feDistantLight(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::feDistantLight, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feDropShadow(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::feDropShadow, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feFlood(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::feFlood, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feFuncA(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::feFuncA, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feFuncB(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::feFuncB, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feFuncG(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::feFuncG, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feFuncR(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::feFuncR, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feGaussianBlur(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::feGaussianBlur, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feImage(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::feImage, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feMerge(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::feMerge, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feMergeNode(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::feMergeNode, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feMorphology(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::feMorphology, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feOffset(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::feOffset, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::fePointLight(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::fePointLight, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feSpecularLighting(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::feSpecularLighting, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feSpotLight(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::feSpotLight, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feTile(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::feTile, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feTurbulence(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::feTurbulence, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::filter(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::filter, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::font(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::font, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::font_face(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::font_face, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::font_face_format(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::font_face_format, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::font_face_name(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::font_face_name, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::font_face_src(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::font_face_src, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::font_face_uri(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::font_face_uri, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::foreignObject(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::foreignObject, attrs);
  return *this;
}
CairoSVGWriter &CairoSVGWriter::g(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::g, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::glyph(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::glyph, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::glyphRef(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::glyphRef, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::hatch(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::hatch, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::hatchpath(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::hatchpath, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::hkern(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::hkern, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::image(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::image, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::line(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::line, attrs);
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
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::linearGradient(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::linearGradient, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::marker(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::marker, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::mask(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::mask, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::mesh(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::mesh, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::meshgradient(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::meshgradient, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::meshpatch(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::meshpatch, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::meshrow(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::meshrow, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::metadata(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::metadata, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::missing_glyph(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::missing_glyph, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::mpath(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::mpath, attrs);
  return *this;
}

bool CairoSVGWriter::CairoExecuteHLine(std::string_view length, bool rel) {
  double X, Y;
  cairo_get_current_point(cairo.get(), &X, &Y);
  while (length.size()) {
    auto End = std::find_if(length.begin(), length.end(),
                            [](char C) { return C == ',' || std::isspace(C); });
    std::string_view lenStr = length.substr(0, End - length.begin());
    if (End == length.end())
      length.remove_prefix(length.size());
    else
      length.remove_prefix((End - length.begin()) + 1);
    CSSUnit lenCss = CSSUnit::parse(lenStr);
    double len = convertCSSWidth(lenCss);
    if (rel)
      cairo_rel_line_to(cairo.get(), len, 0);
    else
      cairo_line_to(cairo.get(), len, Y);
  }
  return true;
}
bool CairoSVGWriter::CairoExecuteVLine(std::string_view length, bool rel) {
  double X, Y;
  cairo_get_current_point(cairo.get(), &X, &Y);
  while (length.size()) {
    auto End = std::find_if(length.begin(), length.end(),
                            [](char C) { return C == ',' || std::isspace(C); });
    std::string_view lenStr = length.substr(0, End - length.begin());
    if (End == length.end())
      length.remove_prefix(length.size());
    else
      length.remove_prefix((End - length.begin()) + 1);
    CSSUnit lenCss = CSSUnit::parse(lenStr);
    double len = convertCSSHeight(lenCss);
    if (rel)
      cairo_rel_line_to(cairo.get(), 0, len);
    else
      cairo_line_to(cairo.get(), X, len);
  }
  return true;
}

bool CairoSVGWriter::CairoExecuteLineTo(std::string_view points, bool rel) {
  while (points.size()) {
    auto xEnd = std::find_if(points.begin(), points.end(), [](char C) {
      return C == ',' || std::isspace(C);
    });
    if (xEnd == points.end())
      return false; // Error: No y value given
    std::string_view xStr = points.substr(0, xEnd - points.begin());
    points.remove_prefix((xEnd - points.begin()) + 1);
    auto yEnd = std::find_if(points.begin(), points.end(),
                             [](char C) { return C == std::isspace(C); });
    std::string_view yStr = points.substr(0, yEnd - points.begin());
    if (yEnd == points.end())
      points.remove_prefix(points.size());
    else
      points.remove_prefix((yEnd - points.begin()) + 1);
    points = strview_trim(points);
    CSSUnit xCss = CSSUnit::parse(xStr);
    CSSUnit yCss = CSSUnit::parse(yStr);
    double x = convertCSSWidth(xCss);
    double y = convertCSSHeight(yCss);
    if (rel)
      cairo_rel_line_to(cairo.get(), x, y);
    else
      cairo_line_to(cairo.get(), x, y);
  }
  return true;
}
bool CairoSVGWriter::CairoExecuteMoveTo(std::string_view points, bool rel) {
  if (points.empty())
    return true;
  auto xEnd = std::find_if(points.begin(), points.end(),
                           [](char C) { return C == ',' || std::isspace(C); });
  if (xEnd == points.end())
    return false; // Error: No y value given
  std::string_view xStr = points.substr(0, xEnd - points.begin());
  points.remove_prefix((xEnd - points.begin()) + 1);
  auto yEnd = std::find_if(points.begin(), points.end(),
                           [](char C) { return C == std::isspace(C); });
  std::string_view yStr = points.substr(0, yEnd - points.begin());
  if (yEnd == points.end())
    points.remove_prefix(points.size());
  else
    points.remove_prefix((yEnd - points.begin()) + 1);
  points = strview_trim(points);
  CSSUnit xCss = CSSUnit::parse(xStr);
  CSSUnit yCss = CSSUnit::parse(yStr);
  double x = convertCSSWidth(xCss);
  double y = convertCSSHeight(yCss);
  if (rel)
    cairo_rel_move_to(cairo.get(), x, y);
  else
    cairo_move_to(cairo.get(), x, y);
  CairoExecuteLineTo(points, rel);
  return true;
}

bool CairoSVGWriter::CairoExecutePath(const char *pathRaw) {
  std::string_view path = strview_trim(pathRaw);
  const char commands[] = "MmLlHhVvCcSsQqTtAaZz";
  // Invariant: There is always non-whitespace content in path and path is
  // trimmed
  while (path.size()) {
    auto cmdpos = path.find_first_of(commands);
    if (cmdpos == path.npos)
      return false; // Error: There was content but no valid command
    char cmd = path[cmdpos];
    path.remove_prefix(cmdpos + 1);
    auto argsEnd = path.find_first_of(commands);
    std::string_view args = path.substr(0, argsEnd);
    args = strview_trim(args);
    if (argsEnd == path.npos)
      path.remove_prefix(path.size());
    else
      path.remove_prefix(argsEnd);
    switch (cmd) {
    case 'M':
      CairoExecuteMoveTo(args, false);
      break;
    case 'm':
      CairoExecuteMoveTo(args, true);
      break;
    case 'L':
      CairoExecuteLineTo(args, false);
      break;
    case 'l':
      CairoExecuteLineTo(args, true);
      break;
    case 'H':
      CairoExecuteHLine(args, false);
      break;
    case 'h':
      CairoExecuteHLine(args, true);
      break;
    case 'V':
      CairoExecuteVLine(args, false);
      break;
    case 'v':
      CairoExecuteVLine(args, true);
      break;
    case 'C':
      break;
    case 'c':
      break;
    case 'S':
      break;
    case 's':
      break;
    case 'Q':
      break;
    case 'q':
      break;
    case 'T':
      break;
    case 't':
      break;
    case 'A':
      break;
    case 'a':
      break;
    case 'Z':
      break;
    case 'z':
      break;
    }
  }
  return true;
}

CairoSVGWriter &
CairoSVGWriter::path(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::path, attrs);
  const SVGAttribute *pathDesc = nullptr;
  struct AttrParser : public SVGAttributeVisitor<AttrParser> {
    AttrParser(const SVGAttribute *&pathDesc) : pathDesc(pathDesc) {}
    void visit_d(const svg::d &desc) { pathDesc = &desc; }
    const SVGAttribute *&pathDesc;
  } attrParser(pathDesc);
  for (const SVGAttribute &Attr : attrs)
    attrParser.visit(Attr);
  if (!pathDesc)
    return *this;
  CairoExecutePath(pathDesc->cstrOrNull());
  applyCSSFillAndStroke(false);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::pattern(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::pattern, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::polygon(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::polygon, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::polyline(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::polyline, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::radialGradient(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::radialGradient, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::rect(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::rect, attrs);
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
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::script(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::script, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::set(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::set, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::solidcolor(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::solidcolor, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::stop(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::stop, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::style(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::style, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::svg(const CairoSVGWriter::AttrContainer &attrs) {
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
  openTag(TagType::svg, attrs);
  cairo_push_group(cairo.get());
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::switch_(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::switch_, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::symbol(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::symbol, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::text(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::text, attrs);
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
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::textPath(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::textPath, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::title(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::title, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::tref(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::tref, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::tspan(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::tspan, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::unknown(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::unknown, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::use(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::use, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::view(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::view, attrs);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::vkern(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::vkern, attrs);
  return *this;
}
