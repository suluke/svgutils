#include "svgutils/cli_args.h"
#include "svgutils/tedisex_config.h"

#include <pstreams/pstream.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <optional>
#include <regex>
#include <sstream>
#include <thread>
#include <type_traits>
#include <variant>

using namespace svg;
namespace fs = std::filesystem;

static cl::list<std::string> Consider(cl::name("filter"),
                                      cl::init(TEDISEX_CONSIDER_PATTERNS));
static cl::list<std::string> Defs(cl::name("param"), cl::name("D"));
static cl::list<std::string> Ignore(cl::name("ignore"), cl::name("i"),
                                    cl::init(TEDISEX_IGNORE_PATTERNS));
static cl::opt<fs::path> Outdir(cl::name("outdir"), cl::name("o"),
                                cl::init(fs::path("./tedisex")));
static cl::opt<bool> Recurse(cl::name("recursive"), cl::name("r"),
                             cl::init(true));
static cl::list<fs::path> Testsuites(cl::meta("Tests"));
static cl::opt<unsigned> Threads(cl::name("threads"), cl::name("j"),
                                 cl::init(std::thread::hardware_concurrency()));
static cl::opt<unsigned> Timeout(cl::name("timeout"), cl::init(0));
static cl::opt<bool> Verbose(cl::name("verbose"), cl::name("v"),
                             cl::init(false));

static const char *TOOLNAME = "tedisex";
static const char *TOOLDESC = "TEst DIScovery and EXecution tool";

struct error {
  error() = delete;
  error(fs::path test, std::string msg)
      : test(std::move(test)), msg(std::move(msg)) {}
  error(const error &) = default;
  error(error &&) = default;
  error &operator=(const error &) = default;
  error &operator=(error &&) = default;

  void print(std::ostream &os) {
    os << "Error in test " << test << ":\n" << msg << '\n';
  }

private:
  fs::path test;
  std::string msg;
};

template <typename T> struct error_or {
  template <typename... args_t>
  error_or(args_t &&... args) : value(std::forward<args_t>(args)...) {}
  operator bool() const { return !std::holds_alternative<error>(value); }
  T &operator*() {
    assert(*this && "Cannot extract value from error_or in error state");
    return std::get<std::remove_reference_t<T>>(value);
  }
  error &to_error() { return static_cast<error &>(*this); }
  explicit operator T &() { return **this; }
  explicit operator error &() {
    assert(!*this && "Cannot cast to error in non-error state");
    return std::get<error>(value);
  }

private:
  std::variant<error, std::remove_reference_t<T>> value;
};

template <> struct error_or<void> {
  template <typename... args_t>
  error_or(args_t &&... args) : err(error(std::forward<args_t>(args)...)) {}
  error_or() = default;
  operator bool() const { return !err; }
  error &to_error() { return static_cast<error &>(*this); }
  explicit operator error &() {
    assert(!*this && "Cannot cast to error in non-error state");
    return *err;
  }

private:
  std::optional<error> err;
};

/// Describes the contents of a test file.
struct Test {
  explicit Test(const fs::path &testfile) : testfile(testfile) {}
  Test() = delete;
  Test(const Test &) = delete;
  Test &operator=(const Test &) = delete;
  Test(Test &&) = default;
  Test &operator=(Test &&) = default;

  const fs::path &getFile() const { return testfile; }

  std::vector<std::string> run, xfail, required, unsupported;

private:
  fs::path testfile;
};

/// The observable results from running a single command line of a Test
struct TestResult {
  enum Type { PASS, XFAIL, XPASS, FAIL, UNRESOLVED, UNSUPPORTED };
  friend inline std::ostream &operator<<(std::ostream &os, Type type) {
    switch (type) {
    case PASS:
      os << "[PASS       ]";
      break;
    case XFAIL:
      os << "[XFAIL      ]";
      break;
    case XPASS:
      os << "[XPASS      ]";
      break;
    case FAIL:
      os << "[FAIL       ]";
      break;
    case UNRESOLVED:
      os << "[UNRESOLVED ]";
      break;
    case UNSUPPORTED:
      os << "[UNSUPPORTED]";
      break;
    };
    return os;
  }

  TestResult() = delete;
  template <typename T1, typename T2>
  TestResult(Type type, std::string command, int status, T1 &&stdout,
             T2 &&stderr)
      : type(type), command(command), status(status),
        stdout(std::forward<T1>(stdout)), stderr(std::forward<T2>(stderr)) {}
  TestResult(const TestResult &) = delete;
  TestResult &operator=(const TestResult &) = delete;
  TestResult(TestResult &&) = default;
  TestResult &operator=(TestResult &&) = default;

