#ifndef SVGUTILS_UTILS_CLI_ARGS_H
#define SVGUTILS_UTILS_CLI_ARGS_H

#include "svgutils/utils.h"

#include <array>
#include <cassert>
#include <cctype>
#include <charconv>
#include <deque>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string_view>
#include <variant>
#include <vector>

namespace svg {
namespace cl {

/// The name of a command line flag (i.e. CliOpt), i.e. the unique
/// identifier after '-' or '--'.
/// If a CliOpt is not given a name or the name is an empty string (or
/// even nullptr) the option is automatically expected to handle all
/// positional arguments. Use CliMetaName to still provide a meaningful
/// short description for what these values are being used.
struct CliName {
  constexpr CliName(std::string_view name) : name(name) {}

private:
  template <typename T, typename U> friend struct CliOptBase;
  std::string_view name;
};

/// A name for the value that is assigned to a CliOpt.
/// E.g. FILENAME in `-o FILENAME`.
struct CliMetaName {
  constexpr CliMetaName(std::string_view name) : name(name) {}

private:
  template <typename T, typename U> friend struct CliOptBase;
  std::string_view name;
};

/// A textual description of what a CliOpt does. Will be printed as part
/// of the standard help output.
struct CliDesc {
  constexpr CliDesc(std::string_view desc) : desc(desc) {}

private:
  template <typename T, typename U> friend struct CliOptBase;
  std::string_view desc;
};

/// A value to assign to an CliOpt in case no value was provided on the
/// command line.
template <typename ValTy> struct CliInit {
  constexpr CliInit(ValTy val) : val(val) {}

private:
  template <typename T> friend struct CliOpt;
  ValTy val;
};

/// Associates a CliOpt with an external variable that is used for storing
/// its value. Useful when making existing global variables configurable
/// over the command line.
template <typename ValTy> struct CliStorage {
  constexpr CliStorage(ValTy &val) : val(val) {}

private:
  template <typename T> friend struct CliOpt;
  ValTy &val;
};

/// Indicates the "app" an option belongs to. The default AppTag is void.
template <typename ValTy> struct CliAppTag {};

/// Indicates that a value for an option is required.
struct CliRequired {};

/// A free template function to be used by CliOpts to convert command
/// line argument strings into the type of a corresponding option.
/// For types not natively supported by this library clients need to
/// provide their own specializations.
template <typename T> std::optional<T> CliParseValue(std::string_view value);

/// Common functionality for command line options
template <typename DerivedTy, typename DecayTy> struct CliOptBase {
  /// Automatic conversion to the option's underlying value.
  constexpr operator DecayTy &() const {
    DecayTy *ptr = static_cast<const DerivedTy *>(this)->getPtr();
    assert(ptr && "Casting option to underlying type without initialization");
    return *ptr;
  }
  /// Use the dereference (->) operator in cases where implicit conversion
  /// to ValContainer fails, i.e. in member lookups.
  constexpr DecayTy *operator->() {
    DecayTy *ptr = static_cast<DerivedTy *>(this)->getPtr();
    assert(ptr && "Casting option to underlying type without initialization");
    return ptr;
  }
  /// Returns whether this is a required option or not.
  constexpr bool required() const { return Required; }
  /// Prints a visual representation of this option to the specified
  /// stream @p os.
  void display(std::ostream &os) const {
    if (name.size())
      os << "-" << name;
    else
      os << meta;
  }
  /// After all command line arguments have been processed, this function
  /// is used for checking that this option is in a valid state.
  /// E.g., `require`d options need to have been specified by the user.
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

protected:
  /// Registration is performed dynamically after all configuration flags
  /// passed to the constructor have been evaluated. It is possible to
  /// register with a different AppTag, so the registration function
  /// needs to be changeable.
  using RegistrationCB = void (*)(DerivedTy &);
  RegistrationCB registrator = nullptr;
  std::string_view name = "";
  std::string_view meta = "";
  std::string_view desc = "";
  bool Required = false;
  bool valueGiven = false;

  /// Static function to be used for registering a CliOpt with a
  /// ParseArg with a given AppTag. Definition follows further below
  /// after ParseArg has been defined.
  template <typename AppTag> static void registerWithApp(DerivedTy &);

  /// Overload to be called after all flags passed to the constructor
  /// have been `consume`d.
  constexpr void consume() {}

  template <typename... args_t>
  constexpr void consume(const CliName &name, args_t &&... args) {
    this->name = name.name;
    static_cast<DerivedTy *>(this)->consume(std::forward<args_t>(args)...);
  }
  template <typename... args_t>
  constexpr void consume(const CliMetaName &name, args_t &&... args) {
    meta = name.name;
    static_cast<DerivedTy *>(this)->consume(std::forward<args_t>(args)...);
  }
  template <typename... args_t>
  constexpr void consume(const CliDesc &desc, args_t &&... args) {
    this->desc = desc.desc;
    static_cast<DerivedTy *>(this)->consume(std::forward<args_t>(args)...);
  }
  template <typename AppTag, typename... args_t>
  constexpr void consume(const CliAppTag<AppTag> &tag, args_t &&... args) {
    assert(!registrator && "Must not register with more than one app tag");
    registrator = &registerWithApp<AppTag>;
    static_cast<DerivedTy *>(this)->consume(std::forward<args_t>(args)...);
  }
  template <typename... args_t>
  constexpr void consume(const CliRequired &, args_t &&... args) {
    Required = true;
    static_cast<DerivedTy *>(this)->consume(std::forward<args_t>(args)...);
  }
};

/// Implementation of a single-value command line option.
template <typename ValTy>
struct CliOpt : public CliOptBase<CliOpt<ValTy>, ValTy> {
  using DecayTy = ValTy;
  using base_t = CliOptBase<CliOpt<ValTy>, DecayTy>;
  friend base_t;

