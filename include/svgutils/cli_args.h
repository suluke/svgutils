#ifndef SVGUTILS_UTILS_CLI_ARGS_H
#define SVGUTILS_UTILS_CLI_ARGS_H
/// The svgutils command line argument library
///
/// Features:
/// * Declarative configuration of the option parser using option variables.
/// * Support for scalar and aggregate options.
/// * Support for global or local option definitions.
///   Local options will unregister automatically when going out of scope.
/// * Support for application namespaces ("AppTag") to avoid
///   option pollution from global options in different compile units.
/// * Easy extendability for custom types by specializing CliParseValue.
/// * Automatic help text generation (printHelp).
/// * Support for 'stacked' option parsing, i.e. stopping parsing at some
///   point to continue with a different parsing configuration.
///
/// Brief overview:
/// * cl::opt for single-valued options
/// * cl::list for multi-valued options
/// * cl::name to specify names for an option
///   - No name or "": Handle positional arguments
///   - Multiple option names are possible
/// * cl::meta for describing value roles (used in printHelp)
/// * cl::desc for option description (used in printHelp)
/// * cl::init for initial option values
/// * cl::required to force users to provide a value
/// * cl::storage to re-use existing variables for option storage
/// * cl::option_end to stop the parser after the corresponding option
///   has been parsed.
/// * cl::app to specify the application the option is intended for

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
  const ValTy &getValue() const { return val; }

private:
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

/// Indicates that after an option with this attribute was encountered
/// the library is supposed to stop processing any following arguments
/// Use ParseArgs::getNumArgsRead() to find out where exactly this
/// library stopped parsing arguments.
struct CliOptionEnd {};

/// A free template function to be used by CliOpts to convert command
/// line argument strings into the type of a corresponding option.
/// For types not natively supported by this library clients need to
/// provide their own specializations.
template <typename T> std::optional<T> CliParseValue(std::string_view value);

/// Common functionality for command line options
template <typename DerivedTy, typename DecayTy> struct CliOptBase {
  ~CliOptBase() {
    // automatic unregistration
    if (registrator)
      registrator(*static_cast<DerivedTy *>(this), false);
  }

  /// Automatic conversion to the option's underlying value.
  constexpr operator DecayTy &() { return **this; }
  constexpr operator DecayTy &() const { return **this; }
  /// Use the dereference (->) operator in cases where implicit conversion
  /// to ValContainer fails, i.e. in member lookups.
  constexpr DecayTy *operator->() {
    DecayTy *ptr = static_cast<DerivedTy *>(this)->getPtr();
    assert(ptr && "Casting option to underlying type without initialization");
    return ptr;
  }
  constexpr DecayTy *operator->() const {
    DecayTy *ptr = static_cast<const DerivedTy *>(this)->getPtr();
    assert(ptr && "Casting option to underlying type without initialization");
    return ptr;
  }
  /// Use the dereference (*) operator in cases where implicit conversion
  /// to ValContainer fails, i.e. in member lookups.
  constexpr DecayTy &operator*() { return *this->operator->(); }
  constexpr DecayTy &operator*() const { return *this->operator->(); }
  /// Returns whether this is a required option or not.
  constexpr bool required() const { return Required; }
  /// Prints a visual representation of this option to the specified
  /// stream @p os.
  void display(std::ostream &os) const {
    if (names.size() && names.front() != "")
      os << "-" << names.front();
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
        std::cerr << "\"\n";
      }
      return valueGiven;
    }
    return true;
  }

  bool isEnd() const { return isFinalOption; }

