#ifndef SVGUTILS_PLOTLIB_H
#define SVGUTILS_PLOTLIB_H

#include "svg_utils.h"

namespace svg {

template <typename WriterTy>
struct PlotWriter : public ExtendableWriter<PlotWriter<WriterTy>> {
  using base_t = ExtendableWriter<PlotWriter<WriterTy>>;

  PlotWriter(svg::outstream_t &os)
      : base_t(std::make_unique<WriterModel<WriterTy>>(os)) {}

  template <typename... attrs_t>
  PlotWriter &grid(double top, double left, double width, double height,
                   double distx, double disty, attrs_t... attrs) {
    auto &self = static_cast<base_t &>(*this);
    for (double i = top; i <= top + height; i += disty)
      self.line(x1(left), y1(i), x2(left + width), y2(i), attrs...);
    for (double i = left; i < left + width; i += distx)
      self.line(x1(i), y1(top), x2(i), y2(top + height), attrs...);
    return *this;
  }
};

} // namespace svg
#endif // SVGUTILS_PLOTLIB_H