  template <typename... args_t> constexpr CliOpt(args_t &&... args) {
    consume(std::forward<args_t>(args)...);
    if (!getPtr())
      value = OwnedVal(new ValTy);
    if (!base_t::registrator)
      base_t::registrator = &base_t::template registerWithApp<void>;
    base_t::registrator(*this);
  }

  /// Import conversion operators from base_t
  using base_t::operator DecayTy &;
  using base_t::operator->;

  /// Try to parse the given string_views to assign values to this option.
  /// Requires a matching specialization of the CliParseValue template
  /// function.
  std::optional<size_t> parse(std::deque<std::string_view> &values,
                              bool isInline = false) {
    auto parsed = CliParseValue<ValTy>(values.front());
    if (!parsed)
      return std::nullopt;
    assign(std::move(*parsed));
    return 1;
  }

private:
  /// The rationale behind using a unique_ptr to a heap-allocated ValTy
  /// in case no storage is specified is that we want CliOpts to take as
  /// little memory as possible in case storage *IS* specified. In that
  /// case, making OwnedVal = ValTy, the `value` member would be the
  /// same size as ValTy, i.e. taking twice as much storage as ValTy
  using OwnedVal = std::unique_ptr<ValTy>;
  std::variant<ValTy *, OwnedVal> value = nullptr;

  /// Returns the storage location of this option. During option
  /// initialization this function can return nullptr. However, after
  /// the constructor has been executed this function should never
  /// return nullptr.
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

  constexpr void assign(ValTy &&val) {
    base_t::valueGiven = true;
    *getPtr() = std::move(val);
  }

  template <typename... args_t>
  constexpr void consume(const CliStorage<ValTy> &storage, args_t &&... args) {
    if (getPtr() && !std::holds_alternative<OwnedVal>(value))
      svg_unreachable("Multiple storage specifiers given");
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
  /// Import `consume` implementations from base_t
  using base_t::consume;
};

// TODO
//~ struct CliAlias {

//~ };

/// Implementation of a multi-value command line option
template <typename ValTy>
struct CliList : public CliOptBase<CliList<ValTy>, std::vector<ValTy>> {
  using ValContainer = std::vector<ValTy>;
  using DecayTy = ValContainer;
  using base_t = CliOptBase<CliList<ValTy>, DecayTy>;
  friend base_t;

  template <typename... args_t> constexpr CliList(args_t &&... args) {
    consume(std::forward<args_t>(args)...);
    if (!getPtr())
      list = OwnedContainer(new ValContainer);
    if (!base_t::registrator)
      base_t::registrator = &base_t::template registerWithApp<void>;
    base_t::registrator(*this);
  }

  /// Import conversion operators from base_t
  using base_t::operator DecayTy &;
  using base_t::operator->;

