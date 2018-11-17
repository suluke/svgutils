#ifndef SVGUTILS_SVG_READER_WRITER_H
#define SVGUTILS_SVG_READER_WRITER_H

#include "svg_writer.h"

namespace svg {
using instream_t = std::istream;

struct SVGReaderWriterBase {
  struct ParseError {
    ParseError(std::string what) : what(std::move(what)){};
    ParseError(const char *what) : /* I said */ what(what){};
    ParseError(ParseError &&) = default;
    ParseError &operator=(ParseError &&) = default;
    ParseError(const ParseError &) = default;
    ParseError &operator=(const ParseError &) = default;
    friend inline outstream_t &operator<<(outstream_t &os,
                                          const ParseError &err) {
      os << err.what;
      return os;
    }
    std::string what;
  };
  using MaybeError = std::optional<ParseError>;

  SVGReaderWriterBase(WriterConcept &writer) : writer(writer) {}
  MaybeError parse(instream_t &is);

private:
  static constexpr std::nullopt_t ParseSuccess = std::nullopt;
  struct RawAttr {
    std::string name;
    std::string value;
  };
  WriterConcept &writer;
  char Tok;

  enum class TagType;
  std::stack<TagType> parents;

  bool readUntil(instream_t &is, const std::string_view &delim,
                 /* out */ std::stringstream &os);
  bool expect(instream_t &is, const std::string_view &expected);

  MaybeError parseContent(instream_t &is);
  MaybeError parseXMLDecl(instream_t &is);
  MaybeError parseExclTag(instream_t &is);
  MaybeError parseTag(instream_t &is);
  TagType parseTagType(instream_t &is);
  void dispatchTag(TagType tag, const std::vector<SVGAttribute> &attrs);
  std::string parseName(instream_t &is);
  MaybeError parseAttributes(instream_t &is,
                             /*out*/ std::vector<RawAttr> &attrs);
  static MaybeError convertAttrs(const std::vector<RawAttr> &raw,
                                 /* out */ std::vector<SVGAttribute> &attrs);
  MaybeError parseAttrValue(instream_t &is, /* out */ std::string &val);
  void enter(TagType tag) {
    parents.push(tag);
    writer.enter();
  }
  MaybeError leave(TagType tag) {
    TagType expected = parents.top();
    if (tag != expected)
      return ParseError{
          "Encountered closing tag that has not been opened before"};
    parents.pop();
    writer.leave();
    return ParseSuccess;
  }
};

template <typename WriterTy>
struct SVGReaderWriter : public SVGReaderWriterBase {
  using base_t = SVGReaderWriterBase;
  template <typename... args_t>
  SVGReaderWriter(args_t &&... args)
      : base_t(writer), writer(std::forward<args_t>(args)...) {}

  WriterTy &getWriter() { return writer.getWriter(); }

private:
  WriterModel<WriterTy> writer;
};
} // namespace svg
#endif // SVGUTILS_SVG_READER_WRITER_H
