#include "svgcairo/svg_cairo.h"
#include <cmath>

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

CairoSVGWriter::CairoSVGWriter(const fs::path &outfile, double width,
                               double height)
    : currentTag(TagType::NONE),
      fonts(/*FIXME proper checking of optional*/ *Freetype::Create()),
      surface(cairo_pdf_surface_create(outfile.c_str(), width / 1.25,
                                       height / 1.25),
              cairo_surface_destroy),
      cairo(cairo_create(surface.get()), cairo_deleter), width(width / 1.25),
      height(height / 1.25) {
  assert(cairo_surface_status(surface.get()) == CAIRO_STATUS_SUCCESS &&
         "Error initializing cairo pdf surface");
  assert(cairo_status(cairo.get()) == CAIRO_STATUS_SUCCESS &&
         "Error initializing cairo context");
}

CairoSVGWriter &CairoSVGWriter::content(const char *text) {
  closeTag();
  assert(parents.size() && parents.top() == TagType::text &&
         "Content is only supported in text nodes at the moment");
  // Collect the style information
  CSSUnit cssSize = styles.getFontSize();
  double fontSize = convertCSSWidth(cssSize);
  CSSColor color = styles.getColor();
  std::string fontPattern(styles.getFontFamily());

  FT_Face ftFont = fonts.getFace(fontPattern.c_str());
  if (!ftFont)
    unreachable("Error loading font");

  cairo_save(cairo.get());

  cairo_font_face_t *cairoFont =
      cairo_ft_font_face_create_for_ft_face(ftFont, 0);
  cairo_set_font_face(cairo.get(), cairoFont);
  cairo_set_font_size(cairo.get(), fontSize);
  cairo_set_source_rgba(cairo.get(), color.r, color.g, color.b, color.a);
  cairo_move_to(cairo.get(), 80, 95);

  cairo_show_text(cairo.get(), text);

  cairo_restore(cairo.get());
  cairo_set_font_face(cairo.get(), nullptr);
  cairo_font_face_destroy(cairoFont);
  return *this;
  // FIXME: Don't use the toy api
  // Reference:
  // https://github.com/Distrotech/cairo/blob/17ef4acfcb64d1c525910a200e60d63087953c4c/src/cairo.c#L3197
}
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
  cairo_show_page(cairo.get());
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
  closeTag();
  currentTag = T;
  styles.push(attrs);
}

static double convertCSSLength(const CSSUnit &unit) {
  double len = 0;
  if (unit.unit == CSSUnit::PX)
    len = unit.length;
  else if (unit.unit == CSSUnit::PT)
    len = unit.length * 1.25;
  else if (unit.unit == CSSUnit::PC)
    len = unit.length * 15.;
  else if (unit.unit == CSSUnit::MM)
    len = unit.length * 3.543307;
  else if (unit.unit == CSSUnit::CM)
    len = unit.length * 35.43307;
  else if (unit.unit == CSSUnit::IN)
    len = unit.length * 90.;
  else
    unreachable("Encountered unexpected css unit");
  // cairo units are pt (= 1/72in)
  return len / 1.25;
}

double CairoSVGWriter::convertCSSWidth(const CSSUnit &unit) const {
  if (unit.unit == CSSUnit::PERCENT)
    return unit.length / 100. * width;
  return convertCSSLength(unit);
}
double CairoSVGWriter::convertCSSHeight(const CSSUnit &unit) const {
  if (unit.unit == CSSUnit::PERCENT)
    return unit.length / 100. * height;
  return convertCSSLength(unit);
}

/// Utility function to extract a CSS unit from the value of an
/// SVGAttribute
static CSSUnit extractUnitFrom(const SVGAttribute &attr) {
  if (const char *cstr = attr.cstrOrNull())
    return CSSUnit::parse(cstr);
  CSSUnit res;
  res.length = attr.toDouble();
  return res;
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
    void visit_cx(const svg::cx &x) { cx = extractUnitFrom(x); }
    void visit_cy(const svg::cy &y) { cy = extractUnitFrom(y); }
    void visit_r(const svg::r &radius) { r = extractUnitFrom(radius); }
    CSSUnit &cx;
    CSSUnit &cy;
    CSSUnit &r;
  } attrParser(cx, cy, r);
  for (const SVGAttribute &Attr : attrs)
    attrParser.visit(Attr);
  cairo_arc(cairo.get(), convertCSSWidth(cx), convertCSSHeight(cy),
            convertCSSWidth(r), 0., 2 * M_PI);
  CSSColor bg = styles.getFillColor();
  cairo_set_source_rgba(cairo.get(), bg.r, bg.g, bg.b, bg.a);
  cairo_fill(cairo.get());
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
    void visit_x1(const svg::x1 &x) { x1 = extractUnitFrom(x); }
    void visit_y1(const svg::y1 &y) { y1 = extractUnitFrom(y); }
    void visit_x2(const svg::x2 &x) { x2 = extractUnitFrom(x); }
    void visit_y2(const svg::y2 &y) { y2 = extractUnitFrom(y); }
    CSSUnit &x1;
    CSSUnit &y1;
    CSSUnit &x2;
    CSSUnit &y2;
  } attrParser(x1, y1, x2, y2);
  for (const SVGAttribute &Attr : attrs)
    attrParser.visit(Attr);
  const CSSColor color = styles.getStrokeColor();
  const CSSUnit strokeWidth = styles.getStrokeWidth();
  const CSSDashArray strokeDasharray = styles.getStrokeDasharray();
  std::vector<double> dashes;
  for (const CSSUnit &len : strokeDasharray.dashes)
    dashes.emplace_back(convertCSSWidth(len));
  cairo_set_source_rgba(cairo.get(), color.r, color.g, color.b, color.a);
  // FIXME look up what percentages mean in stroke-width
  cairo_set_dash(cairo.get(), dashes.data(), dashes.size(), 0.);
  cairo_set_line_width(cairo.get(), convertCSSWidth(strokeWidth));
  cairo_move_to(cairo.get(), convertCSSWidth(x1), convertCSSHeight(y1));
  cairo_line_to(cairo.get(), convertCSSWidth(x2), convertCSSHeight(y2));
  cairo_stroke(cairo.get());
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
CairoSVGWriter &
CairoSVGWriter::path(const CairoSVGWriter::AttrContainer &attrs) {
  openTag(TagType::path, attrs);
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
  CSSUnit x, y, width, height;
  struct AttrParser : public SVGAttributeVisitor<AttrParser> {
    AttrParser(CSSUnit &x, CSSUnit &y, CSSUnit &width, CSSUnit &height)
        : x(x), y(y), width(width), height(height) {}
    void visit_x(const svg::x &xAttr) { x = extractUnitFrom(xAttr); }
    void visit_y(const svg::y &yAttr) { y = extractUnitFrom(yAttr); }
    void visit_width(const svg::width &w) { width = extractUnitFrom(w); }
    void visit_height(const svg::height &h) { height = extractUnitFrom(h); }
    CSSUnit &x;
    CSSUnit &y;
    CSSUnit &width;
    CSSUnit &height;
  } attrParser(x, y, width, height);
  for (const SVGAttribute &Attr : attrs)
    attrParser.visit(Attr);
  cairo_rectangle(cairo.get(), convertCSSWidth(x), convertCSSHeight(y),
                  convertCSSWidth(width), convertCSSHeight(height));
  const CSSColor bg = styles.getFillColor();
  cairo_set_source_rgba(cairo.get(), bg.r, bg.g, bg.b, bg.a);
  cairo_fill(cairo.get());
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