  /// Try to parse the given string_views to assign values to this option.
  /// Requires a matching specialization of the CliParseValue template
  /// function.
  std::optional<size_t> parse(std::deque<std::string_view> &values,
                              bool isInline = false) {
    for (const auto &val : values) {
      auto parsed = CliParseValue<ValTy>(val);
      if (!parsed) {
        clear();
        return std::nullopt;
      }
      insert(std::move(*parsed));
    }
    return values.size();
  }
  auto begin() const {
    ValContainer *ptr = getPtr();
    assert(ptr && "Tried to iterate over cl::list prior to initialization");
    return ptr->begin();
  }
  auto end() const {
    ValContainer *ptr = getPtr();
    assert(ptr && "Tried to iterate over cl::list prior to initialization");
    return ptr->end();
  }

private:
  using OwnedContainer = std::unique_ptr<ValContainer>;
  std::variant<ValContainer *, OwnedContainer> list = nullptr;

  void insert(ValTy &&val) {
    base_t::valueGiven = true;
    getPtr()->emplace_back(std::move(val));
  }
  void clear() { getPtr()->clear(); }

  constexpr ValContainer *getPtr() const {
    ValContainer *res = nullptr;
    std::visit(
        [&res](auto &&value) {
          using T = std::decay_t<decltype(value)>;
          if constexpr (std::is_same_v<T, ValContainer *>)
            res = value;
          else
            res = value.get();
        },
        list);
    return res;
  }

