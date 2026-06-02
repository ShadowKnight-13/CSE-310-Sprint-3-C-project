# C++ Quizzer

A simple C++ console quiz application that lets you test yourself with custom question-and-answer pairs.

The project includes:
- A menu-driven quiz experience.
- Persistent storage in a plain text file so your questions are saved between runs.
- Persistent quiz history/progress tracking across runs.
- Seeded sample questions focused on C++ concepts.
- Support for text, multiple-choice, and multiple-answer questions.
- Optional timed mode with total quiz and per-question time limits.
- Timed difficulty presets (`easy`, `medium`, `hard`, `custom`) plus pause/resume support.

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
	- `4` Edit a question
	- `5` Delete a question
	- `6` View statistics
	- `7` Exit
3. Add your own questions and answers anytime using option `2`.
4. Your entries are saved in `questions.txt`.
5. In quiz mode, choose `Timed` to enforce both a total quiz timer and per-question timer.
6. Use `View statistics` to see progress trends and recent quiz history.
7. During timed quizzes, type `pause` to pause and resume timers.

## Quiz History File Format

The app stores run history in `quiz_history.txt` using this format:

```text
epochSeconds|score|correct|attempted|totalQuestions|durationSeconds|hintsUsed|mode|timedOutOverall|difficulty
```

Example:

```text
1780405741|80|8|10|10|95|1|normal|0|normal
1780405909|66.6667|6|9|10|60|0|timed|1|hard
```

## Question File Format

The app stores data in `questions.txt` using this format:

```text
Question text|Answer text|Hint text|Type|Options
```

Example:

```text
Which header defines std::vector?|<vector>|The angle brackets contain the header name...|text|
What is the default access level for class members in C++?|1||mc|public;private;protected
Which are C++ associative containers?|1,3||ma|vector;map;set
```

Notes:
- Empty lines are ignored.
- Lines starting with `#` are treated as comments.
- `|` and `\` are escaped automatically when saved by the program.
- `Type` can be `text`, `mc`, or `ma`.
- For `text`, `Answer text` can include multiple valid answers separated by semicolons (example: `shared_ptr;std::shared_ptr`).
- For `mc` and `ma`, `Answer text` stores 1-based option index values (for example `2` or `1,3`).
- For `mc` and `ma`, `Options` are separated by semicolons.

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

- [ ] Add achievement badges and streak tracking