#include "svgcairo/freetype.h"
#include "svgutils/utils.h"

#include <fontconfig/fontconfig.h>

#include FT_CACHE_H

#include <cassert>
#include <filesystem>
#include <iostream>
#include <map>
#include <set>

using namespace svg;

struct Fontconfig {
  Fontconfig() : config(FcConfigReference(nullptr), FcConfigDestroy) {
    FcInitLoadConfigAndFonts();
  }

  using Pattern = std::unique_ptr<FcPattern, decltype(&FcPatternDestroy)>;
  Pattern parsePattern(const char *str) const {
    FcPattern *p = FcNameParse(reinterpret_cast<const FcChar8 *>(str));
    if (!FcConfigSubstitute(config.get(), p, FcMatchPattern))
      svg_unreachable("Failed to substitute font pattern");
    FcDefaultSubstitute(p);
    return Pattern(p, FcPatternDestroy);
  }
  std::optional<std::filesystem::path> matchPattern(const Pattern &p) const {
    FcResult result;
    Pattern match(FcFontMatch(config.get(), p.get(), &result),
                  FcPatternDestroy);
    if (result == FcResultNoMatch) {
      std::cerr << "No match" << std::endl;
      return std::nullopt;
    } else if (result == FcResultOutOfMemory) {
      std::cerr << "Out of memory" << std::endl;
      return std::nullopt;
    }
    FcValue val;
    result = FcPatternGet(match.get(), "file", 0, &val);
    if (result) {
      std::cerr << "Could not extract file path" << std::endl;
      return std::nullopt;
    }
    assert(val.type == FcTypeString);
    using namespace std::filesystem;
    return path(reinterpret_cast<const char *>(val.u.s));
  }
  void printPattern(const Pattern &p) const { FcPatternPrint(p.get()); }

private:
  std::unique_ptr<FcConfig, decltype(&FcConfigDestroy)> config;
};

template <typename ObjectTy, typename DeleterTy> struct FTManaged {
  FTManaged(ObjectTy obj, DeleterTy del) : obj(obj), del(del) {}
  FTManaged() = default;
  FTManaged(FTManaged &&o) { *this = std::move(o); }
  FTManaged &operator=(FTManaged &&o) {
    if (obj)
      del(obj);
    obj = o.obj;
    del = o.del;
    o.obj = nullptr;
    o.del = nullptr;
    return *this;
  }
  ~FTManaged() {
    if (*this)
      del(obj);
  }

  operator bool() const { return obj && del; }

  ObjectTy &operator*() { return obj; }
  const ObjectTy &operator*() const { return obj; }

  ObjectTy *operator->() { return obj; }
  const ObjectTy *operator->() const { return obj; }

  ObjectTy obj;
  DeleterTy del;
};

/// I think FTC_Manager is buggy. No one on the internet seems to use it.
/// Well, I guess I just build my own... However, the API remains similar
/// (ugly C-like).
struct FontCache {
  FontCache(FT_Library lib, void *reqData, FTC_Face_Requester requester)
      : library(lib), requester(requester), reqData(reqData) {}

  using FontFace = FTManaged<FT_Face, decltype(&FT_Done_Face)>;

  FT_Face lookupFace(const void *faceId) {
    if (faces.count(faceId))
      return *faces.at(faceId);
    FT_Face face;
    auto error = requester(const_cast<void *>(faceId), library, reqData, &face);
    if (error)
      return nullptr;
    faces[faceId] = FontFace(face, FT_Done_Face);
    return face;
  }

private:
  FT_Library library;
  FTC_Face_Requester requester;
  void *reqData;
  std::map<const void *, FontFace> faces;
};

struct Freetype::Context {
  Fontconfig fontDB;
  FTManaged<FT_Library, decltype(&FT_Done_FreeType)> library;
  FontCache cache;
  std::set<std::string> faces;

  static std::unique_ptr<Context> Create();

private:
  Context(FT_Library lib, FTC_Face_Requester requester)
      : library(lib, FT_Done_FreeType), cache(lib, this, requester) {}
};
static FT_Error FaceRequester(void *faceId, FT_Library library, void *reqData,
                              FT_Face *face) {
  Freetype::Context &ctx = *reinterpret_cast<Freetype::Context *>(reqData);
  const std::string &str = *reinterpret_cast<std::string *>(faceId);
  Fontconfig::Pattern p = ctx.fontDB.parsePattern(str.c_str());
  auto pathOpt = ctx.fontDB.matchPattern(p);
  if (!pathOpt || !std::filesystem::exists(*pathOpt))
    return 1;
  auto path = *pathOpt;
  auto error = FT_New_Face(library, path.c_str(), 0, face);
  return error;
}
std::unique_ptr<Freetype::Context> Freetype::Context::Create() {
  FT_Library library;
  auto error = FT_Init_FreeType(&library);
  if (error)
    return nullptr;
  return std::unique_ptr<Context>(new Context(library, FaceRequester));
}

Freetype::Freetype(std::unique_ptr<Freetype::Context> ctx)
    : ctx(std::move(ctx)) {}
Freetype::Freetype(Freetype &&) = default;
Freetype &Freetype::operator=(Freetype &&) = default;
Freetype::~Freetype() = default;

FT_Face Freetype::getFace(const char *desc) {
  auto insertRes = ctx->faces.insert(desc);
  auto iter = insertRes.first;
  FT_Face res = ctx->cache.lookupFace(&*iter);
  return res;
}

std::optional<Freetype> Freetype::Create() {
  auto ctx = Context::Create();
  if (!ctx)
    return std::nullopt;
  return Freetype(std::move(ctx));
}
