#include "svgcairo/svg_cairo.h"

using namespace svg;

CairoSVGWriter::CairoSVGWriter(const fs::path &outfile, double width,
                               double height)
    : outfile(outfile),
      surface(cairo_pdf_surface_create(this->outfile.c_str(), width, height),
              cairo_surface_destroy),
      cairo(cairo_create(surface.get()), cairo_destroy) {}

CairoSVGWriter &CairoSVGWriter::content(const char *text) { return *this; }
CairoSVGWriter &CairoSVGWriter::enter() { return *this; }
CairoSVGWriter &CairoSVGWriter::leave() { return *this; }
CairoSVGWriter &CairoSVGWriter::finish() { return *this; }

CairoSVGWriter &CairoSVGWriter::a(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::altGlyph(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::altGlyphDef(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::altGlyphItem(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::animate(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::animateColor(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::animateMotion(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::animateTransform(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::circle(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::clipPath(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::color_profile(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::cursor(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::defs(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::desc(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::discard(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::ellipse(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feBlend(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feColorMatrix(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &CairoSVGWriter::feComponentTransfer(
    const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feComposite(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feConvolveMatrix(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feDiffuseLighting(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feDisplacementMap(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feDistantLight(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feDropShadow(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feFlood(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feFuncA(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feFuncB(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feFuncG(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feFuncR(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feGaussianBlur(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feImage(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feMerge(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feMergeNode(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feMorphology(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feOffset(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::fePointLight(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feSpecularLighting(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feSpotLight(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feTile(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::feTurbulence(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::filter(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::font(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::font_face(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::font_face_format(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::font_face_name(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::font_face_src(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::font_face_uri(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::foreignObject(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &CairoSVGWriter::g(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::glyph(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::glyphRef(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::hatch(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::hatchpath(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::hkern(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::image(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::line(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::linearGradient(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::marker(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::mask(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::mesh(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::meshgradient(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::meshpatch(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::meshrow(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::metadata(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::missing_glyph(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::mpath(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::path(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::pattern(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::polygon(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::polyline(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::radialGradient(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::rect(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::script(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::set(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::solidcolor(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::stop(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::style(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::svg(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::switch_(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::symbol(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::text(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::textPath(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::title(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::tref(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::tspan(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::unknown(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::use(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::view(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
CairoSVGWriter &
CairoSVGWriter::vkern(const CairoSVGWriter::AttrContainer &attrs) {
  return *this;
}
