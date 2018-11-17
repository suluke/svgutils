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

// Eventually, I think I should implement
// http://cs.lmu.edu/~ray/notes/xmlgrammar/ . However, starting out with
// something that's 'close enough' is faster and more fun!
using MaybeError = SVGReaderWriterBase::MaybeError;

MaybeError SVGReaderWriterBase::parse(instream_t &is) {
  for (is.get(Tok); !is.eof(); is.get(Tok)) {
    if (std::isspace(Tok))
      continue;
    if (Tok != '<') {
      if (auto err = parseContent(is))
        return err;
    }
    if (is.eof())
      break;
    assert(Tok == '<' && "End of content is either < or eof");
    if (auto err = parseTag(is))
      return err;
  }
  if (parents.size())
    return ParseError("Not all tags were closed");
  // Required because closing tags are only written when strictly
  // necessary to allow for multiple enter()/leave() calls
  writer.finish();
  return ParseSuccess;
}

bool SVGReaderWriterBase::readUntil(instream_t &is,
                                    const std::string_view &delim,
                                    /* out */ std::stringstream &os) {
  if (delim.empty())
    return true;
  if (is.eof())
    return false;
  size_t matchnum = 0;
  for (is.get(Tok); !is.eof(); is.get(Tok)) {
    os << Tok;
    if (delim[matchnum] == Tok) {
      ++matchnum;
      if (matchnum == delim.size())
        return true;
    } else
      matchnum = 0;
  }
  return matchnum == delim.size(); // In case delim is empty and 'is' is eof
}
bool SVGReaderWriterBase::expect(instream_t &is,
                                 const std::string_view &expected) {
  for (size_t i = 0; i < expected.size(); ++i) {
    if (is.eof())
      return false;
    is.get(Tok);
    if (Tok != expected[i])
      return false;
  }
  return true;
}

MaybeError SVGReaderWriterBase::parseContent(instream_t &is) {
  std::stringstream content;
  content << Tok;
  for (is.get(Tok); !is.eof() && Tok != '<'; is.get(Tok))
    content << Tok;
  std::string cstr = content.str();
  if (cstr.empty())
    return ParseSuccess;
  writer.content(cstr.c_str());
  return ParseSuccess;
}

MaybeError SVGReaderWriterBase::parseXMLDecl(instream_t &is) {
  assert(Tok == '?' && "Expected xml declaration to start out with <?");
  std::stringstream content;
  if (!readUntil(is, "?>", content))
    return ParseError("Unexpected end of input after <?");
  assert(Tok == '>');
  return ParseSuccess;
}
MaybeError SVGReaderWriterBase::parseExclTag(instream_t &is) {
  assert(Tok == '!' && "Expected tag to start out with <!");
  assert(!is.eof() && "Expected more content to be readable");
  is.get(Tok);
  std::stringstream content;
  if (Tok == '-') {
    if (!expect(is, "-"))
      return ParseError("Unexpected char or eof after <!-");
    if (!readUntil(is, "-->", content))
      return ParseError("Unexpected end of input inside comment");
    std::string comment = content.str();
    assert(comment.size() >= 3 &&
           "Comment content should contain at least '-->'");
    comment.erase(comment.size() - 3);
    writer.comment(comment.c_str());
  } else if (Tok == 'D') {
    if (!expect(is, "OCTYPE"))
      return ParseError("Expected '<!DOCTYPE' but got something different");
    if (!readUntil(is, ">", content))
      return ParseError("Unexpected end of input inside <!DOCTYPE> tag");
  } else if (Tok == '[') {
    if (!expect(is, "CDATA["))
      return ParseError("Expected '<![CDATA[' but got something different");
    if (!readUntil(is, "]]>", content))
      return ParseError("Unexpected end of input inside <![CDATA[]]> tag");
  } else
    return ParseError("Encountered invalid sequence after <!");
  return ParseSuccess;
}

