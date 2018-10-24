#include "svgcairo/svg_cairo.h"
#include <cmath>

using namespace svg;

enum class CairoSVGWriter::TagType {
  NONE = 0,
#define SVG_TAG(NAME, STR, ...) NAME,
#include "svgutils/svg_entities.def"
};

CairoSVGWriter::CairoSVGWriter(const fs::path &outfile, double width,
                               double height)
    : outfile(outfile),
      surface(cairo_pdf_surface_create(this->outfile.c_str(), width, height),
              cairo_surface_destroy),
      cairo(cairo_create(surface.get()), cairo_destroy) {}

CairoSVGWriter &CairoSVGWriter::content(const char *text) { return *this; }
CairoSVGWriter &CairoSVGWriter::enter() {
  assert(currentTag && "Cannot enter without root tag");
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
      cairo_pop_group(cairo.get());
      break;
    default:
      break;
    }
    currentTag = TagType::NONE;
  }
}

CairoSVGWriter &CairoSVGWriter::a(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::a;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::altGlyph(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::altGlyph;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::altGlyphDef(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::altGlyphDef;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::altGlyphItem(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::altGlyphItem;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::animate(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::animate;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::animateColor(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::animateColor;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::animateMotion(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::animateMotion;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::animateTransform(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::animateTransform;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::circle(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::circle;
  double cx = 0., cy = 0., r = 0.;
  struct AttrParser : public SVGAttributeVisitor<AttrParser> {
    AttrParser(double &cx, double &cy, double &r) : cx(cx), cy(cy), r(r) {}
    void visit_cx(const svg::cx &x) { cx = x.toDouble(); }
    void visit_cy(const svg::cy &y) { cy = y.toDouble(); }
    void visit_r(const svg::r &radius) { r = radius.toDouble(); }
    double &cx;
    double &cy;
    double &r;
  } attrParser(cx, cy, r);
  for (const SVGAttribute &Attr : attrs)
    attrParser.visit(Attr);
  cairo_arc(cairo.get(), cx, cy, r, 0., 2 * M_PI);
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::clipPath(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::clipPath;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::color_profile(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::color_profile;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::cursor(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::cursor;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::defs(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::defs;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::desc(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::desc;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::discard(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::discard;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::ellipse(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::ellipse;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feBlend(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::feBlend;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feColorMatrix(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::feColorMatrix;
  return *this;
}
CairoSVGWriter &CairoSVGWriter::feComponentTransfer(
    const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::feComponentTransfer;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feComposite(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::feComposite;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feConvolveMatrix(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::feConvolveMatrix;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feDiffuseLighting(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::feDiffuseLighting;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feDisplacementMap(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::feDisplacementMap;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feDistantLight(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::feDistantLight;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feDropShadow(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::feDropShadow;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feFlood(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::feFlood;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feFuncA(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::feFuncA;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feFuncB(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::feFuncB;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feFuncG(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::feFuncG;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feFuncR(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::feFuncR;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feGaussianBlur(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::feGaussianBlur;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feImage(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::feImage;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feMerge(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::feMerge;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feMergeNode(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::feMergeNode;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feMorphology(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::feMorphology;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feOffset(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::feOffset;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::fePointLight(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::fePointLight;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feSpecularLighting(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::feSpecularLighting;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feSpotLight(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::feSpotLight;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feTile(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::feTile;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feTurbulence(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::feTurbulence;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::filter(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::filter;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::font(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::font;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::font_face(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::font_face;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::font_face_format(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::font_face_format;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::font_face_name(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::font_face_name;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::font_face_src(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::font_face_src;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::font_face_uri(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::font_face_uri;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::foreignObject(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::foreignObject;
  return *this;
}
CairoSVGWriter &CairoSVGWriter::g(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::g;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::glyph(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::glyph;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::glyphRef(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::glyphRef;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::hatch(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::hatch;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::hatchpath(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::hatchpath;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::hkern(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::hkern;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::image(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::image;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::line(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::line;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::linearGradient(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::linearGradient;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::marker(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::marker;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::mask(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::mask;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::mesh(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::mesh;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::meshgradient(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::meshgradient;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::meshpatch(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::meshpatch;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::meshrow(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::meshrow;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::metadata(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::metadata;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::missing_glyph(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::missing_glyph;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::mpath(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::mpath;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::path(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::path;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::pattern(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::pattern;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::polygon(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::polygon;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::polyline(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::polyline;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::radialGradient(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::radialGradient;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::rect(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::rect;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::script(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::script;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::set(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::set;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::solidcolor(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::solidcolor;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::stop(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::stop;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::style(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::style;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::svg(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::svg;
  cairo_push_group(cairo.get());
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::switch_(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::switch_;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::symbol(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::symbol;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::text(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::text;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::textPath(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::textPath;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::title(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::title;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::tref(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::tref;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::tspan(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::tspan;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::unknown(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::unknown;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::use(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::use;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::view(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::view;
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::vkern(const CairoSVGWriter::AttrContainer &attrs) {
  closeTag();
  currentTag = TagType::vkern;
  return *this;
}