  Type type;
  std::string command;
  int status;
  std::string stdout, stderr;

  friend inline std::ostream &operator<<(std::ostream &os,
                                         const TestResult &res) {
    os << res.type << "\n  Command: " << res.command << '\n'
       << "  Exit code: " << res.status << '\n'
       << "  <<<<<<<<<<<<<<<<<<Stdout>>>>>>>>>>>>>>>>>>\n"
       << res.stdout << '\n'
       << "  <<<<<<<<<<<<<<<<END Stdout>>>>>>>>>>>>>>>>\n"
       << "  <<<<<<<<<<<<<<<<<<Stderr>>>>>>>>>>>>>>>>>>\n"
       << res.stderr << '\n'
       << "  <<<<<<<<<<<<<<<<END Stderr>>>>>>>>>>>>>>>>\n";
    return os;
  }
};
/// A simple container class to hold a TestResult for each of a Test's
/// RUN lines.
struct TestResults {
  fs::path test;
  std::vector<TestResult> results;

  template <typename... args_t> void emplace_back(args_t &&... args) {
    results.emplace_back(std::forward<args_t>(args)...);
  }
  auto begin() { return results.begin(); }
  auto end() { return results.end(); }
  auto begin() const { return results.begin(); }
  auto end() const { return results.end(); }
  friend inline std::ostream &operator<<(std::ostream &os,
                                         const TestResults &results) {
    for (const auto &res : results)
      os << res;
    return os;
  }
};

/// Responsible for assigning the temp-file (%t/%T) path to tests.
struct TempManager {
  TempManager() = delete;
  TempManager(const TempManager &) = delete;
  TempManager &operator=(const TempManager &) = delete;
  TempManager(TempManager &&) = default;
  TempManager &operator=(TempManager &&) = default;
  TempManager(const fs::path &outDir, const std::vector<fs::path> &tests)
      : outDir(outDir) {
    for (const fs::path &test : tests) {
      fs::path normal = normalize(test);
      fs::path dir = normal.parent_path();
      if (commonRootDir.empty())
        commonRootDir = dir;
      else {
        while (commonRootDir != dir) {
          if (commonRootDir > dir)
            commonRootDir = commonRootDir.parent_path();
          else
            dir = dir.parent_path();
        }
      }
    }
  }
  const fs::path &getTempFor(const fs::path &path) {
    fs::path rel = fs::relative(path, commonRootDir);
    // better lock the whole temps map when working with it now
    std::lock_guard<std::mutex> lock(mutex);
    if (!temps.count(rel)) {
      rel += ".out";
      fs::path tmp = outDir / rel;
      fs::create_directories(tmp.parent_path());
      temps[rel] = tmp;
    }
    return temps.at(rel);
  }

private:
  fs::path normalize(const fs::path &path) {
    fs::path normal = path;
    if (normal.is_relative())
      normal = fs::absolute(normal);
    normal = normal.lexically_normal();
    return normal;
  }

  std::mutex mutex;
  fs::path commonRootDir;
  fs::path outDir;
  std::map<fs::path, fs::path> temps;
};

/// Scans a given testfile for tedisex keywords and constructs a Test
/// from it.
static error_or<Test> parse_test(const fs::path &testpath) {
  Test test{testpath};
  std::string line;
  std::fstream input(testpath.c_str(), input.in);
  if (!input)
    return error(testpath, "Error opening file");
  bool runMultiLine = false;
  const char RunStr[] = "RUN:";
  const char XfailStr[] = "XFAIL:";
  const char RequiresStr[] = "REQUIRES:";
  const char UnsupportedStr[] = "UNSUPPORTED:";
  for (;;) {
    std::getline(input, line);
    if (auto pos = line.find(RunStr); pos != line.npos) {
      line = line.substr(pos + sizeof(RunStr) - 1);
      if (runMultiLine)
        test.run.back().append(line);
      else
        test.run.emplace_back(line);
      if (line.back() == '\\')
        runMultiLine = true;
    } else if (auto pos = line.find(XfailStr); pos != line.npos) {
      line = line.substr(pos + sizeof(XfailStr) - 1);
      test.xfail.emplace_back(line);
    } else if (auto pos = line.find(RequiresStr); pos != line.npos) {
      line = line.substr(pos + sizeof(RequiresStr) - 1);
      test.required.emplace_back(line);
    } else if (auto pos = line.find(UnsupportedStr); pos != line.npos) {
      line = line.substr(pos + sizeof(UnsupportedStr) - 1);
      test.unsupported.emplace_back(line);
    }
    if (input.bad())
      return error(testpath, "Error reading file");
    if (input.eof())
      break;
    if (input.fail())
      return error(testpath, "Error reading file");
  }
  return test;
}

