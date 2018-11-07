#ifndef SVGUTILS_SVG_READER_WRITER_H
#define SVGUTILS_SVG_READER_WRITER_H

#include "svg_writer.h"

namespace svg {
using instream_t = std::istream;

struct SVGReaderWriterBase {
  SVGReaderWriterBase(WriterConcept &writer) : writer(writer) {}
  bool parse(instream_t &is);

private:
  struct RawAttr {
    std::string name;
    std::string value;
  };
  WriterConcept &writer;
  char Tok;

  enum class TagType;
  std::stack<TagType> parents;

  bool parseContent(instream_t &is);
  bool parseTag(instream_t &is);
  TagType parseTagType(instream_t &is);
  void dispatchTag(TagType tag, const std::vector<SVGAttribute> &attrs);
  std::string parseName(instream_t &is);
  bool parseAttributes(instream_t &is, /*out*/ std::vector<RawAttr> &attrs);
  static bool convertAttrs(const std::vector<RawAttr> &raw,
                           /* out */ std::vector<SVGAttribute> &attrs);
  bool parseAttrValue(instream_t &is, /* out */ std::string &val);
  void enter(TagType tag) {
    parents.push(tag);
    writer.enter();
  }
  bool leave(TagType tag) {
    TagType expected = parents.top();
    if (tag != expected)
      return false;
    parents.pop();
    writer.leave();
    return true;
  }
};

template <typename WriterTy>
struct SVGReaderWriter : public SVGReaderWriterBase {
  using base_t = SVGReaderWriterBase;
  template <typename... args_t>
  SVGReaderWriter(args_t &&... args)
      : base_t(writer), writer(std::forward<args_t>(args)...) {}

private:
  WriterModel<WriterTy> writer;
};
} // namespace svg
#endif // SVGUTILS_SVG_READER_WRITER_H
