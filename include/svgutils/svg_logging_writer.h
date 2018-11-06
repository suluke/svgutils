#ifndef SVGUTILS_SVG_LOGGING_WRITER_H
#define SVGUTILS_SVG_LOGGING_WRITER_H

#include "svgutils/svg_writer.h"

namespace svg {

template <typename WrappedTy> struct SVGLoggingWriter {
  using self_t = SVGLoggingWriter<WrappedTy>;
  template <typename... args_t>
  SVGLoggingWriter(args_t &&... args) : writer(std::forward<args_t>(args)...) {}
  SVGLoggingWriter(self_t &&) = default;
  self_t &operator=(self_t &&) = default;

#define SVG_TAG(NAME, STR, ...)                                                \
  template <typename... attrs_t> self_t &NAME(attrs_t... attrs) {              \
    std::vector<SVGAttribute> attrsVec({std::forward<attrs_t>(attrs)...});     \
    NAME(attrsVec);                                                            \
    return *this;                                                              \
  }                                                                            \
  template <typename container_t> self_t &NAME(const container_t &attrs) {     \
    log(#NAME, &attrs);                                                        \
    writer.NAME(attrs);                                                        \
    return *this;                                                              \
  }
#include "svgutils/svg_entities.def"

  self_t &content(const char *text) {
    log<std::vector<SVGAttribute>>("content", nullptr, text);
    writer.content(text);
    return *this;
  }

  self_t &enter() {
    log("enter");
    writer.enter();
    return *this;
  }
  self_t &leave() {
    log("leave");
    writer.leave();
    return *this;
  }
  self_t &finish() {
    log("finish");
    writer.finish();
    return *this;
  }

  void log(const char *action) { log<std::vector<SVGAttribute>>(action); }

  template <typename container_t>
  void log(const char *action, const container_t *attrs = nullptr,
           const char *text = nullptr) {
    std::cout << action;
    if (attrs && attrs->size()) {
      std::cout << '(';
      auto It = attrs->begin(), End = attrs->end();
      std::cout << *(It++);
      for (; It != End; ++It)
        std::cout << ", " << *It;
      std::cout << ')';
    }
    if (text)
      std::cout << ": \"" << text << "\"";
    std::cout << std::endl;
  }

private:
  WrappedTy writer;
};
} // namespace svg
#endif // SVGUTILS_SVG_LOGGING_WRITER_H