/// Substitutes built-in variables (s,S,p,t,T,{pathsep}) in a given
/// command.
static error_or<std::string> substitute_vars(const std::string_view &str,
                                             const fs::path &testpath,
                                             TempManager &tempman) {
  // TODO substitute user-provided variables as well
  std::stringstream subst;
  for (size_t i = 0; i < str.size(); ++i) {
    char c1 = str[i];
    if (c1 != '%') {
      subst << c1;
      continue;
    }
    if (i + 1 >= str.size())
      return error(testpath, "Unexpected end of input after %-sign");
    char c2 = str[i + 1];
    size_t extra = 1; // number of chars consumed beyond %
    if (c2 == 's')
      subst << testpath.c_str();
    else if (c2 == 'S' || c2 == 'p')
      subst << testpath.parent_path().c_str();
    else if (c2 == 't')
      subst << tempman.getTempFor(testpath).c_str();
    else if (c2 == 'T')
      subst << tempman.getTempFor(testpath).parent_path().c_str();
    else if (c2 == '%')
      subst << '%';
    else if (c2 == '{') {
      // We expect %{pathsep} ~> 10 characters
      if (i + 10 > str.size())
        return error(testpath,
                     "Unexpected end of input after %{, expected %{pathsep}");
      if (str.substr(i, 10) != "%{pathsep}")
        return error(testpath, std::string("Expected %{pathsep}, got ") +
                                   std::string(str.substr(i, 10)));
      subst << fs::path::preferred_separator;
      extra = 9;
    }
    i += extra;
  }
  return subst.str();
}

/// Parses and executes a test.
static error_or<TestResults> run_test(const fs::path &testpath,
                                      TempManager &tempman) {
  auto test_or_error = parse_test(testpath);
  if (!test_or_error)
    return std::move(test_or_error.to_error());
  const Test &test = *test_or_error;
  if (test.run.empty())
    return error(testpath, "Test has no RUN lines");
  // TODO check that current configuration matches xfail/required/unsupported
  using namespace redi;
  TestResults results;
  results.test = testpath;
  for (const std::string &cmd : test.run) {
    auto subst_or_error = substitute_vars(cmd, testpath, tempman);
    if (!subst_or_error)
      return subst_or_error.to_error();
    const std::string &subst = *subst_or_error;
    ipstream exe(subst, pstreams::pstdout | pstreams::pstderr);
    std::array<char, 1024> buf;
    std::string stdout, stderr;
    bool stdout_eof = false, stderr_eof = false;
    while (!stdout_eof || !stderr_eof) {
      if (!stderr_eof) {
        exe.err();
        while (auto strsize = exe.readsome(buf.data(), buf.size()))
          stderr += std::string_view(buf.data(), strsize);
        if (exe.eof()) {
          stderr_eof = true;
          if (!stdout_eof)
            exe.clear();
        }
      }
      if (!stdout_eof) {
        exe.out();
        while (auto strsize = exe.readsome(buf.data(), buf.size()))
          stdout += std::string_view(buf.data(), strsize);
        if (exe.eof()) {
          stdout_eof = true;
          if (!stderr_eof)
            exe.clear();
        }
      }
    }
    exe.close();
    pstreambuf *exebuf = exe.rdbuf();
    if (exebuf->status()) {
      std::stringstream msg;
      msg << "Exited with code " << exebuf->status() << '\n'
          << "Command was: " << subst << '\n'
          << "stdout: " << stdout << '\n'
          << "stderr: " << stderr << '\n';
      return error(testpath, msg.str());
    }
    results.emplace_back(TestResult::PASS, subst, exebuf->status(),
                         std::move(stdout), std::move(stderr));
  }
  return results;
}

