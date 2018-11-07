#include "svgutils/svg_reader_writer.h"

#include <cctype>
#include <sstream>

using namespace svg;

enum class SVGReaderWriterBase::TagType {
  NONE = 0,
  COMMENT,
  CDATA,
  XMLDECL,
  DOCTYPE,
#define SVG_TAG(NAME, STR, ...) NAME,
#include "svgutils/svg_entities.def"
};

#define PARSE_ERROR(ERR) return false
#define PARSE_SUCCESS true

// Eventually, I think I should implement
// http://cs.lmu.edu/~ray/notes/xmlgrammar/ . However, starting out with
// something that's 'close enough' is faster and more fun!

bool SVGReaderWriterBase::parse(instream_t &is) {
  for (is.get(Tok); !is.eof(); is.get(Tok)) {
    if (std::isspace(Tok))
      continue;
    if (Tok != '<') {
      if (auto res = parseContent(is); !res)
        PARSE_ERROR(res);
    }
    if (is.eof())
      break;
    assert(Tok == '<' && "End of content is either < or eof");
    if (auto res = parseTag(is); !res)
      PARSE_ERROR(res);
  }
  if (parents.size())
    PARSE_ERROR("Not all tags were closed");
  // Required because closing tags are only written when strictly
  // necessary to allow for multiple enter()/leave() calls
  writer.finish();
  return PARSE_SUCCESS;
}

bool SVGReaderWriterBase::parseContent(instream_t &is) {
  std::stringstream content;
  content << Tok;
  for (is.get(Tok); !is.eof() && Tok != '<'; is.get(Tok))
    content << Tok;
  std::string cstr = content.str();
  if (cstr.empty())
    return PARSE_SUCCESS;
  writer.content(cstr.c_str());
  return PARSE_SUCCESS;
}
bool SVGReaderWriterBase::parseTag(instream_t &is) {
  assert(Tok == '<' && "Expected tag to start out with <");
  is.get(Tok);
  bool isClose = Tok == '/';
  if (isClose)
    is.get(Tok);
  TagType tag = parseTagType(is);
  if (isClose) {
    if (Tok != '>')
      PARSE_ERROR("Closing tag should end after name");
    return leave(tag);
  } else if (Tok == '>') {
    dispatchTag(tag, {});
    enter(tag);
  } else if (Tok == '/') {
    is.get(Tok);
    if (Tok != '>') {
      PARSE_ERROR("Encountered misplaced /");
    }
    dispatchTag(tag, {});
  } else if (std::isspace(Tok)) {
    std::vector<RawAttr> RawAttrs;
    std::vector<SVGAttribute> Attrs;
    if (auto res = parseAttributes(is, RawAttrs); !res)
      PARSE_ERROR(res);
    if (auto res = convertAttrs(RawAttrs, Attrs); !res)
      PARSE_ERROR(res);
    bool isClosed = Tok == '/';
    if (isClosed) {
      is.get(Tok);
      if (Tok != '>')
        PARSE_ERROR("Encountered misplaced /");
    }
    assert(Tok == '>' && "We should be on the tag's closing >");
    dispatchTag(tag, Attrs);
    if (!isClosed)
      enter(tag);
  } else if (is.eof()) {
    PARSE_ERROR("Tag cannot end here");
  } else
    svg_unreachable("Somehow ended up with token not in follow-set of tag");
  return PARSE_SUCCESS;
}

SVGReaderWriterBase::TagType SVGReaderWriterBase::parseTagType(instream_t &is) {
  std::string name = parseName(is);
#define SVG_TAG(NAME, STR, ...)                                                \
  if (name == STR)                                                             \
    return TagType::NAME;
#include "svgutils/svg_entities.def"
  return TagType::NONE;
}
void SVGReaderWriterBase::dispatchTag(SVGReaderWriterBase::TagType tag,
                                      const std::vector<SVGAttribute> &attrs) {
  assert(tag != TagType::NONE && "Cannot dispatch for unknown tag");
  switch (tag) {
#define SVG_TAG(NAME, STR, ...)                                                \
  case TagType::NAME:                                                          \
    writer.NAME(attrs);                                                        \
    break;
#include "svgutils/svg_entities.def"
  case TagType::NONE:
  case TagType::COMMENT:
  case TagType::CDATA:
  case TagType::XMLDECL:
  case TagType::DOCTYPE:
    svg_unreachable("Cannot dispatch for special or unknown tag");
  };
}
bool SVGReaderWriterBase::parseAttributes(instream_t &is,
                                          /*out*/ std::vector<RawAttr> &attrs) {
  for (; !is.eof() && Tok != '>' && Tok != '/'; is.get(Tok)) {
    if (std::isspace(Tok))
      continue;
    std::string name = parseName(is);
    if (Tok != '=')
      PARSE_ERROR("Attribute has no value");
    is.get(Tok);
    std::string value;
    if (auto res = parseAttrValue(is, value); !res)
      PARSE_ERROR(res);
    attrs.emplace_back();
    attrs.back().name = std::move(name);
    attrs.back().value = std::move(value);
  }
  if (is.eof() && Tok != '>' && Tok != '/')
    PARSE_ERROR("Unexpected end of input");
  return PARSE_SUCCESS;
}

bool SVGReaderWriterBase::parseAttrValue(instream_t &is,
                                         /* out */ std::string &val) {
  char Delim = 0;
  if (Tok == '"' || Tok == '\'')
    Delim = Tok;
  if (!Delim)
    PARSE_ERROR("Attribute value not starting with ' or \"");
  std::stringstream value;
  for (is.get(Tok); !is.eof() && Tok != Delim; is.get(Tok))
    value << Tok; // FIXME check for illegal characters
  if (is.eof() && Tok != Delim)
    PARSE_ERROR("Attribute value not starting with ' or \"");
  val = value.str();
  return PARSE_SUCCESS;
}
std::string SVGReaderWriterBase::parseName(instream_t &is) {
  std::stringstream namestr;
  for (; !is.eof() && Tok != '>' && Tok != '/' && Tok != '=' &&
         !std::isspace(Tok);
       is.get(Tok))
    namestr << Tok;
  return namestr.str();
}

bool SVGReaderWriterBase::convertAttrs(
    const std::vector<RawAttr> &raws,
    /* out */ std::vector<SVGAttribute> &attrs) {
  assert(attrs.empty() && "Expected output vector to be empty");
  for (const RawAttr &raw : raws) {
#define SVG_ATTR(NAME, STR, DEFAULT)                                           \
  if (raw.name == STR) {                                                       \
    attrs.emplace_back(NAME(raw.value.c_str()));                               \
    continue;                                                                  \
  }
#include "svgutils/svg_entities.def"
    PARSE_ERROR("Unknown attribute");
  }
  return PARSE_SUCCESS;
}
