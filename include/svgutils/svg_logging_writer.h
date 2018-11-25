#ifndef SVGUTILS_SVG_LOGGING_WRITER_H
#define SVGUTILS_SVG_LOGGING_WRITER_H

#include "svgutils/svg_writer.h"

namespace svg {

struct SVGDummyWriter : public SVGWriterBase<SVGDummyWriter> {
  using base_t = SVGWriterBase<SVGDummyWriter>;
  using RetTy = SVGWriterErrorOr<SVGDummyWriter *>;
  SVGDummyWriter() : base_t(/* could be anything */ std::cout) {}

  RetTy content(const char *text) {
    closeTag();
    return this;
  }
  RetTy comment(const char *comment) {
    closeTag();
    return this;
  }

private:
  friend base_t;
  template <typename container_t>
  void openTag(const char *tagname, const container_t &attrs) {
    closeTag();
    base_t::currentTag = tagname;
  }
  void closeTag() {
    if (base_t::currentTag) {
      base_t::currentTag = nullptr;
    }
  }
};

template <typename WrappedTy> struct SVGLoggingWriter {
  using self_t = SVGLoggingWriter<WrappedTy>;
  using RetTy = SVGWriterErrorOr<SVGLoggingWriter *>;
  template <typename... args_t>
  SVGLoggingWriter(outstream_t &os, args_t &&... args)
      : os(&os), writer(std::forward<args_t>(args)...) {}
  SVGLoggingWriter(self_t &&) = default;
  self_t &operator=(self_t &&) = default;

#define SVG_TAG(NAME, STR, ...)                                                \
  template <typename... attrs_t> RetTy NAME(attrs_t... attrs) {                \
    std::vector<SVGAttribute> attrsVec({std::forward<attrs_t>(attrs)...});     \
    return NAME(attrsVec);                                                     \
  }                                                                            \
  template <typename container_t> RetTy NAME(const container_t &attrs) {       \
    log(#NAME, &attrs);                                                        \
    return writer.NAME(attrs).with_value(this);                                \
  }
#include "svgutils/svg_entities.def"

  template <typename... attrs_t>
  RetTy custom_tag(const char *tagname, attrs_t... attrs) {
    std::vector<SVGAttribute> attrsVec({std::forward<attrs_t>(attrs)...});
    return custom_tag(tagname, attrsVec);
  }
  template <typename container_t>
  RetTy custom_tag(const char *tagname, const container_t &attrs) {
    log(tagname, &attrs);
    return writer.custom_tag(tagname, attrs).with_value(this);
  }

  RetTy comment(const char *comment) {
    log<std::vector<SVGAttribute>>("comment", nullptr, comment);
    return writer.comment(comment).with_value(this);
  }
  RetTy content(const char *text) {
    log<std::vector<SVGAttribute>>("content", nullptr, text);
    return writer.content(text).with_value(this);
  }

  RetTy enter() {
    log("enter");
    return writer.enter().with_value(this);
  }
  RetTy leave() {
    log("leave");
    return writer.leave().with_value(this);
  }
  RetTy finish() {
    log("finish");
    return writer.finish().with_value(this);
  }

  WrappedTy &getWriter() { return writer; }

  void log(const char *action) { log<std::vector<SVGAttribute>>(action); }

  template <typename container_t>
  void log(const char *action, const container_t *attrs = nullptr,
           const char *text = nullptr) {
    outs() << action;
    if (attrs && attrs->size()) {
      outs() << '(';
      auto It = attrs->begin(), End = attrs->end();
      outs() << *(It++);
      for (; It != End; ++It)
        outs() << ", " << *It;
      outs() << ')';
    }
    if (text)
      outs() << ": \"" << text << "\"";
    outs() << std::endl;
  }

private:
  outstream_t &outs() { return *os; }

  outstream_t *os;
  WrappedTy writer;
};
} // namespace svg
#endif // SVGUTILS_SVG_LOGGING_WRITER_H
