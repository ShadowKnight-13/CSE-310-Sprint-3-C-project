# C++ Quizzer

A simple C++ console quiz application that lets you test yourself with custom question-and-answer pairs.

The project includes:
- A menu-driven quiz experience.
- Persistent storage in a plain text file so your questions are saved between runs.
- Seeded sample questions focused on C++ concepts.

## Instructions for Build and Use

### Build (CMake)

1. Open a terminal in the project folder.
2. Configure CMake:

```powershell
cmake -S . -B build
```

3. Build:

```powershell
cmake --build build --config Release
```

### Run

If you are on Windows with Visual Studio generator:

```powershell
.\build\Release\CppQuizzer.exe
```

If CMake uses Ninja or Makefiles:

```powershell
.\build\CppQuizzer.exe
```

### How to Use the App

1. Start the program.
2. Choose one of the menu options:
	- `1` Take quiz
	- `2` Add a question
	- `3` List questions
	- `4` Exit
3. Add your own questions and answers anytime using option `2`.
4. Your entries are saved in `questions.txt`.

## Question File Format

The app stores data in `questions.txt` using this format:

```text
Question text|Answer text
```

Example:

```text
Which header defines std::vector?|<vector>
```

Notes:
- Empty lines are ignored.
- Lines starting with `#` are treated as comments.
- `|` and `\` are escaped automatically when saved by the program.

## Development Environment

To recreate the development environment, install:

- C++ compiler with C++17 support (MSVC, GCC, or Clang)
- CMake 3.16+
- VS Code (optional, recommended)

## Useful Websites to Learn More

- [C++ Reference](https://en.cppreference.com/)
- [CMake Tutorial](https://cmake.org/cmake/help/latest/guide/tutorial/index.html)
- [Microsoft C++ Documentation](https://learn.microsoft.com/cpp/)

## Future Work

- [ ] Add multiple-choice question support
- [ ] Accept multiple valid answers per question
- [ ] Track quiz history and progress over time