#ifndef SVGUTILS_JSWRITER_H
#define SVGUTILS_JSWRITER_H

#include "svg_writer.h"

namespace svg {

struct SVGJSWriter : public svg::SVGWriterBase<SVGJSWriter> {
  using self_t = SVGJSWriter;
  using base_t = svg::SVGWriterBase<self_t>;
  using RetTy = SVGWriterErrorOr<SVGJSWriter *>;
  using outstream_t = svg::outstream_t;
  SVGJSWriter(outstream_t &outstream,
              const char *CreatedTagsListName = "rootTags")
      : base_t(outstream) {
    writeJSLine("if (typeof SVGWriterState === 'undefined')");
    ++indent;
    writeJSLine("var SVGWriterState = {xmlns: 'http://www.w3.org/2000/svg', "
                "parentTags: [], currentTag: null, rootTags: null};");
    --indent;
    writeIndent();
    output() << "SVGWriterState.rootTags = " << CreatedTagsListName << ";\n";
  }
  RetTy enter() {
    writeJSLine("if (SVGWriterState.currentTag === null)");
    ++indent;
    writeJSLine("throw new Error('No current tag: Cannot enter');");
    --indent;
    writeJSLine("SVGWriterState.parentTags.push(SVGWriterState.currentTag);");
    writeJSLine("SVGWriterState.currentTag = null;");
    return this;
  }
  RetTy leave() {
    writeJSLine("if (SVGWriterState.parentTags.length === 0)");
    ++indent;
    writeJSLine("throw new Error('No parent tags: Cannot leave');");
    --indent;
    writeJSLine("SVGWriterState.currentTag = SVGWriterState.parentTags.pop();");
    return this;
  }
  RetTy finish() {
    --indent;
    return this;
  }
  RetTy content(const char *text) {
    writeIndent();
    output() << "SVGWriterState.parentTags[SVGWriterState.parentTags.length - "
                "1].innerHTML = '"
             << text << "';\n";
    return this;
  }

private:
  friend base_t;
  template <typename container_t>
  void openTag(const char *tagname, container_t attrs) {
    writeIndent();
    output() << "SVGWriterState.currentTag = "
                "document.createElementNS(SVGWriterState.xmlns, '"
             << tagname << "');\n";
    for (const svg::SVGAttribute &attr : attrs) {
      if (attr.getName() == svg::xmlns().getName())
        continue;
      writeIndent();
      output() << "SVGWriterState.currentTag.setAttributeNS(null, '"
               << attr.getName() << "', `";
      attr.writeValue(output());
      output() << "`);\n";
    }
    writeJSLine("if (SVGWriterState.parentTags.length > 0)");
    ++indent;
    writeJSLine("SVGWriterState.parentTags[SVGWriterState.parentTags.length - "
                "1].appendChild(SVGWriterState.currentTag);");
    --indent;
    writeJSLine("else");
    ++indent;
    writeJSLine("SVGWriterState.rootTags.push(SVGWriterState.currentTag);");
    --indent;
  }
  void closeTag() {
    // nothing to do
  }
  outstream_t &output() { return this->base_t::output(); }
  static inline outstream_t &repeat(outstream_t &os, char c, size_t count) {
    for (size_t i = 0; i < count; ++i)
      os << c;
    return os;
  }
  void writeIndent() { repeat(output(), indentChar, indentWidth * indent); }
  void writeJSLine(const char *code) {
    writeIndent();
    output() << code << "\n";
  }

  char indentChar = ' ';
  size_t indentWidth = 2;
  size_t indent = 0;
};
} // namespace svg
#endif // SVGUTILS_JSWRITER_H
