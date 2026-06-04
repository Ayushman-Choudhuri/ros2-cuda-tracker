# Contributing

## Code Style

This project follows the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html). The rules below summarize the conventions enforced in this codebase.

### Formatting

- **Indent:** 4 spaces (no tabs)
- **Column limit:** 100 characters
- **Brace style:** opening brace on same line as declaration
- **Pointer/reference alignment:** left (`int* ptr`, `int& ref`)

Enforced via `clang-format`. Run before committing:

```bash
clang-format -i src/*.cpp include/*.hpp
```

Static analysis via `clang-tidy` checks: `readability-*`, `modernize-*` (excluding alpha).

---

### Naming Conventions

#### Variables

`snake_case` for all local variables and function parameters.

```cpp
int frame_count = 0;
double time_elapsed = 0.0;
```

#### Member Variables

`snake_case` with trailing underscore `_`.

```cpp
class Camera {
    int device_id_;
    double current_fps_;
    cv::VideoCapture frame_capture_;
};
```

#### Constants

`kPascalCase` — `k` prefix marks compile-time constants. Applies to `constexpr` variables and `static const` members.

```cpp
static constexpr int kDefaultWidth = 640;
static constexpr double kAlphaFps = 0.1;
```

Do **not** use `#define` for constants. Use `constexpr` instead — it is typed, scoped, and evaluated at compile time.

#### Functions and Methods

`PascalCase` for all functions and class methods.

```cpp
bool Initialize();
cv::Mat GetFrame();
void AnnotateFrame(cv::Mat* frame);
```

#### Classes and Structs

`PascalCase`.

```cpp
class Camera { ... };
class TensorRTInference { ... };
```

#### Enums

`PascalCase` for the enum type. `kPascalCase` for enumerators (same as constants).

```cpp
enum class PipelineState {
    kUninitialized,
    kRunning,
    kStopped,
};
```

#### Macros

`ALL_CAPS_WITH_UNDERSCORES`. Avoid macros where `constexpr` or `inline` functions suffice.

```cpp
#define DEFAULT_DEVICE_ID 4
```

#### Files

- Headers: `snake_case.hpp`
- Sources: `snake_case.cpp`
- Match header and source names: `camera.hpp` / `camera.cpp`

---

### C++ Specifics

#### Use `constexpr` over `#define` for constants

```cpp
// bad
#define DEFAULT_WIDTH 640

// good
static constexpr int kDefaultWidth = 640;
```

#### Use member initializer lists

```cpp
// preferred
Camera::Camera(int device_id) : device_id_(device_id) {}

// avoid
Camera::Camera(int device_id) {
    device_id_ = device_id;  // default-constructs then assigns
}
```

#### Pass output parameters as pointers, not non-const references

```cpp
void AnnotateFrame(cv::Mat* frame);  // caller sees mutation is possible
```

#### Mark single-argument constructors `explicit`

Prevents implicit conversions.

```cpp
explicit Camera(int device_id = 0);
```

#### Use `const` on methods that do not mutate state

```cpp
bool IsOpened() const;
double GetFps() const;
```

#### Standard: C++23

All new code targets C++23. Use modern features where they improve clarity (`std::format`, ranges, `[[nodiscard]]`, etc.).

---

## Build

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

Use the devcontainer for camera access — it mounts `/dev` and the X11 socket.

---

## Commit Messages

Use [Conventional Commits](https://www.conventionalcommits.org/):

```
feat: add TensorRT inference node
fix: correct EMA alpha application in GetFrame
chore: update clang-format config
```

Subject line ≤ 72 characters. Body only when the *why* is non-obvious from the diff.

---

## Pull Requests

- Branch from `main`, name branches `feat/`, `fix/`, `chore/` etc.
- All CI checks must pass before merge.
- TRT engines are SM-specific — do **not** commit `.engine` files. Commit `.onnx` files only.