MaybeError SVGReaderWriterBase::parseTag(instream_t &is) {
  assert(Tok == '<' && "Expected tag to start out with <");
  assert(!is.eof() && "Expected more content to be readable");
  is.get(Tok);
  if (is.eof())
    return ParseError("Unexpected end of input after <");
  if (Tok == '?')
    return parseXMLDecl(is);
  else if (Tok == '!')
    return parseExclTag(is);
  bool isClose = Tok == '/';
  if (isClose)
    is.get(Tok);
  TagType tag = parseTagType(is);
  if (isClose) {
    if (Tok != '>')
      return ParseError("Closing tag should end after name");
    return leave(tag);
  } else if (Tok == '>') {
    dispatchTag(tag, {});
    enter(tag);
  } else if (Tok == '/') {
    is.get(Tok);
    if (Tok != '>') {
      return ParseError("Encountered misplaced /");
    }
    dispatchTag(tag, {});
  } else if (std::isspace(Tok)) {
    std::vector<RawAttr> RawAttrs;
    std::vector<SVGAttribute> Attrs;
    if (auto err = parseAttributes(is, RawAttrs))
      return err;
    if (auto err = convertAttrs(RawAttrs, Attrs))
      return err;
    bool isClosed = Tok == '/';
    if (isClosed) {
      is.get(Tok);
      if (Tok != '>')
        return ParseError("Encountered misplaced /");
    }
    assert(Tok == '>' && "We should be on the tag's closing >");
    dispatchTag(tag, Attrs);
    if (!isClosed)
      enter(tag);
  } else if (is.eof()) {
    return ParseError("Tag cannot end here");
  } else
    svg_unreachable("Somehow ended up with token not in follow-set of tag");
  return ParseSuccess;
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
MaybeError
SVGReaderWriterBase::parseAttributes(instream_t &is,
                                     /*out*/ std::vector<RawAttr> &attrs) {
  for (; !is.eof() && Tok != '>' && Tok != '/'; is.get(Tok)) {
    if (std::isspace(Tok))
      continue;
    std::string name = parseName(is);
    if (Tok != '=')
      return ParseError("Attribute has no value");
    is.get(Tok);
    std::string value;
    if (auto err = parseAttrValue(is, value))
      return err;
    attrs.emplace_back();
    attrs.back().name = std::move(name);
    attrs.back().value = std::move(value);
  }
  if (is.eof() && Tok != '>' && Tok != '/')
    return ParseError("Unexpected end of input");
  return ParseSuccess;
}

MaybeError SVGReaderWriterBase::parseAttrValue(instream_t &is,
                                               /* out */ std::string &val) {
  char Delim = 0;
  if (Tok == '"' || Tok == '\'')
    Delim = Tok;
  if (!Delim)
    return ParseError("Attribute value not starting with ' or \"");
  std::stringstream value;
  for (is.get(Tok); !is.eof() && Tok != Delim; is.get(Tok))
    value << Tok; // FIXME check for illegal characters
  if (is.eof() && Tok != Delim)
    return ParseError("Attribute value not starting with ' or \"");
  val = value.str();
  return ParseSuccess;
}
std::string SVGReaderWriterBase::parseName(instream_t &is) {
  std::stringstream namestr;
  for (; !is.eof() && Tok != '>' && Tok != '/' && Tok != '=' &&
         !std::isspace(Tok);
       is.get(Tok))
    namestr << Tok;
  return namestr.str();
}

MaybeError
SVGReaderWriterBase::convertAttrs(const std::vector<RawAttr> &raws,
                                  /* out */ std::vector<SVGAttribute> &attrs) {
  assert(attrs.empty() && "Expected output vector to be empty");
  for (const RawAttr &raw : raws) {
#define SVG_ATTR(NAME, STR, DEFAULT)                                           \
  if (raw.name == STR) {                                                       \
    attrs.emplace_back(NAME(raw.value.c_str()));                               \
    continue;                                                                  \
  }
#include "svgutils/svg_entities.def"
    return ParseError("Unknown attribute");
  }
  return ParseSuccess;
}