/// This should be considered as the main function of this program.
/// It manages the multi-threaded execution of the given test files.
static bool run_tests(const std::vector<fs::path> &tests) {
  bool success = true;
  TempManager tmpman(*Outdir, tests);

  // TODO use the results from run_test calls (e.g. in verbose mode)
  if (Threads < 2)
    for (const fs::path &test : tests) {
      auto testresult = run_test(test, tmpman);
      if (!testresult) {
        testresult.to_error().print(std::cerr);
        success = false;
      } else if (Verbose) {
        std::cerr << *testresult;
      }
    }
  else {
    std::mutex testlist_mutex, outstream_mutex;
    std::vector<const fs::path *> testlist;
    for (const fs::path &test : tests)
      testlist.emplace_back(&test);
    auto execute = [&tmpman, &testlist, &testlist_mutex, &outstream_mutex,
                    &success]() {
      const fs::path *test;
      while (true) {
        {
          std::lock_guard<std::mutex> lock{testlist_mutex};
          if (testlist.empty())
            return;
          test = testlist.back();
          testlist.pop_back();
        }
        auto testresult = run_test(*test, tmpman);
        if (!testresult) {
          std::lock_guard<std::mutex> lock{outstream_mutex};
          testresult.to_error().print(std::cerr);
          success = false;
        } else if (Verbose) {
          std::lock_guard<std::mutex> lock{outstream_mutex};
          std::cerr << *testresult;
        }
      }
    };
    std::vector<std::thread> threads;
    for (unsigned i = 1 /* main thread also counts */; i < Threads; ++i)
      threads.emplace_back(execute);
    execute();
    for (std::thread &thread : threads)
      thread.join();
  }
  return success;
}

/// Check if the shell-glob-like @p pattern matches the given path.
static bool match(const fs::path &path, const std::string_view &pattern) {
  std::stringstream patcvt;
  // Substitutions:
  // ** => .*
  // * => [^\/]*
  // Other regex special characters: .+\()[]{}^$?!|
  for (size_t i = 0; i < pattern.size(); ++i) {
    const char c1 = pattern[i];
    const char c2 = (i + 1 >= pattern.size()) ? 0 : pattern[i + 1];
    if (c1 == '.' || c1 == '+' || c1 == '\\' || c1 == '(' || c1 == ')' ||
        c1 == '[' || c1 == ']' || c1 == '{' || c1 == '}' || c1 == '^' ||
        c1 == '$' || c1 == '?' || c1 == '!' || c1 == '|')
      patcvt << '\\' << c1;
    else if (c1 == '*') {
      if (c2 == '*') {
        patcvt << ".*";
        ++i;
      } else
        patcvt << "[^\\/]*";
    } else
      patcvt << c1;
  }
  // make the pattern only match at the end
  patcvt << '$';
  std::string pat = patcvt.str();
  std::regex regex(pat.c_str());
  std::match_results<const char *> matches;
  std::string pathstr = path.native();
  return std::regex_search(pathstr.c_str(), pathstr.c_str() + pathstr.size(),
                           matches, regex);
}

/// Matches the given path against the list of Ignore and Consider
/// patterns.
static bool is_valid_path(const fs::path &p) {
  bool isDirectory = fs::is_directory(p);
  for (const std::string &ignored : Ignore) {
    std::string_view pat = ignored;
    if (pat.back() == '/') {
      if (!isDirectory)
        continue;
      pat = pat.substr(0, pat.size() - 1);
    }
    if (match(p, pat)) {
      return false;
    }
  }
  // 'consider' regexes only apply to files
  if (isDirectory)
    return true;
  for (const std::string &considered : Consider) {
    std::string_view pat = considered;
    if (pat.back() == '/') {
      if (!isDirectory)
        continue;
      pat = pat.substr(0, pat.size() - 1);
    }
    if (match(p, pat)) {
      return true;
    }
  }
  // If there is at least one pattern in Consider, then none of these
  // matched this path and therefore the path shouldn't be considered.
  // If there are no considered patterns, then all paths are valid as
  // long as they aren't explicitly Ignore'd
  return Consider->empty();
}

/// (Recursively) inspects the given path for valid test files.
static void search_test_path(const fs::path &testpath,
                             /* out */ std::vector<fs::path> &collection) {
  if (fs::is_directory(testpath))
    for (const fs::directory_entry &entry : fs::directory_iterator(testpath)) {
      if (is_valid_path(entry)) {
        if (entry.is_directory() && Recurse)
          search_test_path(entry, collection);
        else
          collection.emplace_back(entry);
      }
    }
  else if (is_valid_path(testpath))
    collection.emplace_back(testpath);
}

static std::vector<fs::path> find_tests() {
  std::vector<fs::path> tests;
  for (const fs::path &suite : Testsuites)
    search_test_path(suite, tests);
  return tests;
}

int main(int argc, const char **argv) {
  cl::ParseArgs(TOOLNAME, TOOLDESC, argc, argv);
  // Use the current directory as testsuite root if nothing else was
  // specified
  if (Testsuites->empty())
    Testsuites->emplace_back(fs::current_path());
  const std::vector<fs::path> tests = find_tests();
  if (tests.empty()) {
    std::cerr << "No test files found\n";
    return 1;
  }
  if (!run_tests(tests)) {
    std::cerr << "Some tests failed\n";
    return 1;
  }
  return 0;
}
