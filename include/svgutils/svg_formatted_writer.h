#ifndef SVGUTILS_SVG_FORMATTED_WRITER_H
#define SVGUTILS_SVG_FORMATTED_WRITER_H

#include "svgutils/svg_writer.h"

namespace svg {
/// SVG document writer with several formating features.
struct SVGFormattedWriter : public SVGWriterBase<SVGFormattedWriter> {
  using self_t = SVGFormattedWriter;
  using base_t = SVGWriterBase<self_t>;
  using RetTy = SVGWriterErrorOr<self_t *>;

  SVGFormattedWriter(outstream_t &os) : base_t(os) {}
  SVGFormattedWriter(outstream_t &os, char indentChar, size_t indentWidth)
      : base_t(os), indentChar(indentChar), indentWidth(indentWidth) {}

  RetTy enter() {
    base_t::enter();
    base_t::output() << "\n";
    ++indent;
    wasEntered.top() = true;
    return this;
  }
  RetTy leave() {
    base_t::leave();
    --indent;
    return this;
  }
  RetTy content(const char *text) {
    closeTag();
    writeIndent();
    base_t::output() << text << "\n";
    return this;
  }
  RetTy comment(const char *comment) {
    closeTag();
    writeIndent();
    output() << "<!--\n";
    writeIndent();
    std::string_view trimmed = strview_trim(comment);
    output() << trimmed << '\n';
    writeIndent();
    output() << "-->\n";
    return this;
  }

private:
  static inline outstream_t &repeat(outstream_t &os, char c, size_t count) {
    for (size_t i = 0; i < count; ++i)
      os << c;
    return os;
  }
  void writeIndent() {
    repeat(base_t::output(), indentChar, indentWidth * indent);
  }

  friend base_t;
  template <typename container_t>
  void openTag(const char *tagname, container_t attrs) {
    closeTag();
    writeIndent();
    base_t::output() << "<" << tagname;
    base_t::writeAttrs(attrs);
    base_t::output() << ">";
    base_t::currentTag = tagname;
    wasEntered.push(false);
  }
  void closeTag() {
    if (base_t::currentTag) {
      if (wasEntered.top())
        writeIndent();
      base_t::output() << "</" << base_t::currentTag << ">";
      base_t::currentTag = nullptr;
      base_t::output() << "\n";
      wasEntered.pop();
    }
  }

  char indentChar = ' ';
  size_t indentWidth = 2;
  size_t indent = 0;
  std::stack<bool> wasEntered;
};
} // namespace svg
#endif // SVGUTILS_SVG_FORMATTED_WRITER_H