  /// Import `consume` implementations from base_t
  using base_t::consume;
};

/// Interface for all types of options.
struct CliOptConcept {
  virtual ~CliOptConcept() = default;
  [[nodiscard]] virtual std::optional<size_t>
  parse(std::deque<std::string_view> &values, bool isInline = false) = 0;
  [[nodiscard]] virtual bool validate() const = 0;
  [[nodiscard]] virtual bool required() const = 0;
  virtual void display(std::ostream &os) const = 0;

protected:
  CliOptConcept() = default;
};

/// Generic implementation of the CliOptConcept interface that simply
/// forwards all function calls to the corresponding function of the
/// OptTy object this model references.
template <typename OptTy> struct CliOptModel : public CliOptConcept {
  CliOptModel(OptTy &opt) : opt(opt) {}
  [[nodiscard]] std::optional<size_t>
  parse(std::deque<std::string_view> &values, bool isInline = false) override {
    return opt.parse(values, isInline);
  }
  [[nodiscard]] bool validate() const override { return opt.validate(); }
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
template <typename T> CliInit<T> init(T &&val) {
  return CliInit<T>(std::forward<T>(val));
}
template <typename T> CliStorage<T> storage(T &storage) {
  return CliStorage(storage);
}
template <typename T> using opt = CliOpt<T>;
template <typename T> using list = CliList<T>;

/// Entry point to the cli_args library.
template <typename AppTag = void> struct ParseArgs {
  using OwnedOption = std::unique_ptr<CliOptConcept>;

  /// Parse the command line arguments specified by @p argc and @p argv.
  /// The parameter @p tool is the application's name and @p desc should
  /// contain a brief description of what the application does. Both
  /// these values are used by the help message.
  ParseArgs(const char *tool, const char *desc, int argc, const char **argv)
      : tool(tool), desc(desc) {
    bool verbatim = false;
    std::deque<std::string_view> values;
    std::deque<std::string_view> positional;
    // =1: Skip executable name in argument parsing
    for (int argNum = 1; argNum < argc; ++argNum) {
      std::string_view arg = argv[argNum];
      if (!verbatim && arg == "--") {
        // Treat all remaining arguments as verbatim
        verbatim = true;
      } else if (!verbatim && arg.front() == '-' && arg.size() > 1) {
        // We have an option. Do we know it?
        std::string_view name = parseOptName(arg);
        OwnedOption &opt = options()[name];
        if (!opt) {
          std::cerr << "Encountered unknown option " << arg << std::endl;
          bail();
        }
        // Is the argument just the name or also an '=' assignment?
        if (auto prefixLen = &name.front() - &arg.front() + name.size();
            prefixLen < arg.size()) {
          std::string_view inlineVal =
              arg.substr(prefixLen + 1 /* equals sign */);
          values.push_back(inlineVal);
          auto res = opt->parse(values, true);
          if (!res)
            bail(); // Error message is written by `parse()`
          if (0 > *res || *res > values.size())
            svg_unreachable("Illegal number of values read by option");
          if (*res == 0)
            svg_unreachable(
                "Failing to parse inline argument MUST result in std::nullopt");
          // Don't forget to clear!!!
          values.clear();
          // The option ends after the inline value
          continue;
        }
        // Collect all arguments that could be values to the current option
        while (argNum + 1 < argc) {
          arg = argv[argNum + 1];
          if (!verbatim) {
            if (arg == "--") {
              verbatim = true;
              ++argNum;
              continue;
            } else if (arg.front() == '-' && arg.size() > 1) {
              // '-'-prefixed values are still treated as flags
              break;
            }
          }
          ++argNum;
          values.push_back(arg);
        }
        auto res = opt->parse(values);
        if (!res)
          bail(); // Error message is written by `parse()`
        if (0 > *res || *res > values.size())
          svg_unreachable("Illegal number of values read by option");
        for (size_t i = 0; i < *res; ++i)
          values.pop_front();
        positional.insert(positional.end(), values.begin(), values.end());
        values.clear();
      } else
        positional.push_back(arg);
    }
    // Parse positional arguments
    if (positional.size()) {
      OwnedOption &eatAll = options()[""];
      if (!eatAll) {
        std::cerr << "Too many positional arguments given:\n";
        for (const auto &arg : positional)
          std::cerr << arg << std::endl;
        bail();
      }
      auto res = eatAll->parse(positional);
      if (!res)
        bail();
      if (0 > *res || *res > positional.size())
        svg_unreachable("Illegal number of values read by option");
      if (*res != positional.size()) {
        std::cerr << "Too many positional arguments given:\n";
        for (auto It = positional.begin() + *res, End = positional.end();
             It != End; ++It)
          std::cerr << *It << std::endl;
        bail();
      }
    }
    // Check that all options are in a valid state
    bool allValid = true;
    for (const auto &KeyValuePair : options())
      if (KeyValuePair.second && !KeyValuePair.second->validate())
        allValid = false;
    if (!allValid)
      bail();
  }

  /// Prints a help message for this ParseArg instance.
  void printHelp() {
    // FIXME this is still in MVP state
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

  /// Add an option to the ParseArg namespace denoted by AppTag.
  static void addOption(std::string_view name, OwnedOption opt) {
    assert(!options().count(name) && "Registered option more than once");
    options()[name] = std::move(opt);
  }

private:
  const char *tool;
  const char *desc;

  using optionmap_t = std::map<std::string_view, OwnedOption>;
  /// This function is used for implementing the registration process.
  /// Since it is a template function it will be turned into a weak
  /// symbol by the compiler which makes it possible to have only one
  /// global container of registered options without requiring a
  /// compilation unit. Furthermore, wrapping the static optionmap_t
  /// inside the function fixes initialization order issues in conjunction
  /// with statically initialized options.
  static optionmap_t &options();
  template <typename T> friend struct CliOpt;

  /// Extract the name part of an option from a string, i.e.
  /// everything between '-'/'--' and '='/the end of the string
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

template <typename DerivedTy, typename DecayTy>
template <typename AppTag>
void CliOptBase<DerivedTy, DecayTy>::registerWithApp(DerivedTy &opt) {
  ParseArgs<AppTag>::addOption(opt.name,
                               ParseArgs<>::OwnedOption(new CliOptModel(opt)));
}

/// Definition of CliParseValue<bool>
template <> std::optional<bool> CliParseValue(std::string_view value) {
  // All legal values fit in a 5 character array (yes,no,true,false,on,off)
  if (value.size() > 5)
    return std::nullopt;
  std::array<char, 5> lower;
  for (unsigned i = 0; i < value.size(); ++i)
    lower[i] = std::tolower(value[i]);
  std::string_view cmp(lower.begin(), value.size());
  if (cmp == "true")
    return true;
  if (cmp == "false")
    return false;
  if (cmp == "on")
    return true;
  if (cmp == "off")
    if (cmp == "yes")
      return true;
  if (cmp == "no")
    return false;
  return std::nullopt;
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

/// We specialize option parsing for booleans since they should only be
/// set implicitly (just the flag) or using an inline value
template <>
std::optional<size_t> CliOpt<bool>::parse(std::deque<std::string_view> &values,
                                          bool isInline) {
  if (values.empty() || (values.size() && !isInline)) {
    assign(true);
    return 0;
  }
  auto parsed = CliParseValue<bool>(values.front());
  if (!parsed) {
    std::cerr << "Could not parse boolean value for flag ";
    display(std::cerr);
    std::cerr << ": " << values.front() << std::endl;
    return std::nullopt;
  }
  // Assign takes rvalue, so we need to be slightly stupid here
  assign(*parsed && *parsed);
  return 1;
}
} // namespace cl
} // namespace svg
#endif // SVGUTILS_UTILS_CLI_ARGS_H