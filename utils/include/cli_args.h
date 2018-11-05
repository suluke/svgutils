#ifndef SVGUTILS_UTILS_CLI_ARGS_H
#define SVGUTILS_UTILS_CLI_ARGS_H

#include "svgutils/utils.h"

#include <cassert>
#include <charconv>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string_view>
#include <variant>

namespace svg {
namespace cl {
///
struct CliName {
  constexpr CliName(std::string_view name) : name(name) {}

private:
  template <typename T> friend struct CliOpt;
  std::string_view name;
};
///
struct CliMetaName {
  constexpr CliMetaName(std::string_view name) : name(name) {}

private:
  template <typename T> friend struct CliOpt;
  std::string_view name;
};
///
struct CliDesc {
  constexpr CliDesc(std::string_view desc) : desc(desc) {}

private:
  template <typename T> friend struct CliOpt;
  std::string_view desc;
};
///
template <typename ValTy> struct CliInit {
  constexpr CliInit(ValTy val) : val(val) {}

private:
  template <typename T> friend struct CliOpt;
  ValTy val;
};
///
template <typename ValTy> struct CliStorage {
  constexpr CliStorage(ValTy &val) : val(val) {}

private:
  template <typename T> friend struct CliOpt;
  ValTy &val;
};
///
template <typename ValTy> struct CliAppTag {};
///
struct CliRequired {};

template <typename T> std::optional<T> CliParseValue(std::string_view value);

template <typename ValTy> struct CliOpt {

  template <typename... args_t> constexpr CliOpt(args_t &&... args) {
    consume(std::forward<args_t>(args)...);
    if (!getPtr())
      value = OwnedVal(new ValTy);
    if (!registrator)
      registrator = &registerWithApp<void>;
    registrator(*this);
  }
  operator ValTy &() const {
    ValTy *val = getPtr();
    assert(val && "Failed to find value");
    return *val;
  }
  ValTy *operator->() {
    assert(getPtr() && "Casting cl::opt to value without initialization");
    return getPtr();
  }
  bool parse(std::string_view val) {
    auto parsed = CliParseValue<ValTy>(val);
    if (!parsed)
      return false;
    *getPtr() = std::move(*parsed);
    valueGiven = true;
    return true;
  }
  bool validate() const {
    if (required()) {
      if (!valueGiven) {
        std::cerr << "Required value not given for option \"";
        display(std::cerr);
        std::cerr << "\"" << std::endl;
      }
      return valueGiven;
    }
    return true;
  }
  int nargs() const { return 1; }
  bool required() const {
    return Required;
  }
  void display(std::ostream &os) const {
    if (name.size())
      os << "-" << name;
    else
      os << meta;
  }

private:
  using OwnedVal = std::unique_ptr<ValTy>;
  std::variant<ValTy *, OwnedVal> value = nullptr;
  using RegistrationCB = void (*)(CliOpt<ValTy> &);
  RegistrationCB registrator = nullptr;
  std::string_view name = "";
  std::string_view meta = "";
  std::string_view desc = "";
  bool Required = false;
  bool valueGiven = false;

  constexpr ValTy *getPtr() const {
    ValTy *val = nullptr;
    std::visit(
        [&val](auto &&value) {
          using T = std::decay_t<decltype(value)>;
          if constexpr (std::is_same_v<T, ValTy *>)
            val = value;
          else
            val = value.get();
        },
        value);
    return val;
  }

  constexpr void consume() {}
  template <typename... args_t>
  constexpr void consume(const CliName &name, args_t &&... args) {
    this->name = name.name;
    consume(std::forward<args_t>(args)...);
  }
  template <typename... args_t>
  constexpr void consume(const CliMetaName &name, args_t &&... args) {
    this->meta = name.name;
    consume(std::forward<args_t>(args)...);
  }
  template <typename... args_t>
  constexpr void consume(const CliDesc &desc, args_t &&... args) {
    this->desc = desc.desc;
    consume(std::forward<args_t>(args)...);
  }
  template <typename AppTag, typename... args_t>
  constexpr void consume(const CliAppTag<AppTag> &tag, args_t &&... args) {
    assert(!registrator && "Must not register with more than one app tag");
    registrator = &registerWithApp<AppTag>;
    consume(std::forward<args_t>(args)...);
  }
  template <typename... args_t>
  constexpr void consume(const CliStorage<ValTy> &storage, args_t &&... args) {
    if (getPtr() && !std::holds_alternative<OwnedVal>(value))
      unreachable("Multiple storage specifiers given");
    OwnedVal prev(nullptr);
    if (std::holds_alternative<OwnedVal>(value))
      prev = std::move(std::get<OwnedVal>(value));
    value = &storage.val;
    if (prev)
      *storage.val = *prev;
    consume(std::forward<args_t>(args)...);
  }
  template <typename... args_t>
  constexpr void consume(const CliInit<ValTy> &init, args_t &&... args) {
    if (!getPtr())
      value = OwnedVal(new ValTy(init.val));
    else
      *getPtr() = init.val;
    consume(std::forward<args_t>(args)...);
  }
  template <typename... args_t>
  constexpr void consume(const CliRequired &, args_t &&... args) {
    Required = true;
    consume(std::forward<args_t>(args)...);
  }
  template <typename AppTag> static void registerWithApp(CliOpt<ValTy> &);
};

template <> int CliOpt<bool>::nargs() const { return 0; }

// TODO
//~ struct CliAlias {

//~ };

//~ template<typename ValTy>
//~ struct CliList {

//~ template<typename... args_t>
//~ CliList(args_t &&... args) {

//~ }

//~ };

struct CliOptConcept {
  virtual ~CliOptConcept() = default;
  [[nodiscard]] virtual bool parse(std::string_view val) = 0;
  [[nodiscard]] virtual bool validate() const = 0;
  [[nodiscard]] virtual int nargs() const = 0;
  [[nodiscard]] virtual bool required() const = 0;
  virtual void display(std::ostream &os) const = 0;

protected:
  CliOptConcept() = default;
};

template <typename OptTy> struct CliOptModel : public CliOptConcept {
  CliOptModel(OptTy &opt) : opt(opt) {}
  [[nodiscard]] bool parse(std::string_view val) override {
    return opt.parse(val);
  }
  [[nodiscard]] bool validate() const override { return opt.validate(); }
  [[nodiscard]] int nargs() const override { return opt.nargs(); }
  [[nodiscard]] bool required() const override { return opt.required(); }
  void display(std::ostream &os) const override { opt.display(os); }

private:
  OptTy &opt;
};

/// Convenience type aliases
using name = CliName;
using meta = CliMetaName;
using desc = CliDesc;
using required = CliRequired;
template <typename T> using init = CliInit<T>;
template <typename T> using storage = CliStorage<T>;
template <typename T> using opt = CliOpt<T>;

template <typename AppTag = void> struct ParseArgs {
  using OwnedOption = std::unique_ptr<CliOptConcept>;

  ParseArgs(const char *tool, const char *desc, int argc, const char **argv) : tool(tool), desc(desc) {
    OwnedOption &eatAll = options()[""];
    int argNum = 1; // Skip executable name in argument parsing
    bool verbatim = false;
    for (; argNum < argc; ++argNum) {
      std::string_view arg = argv[argNum];
      if (!verbatim && arg == "--") {
        // treat all remaining arguments as verbatim
        verbatim = true;
      } else if (!verbatim && arg.front() == '-' && arg.size() > 1) {
        // we have an option
        std::string_view name = parseOptName(arg);
        OwnedOption &opt = options()[name];
        if (!opt) {
          std::cerr << "Encountered unknown option " << arg << std::endl;
          bail();
        }
        if (auto prefixLen = &name.front() - &arg.front() + name.size();
            prefixLen < arg.size()) {
          std::string_view inlineVal =
              arg.substr(prefixLen + 1 /* equals sign */);
          opt->parse(inlineVal);
          // The option ends after the inline value
          continue;
        }
        int nargNum = 0, nargs = opt->nargs();
        if (argNum + 1 == argc && nargNum < nargs) {
          std::cerr << "Not enough values given for option ";
          opt->display(std::cerr);
          std::cerr << std::endl;
          bail();
        }
        while (argNum + 1 < argc && nargNum < nargs) {
          arg = argv[argNum + 1];
          if (arg == "--") {
            verbatim = true;
            continue;
          } else if (!verbatim && arg.front() == '-' && arg.size() > 1) {
            // '-'-prefixed values are still treated as flags
            break;
          }
          ++argNum;
          ++nargNum;
          opt->parse(arg);
        }
        if (nargNum < nargs) {
          std::cerr << "Not enough values given for option ";
          opt->display(std::cerr);
          std::cerr << std::endl;
          bail();
        }
      } else {
        // this must be a positional argument
        if (!eatAll) {
          std::cerr << "Too many positional arguments: " << arg << std::endl;
          bail();
        }
        eatAll->parse(arg);
      }
    }
    for (; argNum < argc; ++argNum) {
      std::string_view arg = argv[argNum];
      if (!eatAll) {
        std::cerr << "Too many positional arguments: " << arg << std::endl;
        bail();
      }
      eatAll->parse(arg);
    }
    bool allValid = true;
    for (const auto &KeyValuePair : options())
      if (KeyValuePair.second && !KeyValuePair.second->validate())
        allValid = false;
    if (!allValid)
      bail();
  }

  void printHelp() {
    std::cout << "usage: " << tool << " <OPTION>...";
    OwnedOption &eatAll = options()[""];
    if (eatAll) {
      std::cout << " ";
      if (!eatAll->required())
        std::cout << "[";
      std::cout << "<";
      eatAll->display(std::cout);
      std::cout << ">";
      if (!eatAll->required())
        std::cout << "]";
    }
    std::cout << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    for (const auto &KeyValuePair : options()) {
      const OwnedOption &opt = KeyValuePair.second;
      if (opt == eatAll)
        continue;
      opt->display(std::cout);
      std::cout << std::endl;
    }
  }

  static void addOption(std::string_view name, OwnedOption opt) {
    assert(!options().count(name) && "Registered option more than once");
    options()[name] = std::move(opt);
  }

private:
  const char *tool;
  const char *desc;

  using optionmap_t = std::map<std::string_view, OwnedOption>;
  static optionmap_t &options();
  template <typename T> friend struct CliOpt;

  std::string_view parseOptName(std::string_view opt) {
    if (opt.front() == '-')
      opt = opt.substr(1);
    if (opt.front() == '-')
      opt = opt.substr(1);
    if (auto eqPos = opt.find("="); eqPos != std::string_view::npos) {
      opt = opt.substr(0, eqPos);
    }
    return opt;
  }
  void bail() {
    std::cout << std::endl;
    printHelp();
    std::exit(1);
  }
};

template <typename AppTag>
ParseArgs<>::optionmap_t &ParseArgs<AppTag>::options() {
  // this static map is used for the static registration of options
  static optionmap_t opts;
  return opts;
}

template <typename ValTy>
template <typename AppTag>
void CliOpt<ValTy>::registerWithApp(CliOpt<ValTy> &opt) {
  ParseArgs<AppTag>::addOption(opt.name,
                               ParseArgs<>::OwnedOption(new CliOptModel(opt)));
}

/// Definition of CliParseValue<std::string>
template <> std::optional<std::string> CliParseValue(std::string_view value) {
  return std::string(value);
}
/// Definition of CliParseValue<std::filesystem::path>
template <>
std::optional<std::filesystem::path> CliParseValue(std::string_view value) {
  return std::filesystem::path(value);
}
/// Definition of CliParseValue<unsigned>
template <> std::optional<unsigned> CliParseValue(std::string_view value) {
  unsigned u;
  auto result = std::from_chars(value.data(), value.data() + value.size(), u);
  if (result.ptr != value.data() + value.size())
    return std::nullopt;
  return u;
}
} // namespace cl
} // namespace svg
#endif // SVGUTILS_UTILS_CLI_ARGS_H