protected:
  /// Registration is performed dynamically after all configuration flags
  /// passed to the constructor have been evaluated. It is possible to
  /// register with a different AppTag, so the registration function
  /// needs to be changeable.
  using RegistrationCB = void (*)(DerivedTy &, bool);
  RegistrationCB registrator = nullptr;
  std::vector<std::string_view> names;
  std::string_view meta = "";
  std::string_view desc = "";
  bool Required = false;
  bool valueGiven = false;
  bool isFinalOption = false;

  /// Static function to be used for registering a CliOpt with a
  /// ParseArg with a given AppTag. Definition follows further below
  /// after ParseArg has been defined.
  /// @param no_unreg specifies whether registration or unregistration is to be
  /// performed
  template <typename AppTag>
  static void registerWithApp(DerivedTy &, bool no_unreg);

  /// Overload to be called after all flags passed to the constructor
  /// have been `consume`d.
  constexpr void consume() {}

  template <typename... args_t>
  constexpr void consume(const CliName &name, args_t &&... args) {
    names.emplace_back(name.name);
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
  template <typename... args_t>
  constexpr void consume(const CliOptionEnd &, args_t &&... args) {
    isFinalOption = true;
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
      value = std::make_unique<ValTy>();
    if (!base_t::registrator)
      base_t::registrator = &base_t::template registerWithApp<void>;
    base_t::registrator(*this, true);
  }

  /// Import conversion operators from base_t
  using base_t::operator DecayTy &;
  using base_t::operator->;
  using base_t::operator*;

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
  template <typename T, typename... args_t>
  constexpr void consume(const CliInit<T> &init, args_t &&... args) {
    if (!getPtr())
      value =
          std::make_unique<ValTy>(static_cast<const ValTy &>(init.getValue()));
    else
      *getPtr() = init.getValue();
    consume(std::forward<args_t>(args)...);
  }
  /// Import `consume` implementations from base_t
  using base_t::consume;
};

/// Implementation of a multi-value command line option
template <typename ValTy>
struct CliList : public CliOptBase<CliList<ValTy>, std::vector<ValTy>> {
  using container_t = std::vector<ValTy>;
  using DecayTy = container_t;
  using base_t = CliOptBase<CliList<ValTy>, DecayTy>;
  friend base_t;

  template <typename... args_t> constexpr CliList(args_t &&... args) {
    consume(std::forward<args_t>(args)...);
    if (!getPtr())
      list = std::make_unique<container_t>();
    if (!base_t::registrator)
      base_t::registrator = &base_t::template registerWithApp<void>;
    base_t::registrator(*this, true);
  }

  /// Import conversion operators from base_t
  using base_t::operator DecayTy &;
  using base_t::operator->;
  using base_t::operator*;

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
  auto begin() const { return (*this)->begin(); }
  auto end() const { return (*this)->end(); }
  auto &operator[](size_t i) { return (**this)[i]; }

private:
  using OwnedContainer = std::unique_ptr<container_t>;
  std::variant<container_t *, OwnedContainer> list = nullptr;

  void insert(ValTy &&val) {
    // When getting a value for the first time, make sure the list is empty
    if (!base_t::valueGiven)
      getPtr()->clear();
    base_t::valueGiven = true;
    getPtr()->emplace_back(std::move(val));
  }
  void clear() { getPtr()->clear(); }

  constexpr container_t *getPtr() const {
    container_t *res = nullptr;
    std::visit(
        [&res](auto &&value) {
          using T = std::decay_t<decltype(value)>;
          if constexpr (std::is_same_v<T, container_t *>)
            res = value;
          else
            res = value.get();
        },
        list);
    return res;
  }

  /// Import `consume` implementations from base_t
  using base_t::consume;

  template <typename T, typename... args_t>
  void consume(const CliInit<T> &init, args_t &&... args) {
    if (!getPtr())
      list = std::make_unique<container_t>();
    container_t &list = *this;
    if constexpr (std::is_assignable_v<T, ValTy>)
      list.emplace_back(init.getValue());
    else
      list.insert(list.end(), init.getValue().begin(), init.getValue().end());
    consume(std::forward<args_t>(args)...);
  }
};

/// Interface for all types of options.
struct CliOptConcept {
  virtual ~CliOptConcept() = default;
  [[nodiscard]] virtual std::optional<size_t>
  parse(std::deque<std::string_view> &values, bool isInline = false) = 0;
  [[nodiscard]] virtual bool validate() const = 0;
  [[nodiscard]] virtual bool required() const = 0;
  [[nodiscard]] virtual bool isEnd() const = 0;
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
  [[nodiscard]] bool isEnd() const override { return opt.isEnd(); }
  void display(std::ostream &os) const override { opt.display(os); }

private:
  OptTy &opt;
};

/// Convenience type aliases
using name = CliName;
using meta = CliMetaName;
using desc = CliDesc;
using required = CliRequired;
using option_end = CliOptionEnd;
template <typename T> CliInit<T> init(T &&val) {
  return CliInit<T>(std::forward<T>(val));
}
template <typename T>
CliInit<std::vector<T>> init(std::initializer_list<T> &&val) {
  return CliInit<std::vector<T>>(std::forward<std::initializer_list<T>>(val));
}
template <typename T> CliStorage<T> storage(T &storage) {
  return CliStorage(storage);
}
template <typename T> CliAppTag<T> app() { return CliAppTag<T>(); }
template <typename T> using opt = CliOpt<T>;
template <typename T> using list = CliList<T>;

/// Entry point to the cli_args library.
template <typename AppTag = void> struct ParseArgs {
  using OwnedOption = std::unique_ptr<CliOptConcept>;

  /// Parse the command line arguments specified by @p argc and @p argv.
  /// The parameter @p tool is the application's name and @p desc should
  /// contain a brief description of what the application does. Both
  /// these values are used by the help message.
  ParseArgs(const char *tool, const char *desc, int argc, const char **argv,
            int offset = 1)
      : tool(tool), desc(desc) {
    bool verbatim = false;
    std::deque<std::string_view> values;
    std::deque<std::string_view> positional;
    for (int argNum = offset; argNum < argc; ++argNum) {
      ++numArgsRead;
      std::string_view arg = argv[argNum];
      if (!verbatim && arg == "--") {
        // Treat all remaining arguments as verbatim
        verbatim = true;
      } else if (!verbatim && arg.front() == '-' && arg.size() > 1) {
        // We have an option. Do we know it?
        std::string_view name = parseOptName(arg);
        OwnedOption &opt = options()[name];
        if (!opt) {
          std::cerr << "Encountered unknown option " << arg << '\n';
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
          // If this is the final option, only break to give positional
          // arguments collected so far the chance to be consumed by
          // a corresponding option
          if (opt->isEnd())
            break;
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
        numArgsRead += *res;
        positional.insert(positional.end(), values.begin(), values.end());
        values.clear();
        if (opt->isEnd())
          break;
      } else
        positional.push_back(arg);
    }
    // Parse positional arguments
    if (positional.size()) {
      OwnedOption &eatAll = options()[""];
      if (!eatAll) {
        std::cerr << "Too many positional arguments given:\n";
        for (const auto &arg : positional)
          std::cerr << arg << '\n';
        bail();
      }
      auto res = eatAll->parse(positional);
      if (!res)
        bail();
      if (0 > *res || *res > positional.size())
        svg_unreachable("Illegal number of values read by option");
      numArgsRead += *res;
      if (eatAll->isEnd())
        return;
      if (*res != positional.size()) {
        std::cerr << "Too many positional arguments given:\n";
        for (auto It = positional.begin() + *res, End = positional.end();
             It != End; ++It)
          std::cerr << *It << '\n';
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
    std::cout << "\n\n";
    std::cout << "Options:\n";
    for (const auto &KeyValuePair : options()) {
      const OwnedOption &opt = KeyValuePair.second;
      if (opt == eatAll)
        continue;
      opt->display(std::cout);
      std::cout << '\n';
    }
  }

  int getNumArgsRead() const { return numArgsRead; }

  /// Add an option to the ParseArg namespace denoted by AppTag.
  static void addOption(std::string_view name, OwnedOption opt) {
    assert(!options().count(name) && "Registered option more than once");
    options()[name] = std::move(opt);
  }
  static void removeOption(std::string_view name) {
    assert(options().count(name) && "Tried to remove unregistered option");
    options().erase(name);
  }

private:
  const char *tool;
  const char *desc;
  int numArgsRead = 0;

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
    std::cout << '\n';
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
void CliOptBase<DerivedTy, DecayTy>::registerWithApp(DerivedTy &opt,
                                                     bool no_unreg) {
  if (no_unreg) {
    for (const auto &name : opt.names)
      ParseArgs<AppTag>::addOption(
          name, std::make_unique<CliOptModel<DerivedTy>>(opt));
    if (opt.names.empty())
      ParseArgs<AppTag>::addOption(
          "", std::make_unique<CliOptModel<DerivedTy>>(opt));
  } else {
    for (const auto &name : opt.names)
      ParseArgs<AppTag>::removeOption(name);
    if (opt.names.empty())
      ParseArgs<AppTag>::removeOption("");
    // return to unregistered state to prevent further deregistrations
    // using this registrator function
    opt.registrator = nullptr;
  }
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
    std::cerr << ": " << values.front() << '\n';
    return std::nullopt;
  }
  // Assign takes rvalue, so we need to be slightly stupid here
  assign(*parsed && *parsed);
  return 1;
}
} // namespace cl
} // namespace svg
#endif // SVGUTILS_UTILS_CLI_ARGS_H
