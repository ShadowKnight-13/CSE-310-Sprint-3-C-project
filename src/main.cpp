#include <algorithm>
#include <cctype>
#include <chrono>
#ifdef _WIN32
#include <conio.h>
#endif
#include <ctime>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

enum class QuestionType {
    FreeText,
    MultipleChoice,
    MultipleAnswer
};

struct Question {
    std::string prompt;
    std::string answer;
    std::string hint;
    QuestionType type = QuestionType::FreeText;
    std::vector<std::string> options;
};

struct QuizStats {
    double bestScore = 0.0;
    int totalQuizzes = 0;
    double averageScore = 0.0;
    int totalCorrect = 0;
    int totalAttempted = 0;
};

struct QuizTiming {
    bool timedMode = false;
    int totalSeconds = 0;
    int perQuestionSeconds = 0;
    std::string difficulty = "normal";
};

struct TimedInputResult {
    bool receivedInput = false;
    bool timedOut = false;
    std::string input;
};

struct QuizHistoryEntry {
    long long epochSeconds = 0;
    double score = 0.0;
    int correct = 0;
    int attempted = 0;
    int totalQuestions = 0;
    int durationSeconds = 0;
    int hintsUsed = 0;
    bool timedMode = false;
    bool timedOutOverall = false;
    std::string difficulty = "normal";
};

namespace {

const std::string kQuestionFile = "questions.txt";
const std::string kStatsFile = "quiz_stats.txt";
const std::string kHistoryFile = "quiz_history.txt";

void clearScreen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void printSeparator(int width = 40) {
    for (int i = 0; i < width; ++i) {
        std::cout << "=";
    }
    std::cout << "\n";
}

std::string trim(const std::string& input) {
    std::size_t start = 0;
    while (start < input.size() && std::isspace(static_cast<unsigned char>(input[start]))) {
        ++start;
    }

    std::size_t end = input.size();
    while (end > start && std::isspace(static_cast<unsigned char>(input[end - 1]))) {
        --end;
    }

    return input.substr(start, end - start);
}

std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string escapePipes(const std::string& input) {
    std::string output;
    output.reserve(input.size());

    for (char ch : input) {
        if (ch == '\\' || ch == '|') {
            output.push_back('\\');
        }
        output.push_back(ch);
    }

    return output;
}

std::string unescapePipes(const std::string& input) {
    std::string output;
    output.reserve(input.size());

    bool escapeNext = false;
    for (char ch : input) {
        if (escapeNext) {
            output.push_back(ch);
            escapeNext = false;
            continue;
        }

        if (ch == '\\') {
            escapeNext = true;
            continue;
        }

        output.push_back(ch);
    }

    if (escapeNext) {
        output.push_back('\\');
    }

    return output;
}

std::vector<std::string> splitEscapedFields(const std::string& line) {
    std::vector<std::string> fields;
    std::string current;
    bool escaped = false;

    for (char ch : line) {
        if (escaped) {
            current.push_back(ch);
            escaped = false;
            continue;
        }

        if (ch == '\\') {
            escaped = true;
            continue;
        }

        if (ch == '|') {
            fields.push_back(unescapePipes(trim(current)));
            current.clear();
            continue;
        }

        current.push_back(ch);
    }

    if (escaped) {
        current.push_back('\\');
    }
    fields.push_back(unescapePipes(trim(current)));

    return fields;
}

std::vector<std::string> splitAndTrim(const std::string& input, char delimiter) {
    std::vector<std::string> parts;
    std::string current;

    for (char ch : input) {
        if (ch == delimiter) {
            std::string piece = trim(current);
            if (!piece.empty()) {
                parts.push_back(piece);
            }
            current.clear();
            continue;
        }
        current.push_back(ch);
    }

    std::string piece = trim(current);
    if (!piece.empty()) {
        parts.push_back(piece);
    }

    return parts;
}

std::string join(const std::vector<std::string>& values, const std::string& delimiter) {
    if (values.empty()) {
        return "";
    }

    std::string result;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
            result += delimiter;
        }
        result += values[i];
    }
    return result;
}

std::string questionTypeToStorage(QuestionType type) {
    if (type == QuestionType::MultipleChoice) {
        return "mc";
    }
    if (type == QuestionType::MultipleAnswer) {
        return "ma";
    }
    return "text";
}

QuestionType questionTypeFromStorage(const std::string& value) {
    std::string normalized = toLower(trim(value));
    if (normalized == "mc" || normalized == "multiple-choice" || normalized == "multiple_choice") {
        return QuestionType::MultipleChoice;
    }
    if (normalized == "ma" || normalized == "multiple-answer" || normalized == "multiple_answer") {
        return QuestionType::MultipleAnswer;
    }
    return QuestionType::FreeText;
}

std::string questionTypeLabel(QuestionType type) {
    if (type == QuestionType::MultipleChoice) {
        return "Multiple Choice";
    }
    if (type == QuestionType::MultipleAnswer) {
        return "Multiple Answer";
    }
    return "Text";
}

std::string optionLabel(std::size_t index) {
    if (index < 26) {
        return std::string(1, static_cast<char>('A' + static_cast<int>(index)));
    }
    return std::to_string(index + 1);
}

bool parseOptionSelections(
    const std::string& input,
    std::size_t optionCount,
    bool allowMultiple,
    std::vector<int>& outIndexes
) {
    outIndexes.clear();

    std::string normalized = input;
    for (char& ch : normalized) {
        if (ch == ';') {
            ch = ',';
        }
    }

    std::vector<std::string> tokens = splitAndTrim(normalized, ',');
    if (tokens.empty()) {
        return false;
    }

    if (!allowMultiple && tokens.size() != 1) {
        return false;
    }

    for (const std::string& rawToken : tokens) {
        std::string token = toLower(trim(rawToken));
        if (token.empty()) {
            return false;
        }

        int index = -1;
        if (token.size() == 1 && std::isalpha(static_cast<unsigned char>(token[0]))) {
            index = static_cast<int>(std::toupper(static_cast<unsigned char>(token[0])) - 'A');
        } else {
            bool allDigits = std::all_of(token.begin(), token.end(), [](unsigned char c) {
                return std::isdigit(c) != 0;
            });
            if (!allDigits) {
                return false;
            }

            int numeric = std::stoi(token);
            index = numeric - 1;
        }

        if (index < 0 || static_cast<std::size_t>(index) >= optionCount) {
            return false;
        }

        if (std::find(outIndexes.begin(), outIndexes.end(), index) == outIndexes.end()) {
            outIndexes.push_back(index);
        }
    }

    if (!allowMultiple && outIndexes.size() != 1) {
        return false;
    }

    if (outIndexes.empty()) {
        return false;
    }

    std::sort(outIndexes.begin(), outIndexes.end());
    return true;
}

std::string serializeOptionSelections(const std::vector<int>& indexes) {
    std::vector<std::string> parts;
    parts.reserve(indexes.size());

    for (int index : indexes) {
        parts.push_back(std::to_string(index + 1));
    }

    return join(parts, ",");
}

std::vector<std::string> parseFreeTextAnswers(const std::string& answerField) {
    std::vector<std::string> rawAnswers = splitAndTrim(answerField, ';');
    if (rawAnswers.empty()) {
        rawAnswers.push_back(trim(answerField));
    }

    std::vector<std::string> normalized;
    for (const std::string& value : rawAnswers) {
        std::string candidate = toLower(trim(value));
        if (!candidate.empty() &&
            std::find(normalized.begin(), normalized.end(), candidate) == normalized.end()) {
            normalized.push_back(candidate);
        }
    }

    return normalized;
}

std::string formatFreeTextExpected(const std::string& answerField) {
    std::vector<std::string> rawAnswers = splitAndTrim(answerField, ';');
    if (rawAnswers.empty()) {
        return answerField;
    }
    return join(rawAnswers, " | ");
}

std::string formatExpectedAnswer(const Question& question) {
    if (question.type == QuestionType::FreeText) {
        return formatFreeTextExpected(question.answer);
    }

    std::vector<int> indexes;
    if (!parseOptionSelections(
            question.answer,
            question.options.size(),
            question.type == QuestionType::MultipleAnswer,
            indexes)) {
        return question.answer;
    }

    std::vector<std::string> parts;
    for (int idx : indexes) {
        parts.push_back(
            optionLabel(static_cast<std::size_t>(idx)) + ") " + question.options[static_cast<std::size_t>(idx)]);
    }
    return join(parts, " | ");
}

bool isCorrectAnswer(const Question& question, const std::string& userAnswer) {
    if (question.type == QuestionType::FreeText) {
        std::string normalizedUser = toLower(trim(userAnswer));
        if (normalizedUser.empty()) {
            return false;
        }

        std::vector<std::string> validAnswers = parseFreeTextAnswers(question.answer);
        return std::find(validAnswers.begin(), validAnswers.end(), normalizedUser) != validAnswers.end();
    }

    std::vector<int> expected;
    std::vector<int> actual;
    bool allowMultiple = question.type == QuestionType::MultipleAnswer;

    if (!parseOptionSelections(question.answer, question.options.size(), allowMultiple, expected)) {
        return false;
    }
    if (!parseOptionSelections(userAnswer, question.options.size(), allowMultiple, actual)) {
        return false;
    }

    return expected == actual;
}

std::vector<Question> defaultQuestions() {
    return {
        {"What does RAII stand for in C++?", "Resource Acquisition Is Initialization", "Think about resources and initialization...", QuestionType::FreeText, {}},
        {"Which STL container stores unique keys in sorted order?", "set", "It's similar to map but only stores keys...", QuestionType::FreeText, {}},
        {"What keyword is used to derive one class from another?", "public", "This controls access visibility...", QuestionType::FreeText, {}},
        {"Which smart pointer allows shared ownership of an object?", "shared_ptr", "Multiple pointers can share one object...", QuestionType::FreeText, {}},
        {"What is the return type of main in standard C++?", "int", "It returns a status code to the OS...", QuestionType::FreeText, {}},
        {"Which operator is commonly overloaded for stream output?", "<<", "This is the insertion operator...", QuestionType::FreeText, {}},
        {"What does std::move enable in modern C++?", "Move semantics", "It transfers ownership efficiently...", QuestionType::FreeText, {}},
        {"Which header defines std::vector?", "<vector>", "The angle brackets contain the header name...", QuestionType::FreeText, {}},
        {"What C++ feature allows one function name to have multiple parameter signatures?", "Function overloading", "Multiple functions with same name...", QuestionType::FreeText, {}},
        {"What keyword prevents a method from being overridden in derived classes?", "final", "Stops inheritance in C++11+...", QuestionType::FreeText, {}}
    };
}

std::vector<Question> loadQuestions(const std::string& path) {
    std::vector<Question> questions;
    std::ifstream input(path);

    if (!input) {
        return questions;
    }

    std::string line;
    while (std::getline(input, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::vector<std::string> fields = splitEscapedFields(line);
        if (fields.size() < 2) {
            continue;
        }

        Question question;
        question.prompt = fields[0];
        question.answer = fields[1];
        question.hint = fields.size() >= 3 ? fields[2] : "";
        question.type = fields.size() >= 4 ? questionTypeFromStorage(fields[3]) : QuestionType::FreeText;
        question.options = fields.size() >= 5 ? splitAndTrim(fields[4], ';') : std::vector<std::string>{};

        if (question.type != QuestionType::FreeText) {
            bool allowMultiple = question.type == QuestionType::MultipleAnswer;
            std::vector<int> parsed;

            if (question.options.size() < 2 ||
                !parseOptionSelections(question.answer, question.options.size(), allowMultiple, parsed)) {
                question.type = QuestionType::FreeText;
                question.options.clear();
            }
        }

        if (!question.prompt.empty() && !question.answer.empty()) {
            questions.push_back(question);
        }
    }

    return questions;
}

bool saveQuestions(const std::string& path, const std::vector<Question>& questions) {
    std::ofstream output(path);
    if (!output) {
        return false;
    }

    for (const Question& question : questions) {
        output << escapePipes(question.prompt) << '|'
               << escapePipes(question.answer) << '|'
               << escapePipes(question.hint) << '|'
               << escapePipes(questionTypeToStorage(question.type)) << '|'
               << escapePipes(join(question.options, ";")) << '\n';
    }

    return true;
}

void printMenu() {
    printSeparator();
    std::cout << "C++ QUIZZER\n";
    printSeparator();
    std::cout << "1) Take quiz\n";
    std::cout << "2) Add a question\n";
    std::cout << "3) List questions\n";
    std::cout << "4) Edit a question\n";
    std::cout << "5) Delete a question\n";
    std::cout << "6) View statistics\n";
    std::cout << "7) Exit\n";
    printSeparator();
    std::cout << "Choose an option: ";
}

std::string promptLine(const std::string& prompt) {
    std::cout << prompt;
    std::string input;
    std::getline(std::cin, input);
    return trim(input);
}

TimedInputResult promptLineTimed(const std::string& prompt, int timeoutSeconds) {
    if (timeoutSeconds <= 0) {
        return {false, true, ""};
    }

    std::cout << prompt << " (" << timeoutSeconds << "s left): ";

    const auto start = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::seconds(timeoutSeconds);
    std::string input;

    while (std::chrono::steady_clock::now() - start < timeout) {
#ifdef _WIN32
        if (_kbhit()) {
            const int ch = _getch();

            if (ch == '\r' || ch == '\n') {
                std::cout << "\n";
                return {true, false, trim(input)};
            }

            if (ch == '\b') {
                if (!input.empty()) {
                    input.pop_back();
                    std::cout << "\b \b";
                }
                continue;
            }

            if (std::isprint(static_cast<unsigned char>(ch)) != 0) {
                input.push_back(static_cast<char>(ch));
                std::cout << static_cast<char>(ch);
            }
            continue;
        }
#else
        if (std::cin.rdbuf()->in_avail() > 0) {
            std::getline(std::cin, input);
            return {true, false, trim(input)};
        }
#endif
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "\n";
    return {false, true, ""};
}

int promptIntInRange(const std::string& prompt, int minValue, int maxValue) {
    while (true) {
        std::string input = promptLine(prompt);
        try {
            int value = std::stoi(input);
            if (value >= minValue && value <= maxValue) {
                return value;
            }
        } catch (...) {
        }
        std::cout << "Please enter a value between " << minValue << " and " << maxValue << ".\n";
    }
}

QuizTiming promptQuizTiming() {
    QuizTiming timing;
    std::string mode = promptLine("Quiz mode - 1) Normal  2) Timed: ");

    if (mode != "2") {
        return timing;
    }

    timing.timedMode = true;
    std::cout << "Timed difficulty presets:\n";
    std::cout << "  1) Easy   (10 min total, 45 sec/question)\n";
    std::cout << "  2) Medium (6 min total, 30 sec/question)\n";
    std::cout << "  3) Hard   (4 min total, 20 sec/question)\n";
    std::cout << "  4) Custom\n";

    std::string preset = promptLine("Choose timed difficulty: ");
    if (preset == "1") {
        timing.difficulty = "easy";
        timing.totalSeconds = 600;
        timing.perQuestionSeconds = 45;
    } else if (preset == "2") {
        timing.difficulty = "medium";
        timing.totalSeconds = 360;
        timing.perQuestionSeconds = 30;
    } else if (preset == "3") {
        timing.difficulty = "hard";
        timing.totalSeconds = 240;
        timing.perQuestionSeconds = 20;
    } else {
        timing.difficulty = "custom";
        timing.totalSeconds = promptIntInRange("Total quiz time in seconds (30-3600): ", 30, 3600);
        timing.perQuestionSeconds = promptIntInRange("Per-question time in seconds (5-300): ", 5, 300);
    }

    return timing;
}

QuestionType promptQuestionType(bool allowSkip, QuestionType currentType, bool& wasSkipped, bool& validChoice) {
    while (true) {
        std::cout << "Choose question type:\n";
        std::cout << "  1) Text answer\n";
        std::cout << "  2) Multiple choice (one correct option)\n";
        std::cout << "  3) Multiple answer (select all that apply)\n";

        std::string prompt = allowSkip
            ? "Enter type number (or press Enter to keep current): "
            : "Enter type number: ";
        std::string choice = promptLine(prompt);

        if (allowSkip && choice.empty()) {
            wasSkipped = true;
            validChoice = true;
            return currentType;
        }

        if (choice == "1") {
            wasSkipped = false;
            validChoice = true;
            return QuestionType::FreeText;
        }
        if (choice == "2") {
            wasSkipped = false;
            validChoice = true;
            return QuestionType::MultipleChoice;
        }
        if (choice == "3") {
            wasSkipped = false;
            validChoice = true;
            return QuestionType::MultipleAnswer;
        }

        std::cout << "Invalid type selection.\n";
        if (allowSkip) {
            std::string retry = promptLine("Try again? (y/n): ");
            if (toLower(retry) != "y") {
                wasSkipped = true;
                validChoice = false;
                return currentType;
            }
        }
    }
}

bool configureQuestionByType(Question& question, bool allowKeepExisting) {
    if (question.type == QuestionType::FreeText) {
        std::string answerPrompt = allowKeepExisting
            ? "Enter answer(s) separated by ';' (or press Enter to keep): "
            : "Enter the answer (use ';' for multiple valid answers): ";
        std::string answer = promptLine(answerPrompt);

        if (answer.empty()) {
            if (allowKeepExisting && !question.answer.empty()) {
                question.options.clear();
                return true;
            }
            std::cout << "Answer cannot be empty.\n";
            return false;
        }

        question.answer = answer;
        question.options.clear();
        return true;
    }

    bool keepOptions = false;
    if (allowKeepExisting && question.options.size() >= 2) {
        std::string keep = toLower(promptLine("Keep existing options? (y/n): "));
        keepOptions = (keep == "y");
    }

    if (!keepOptions) {
        std::string countStr = promptLine("How many options? (2-8): ");
        int optionCount = 0;
        try {
            optionCount = std::stoi(countStr);
        } catch (...) {
            std::cout << "Invalid number of options.\n";
            return false;
        }

        if (optionCount < 2 || optionCount > 8) {
            std::cout << "Option count must be between 2 and 8.\n";
            return false;
        }

        question.options.clear();
        for (int i = 0; i < optionCount; ++i) {
            std::string optionText = promptLine(
                "Option " + optionLabel(static_cast<std::size_t>(i)) + ": ");
            if (optionText.empty()) {
                std::cout << "Option text cannot be empty.\n";
                return false;
            }
            question.options.push_back(optionText);
        }
    }

    std::string selectionPrompt = (question.type == QuestionType::MultipleChoice)
        ? "Enter correct option (letter or number"
        : "Enter all correct options (comma-separated letters/numbers";
    if (allowKeepExisting) {
        selectionPrompt += ", or press Enter to keep): ";
    } else {
        selectionPrompt += "): ";
    }

    std::string selection = promptLine(selectionPrompt);
    if (allowKeepExisting && selection.empty()) {
        std::vector<int> existing;
        bool validExisting = parseOptionSelections(
            question.answer,
            question.options.size(),
            question.type == QuestionType::MultipleAnswer,
            existing);
        if (validExisting) {
            return true;
        }
        std::cout << "Existing correct answer is invalid. Please provide a new one.\n";
        selection = promptLine((question.type == QuestionType::MultipleChoice)
            ? "Enter correct option (letter or number): "
            : "Enter all correct options (comma-separated letters/numbers): ");
    }

    std::vector<int> selectedIndexes;
    if (!parseOptionSelections(
            selection,
            question.options.size(),
            question.type == QuestionType::MultipleAnswer,
            selectedIndexes)) {
        std::cout << "Invalid correct-answer selection.\n";
        return false;
    }

    question.answer = serializeOptionSelections(selectedIndexes);
    return true;
}

void listQuestions(const std::vector<Question>& questions) {
    if (questions.empty()) {
        std::cout << "No questions available yet.\n";
        return;
    }

    std::cout << "\n";
    printSeparator();
    std::cout << "QUESTIONS (" << questions.size() << ")\n";
    printSeparator();
    for (std::size_t i = 0; i < questions.size(); ++i) {
        std::cout << (i + 1) << ") [" << questionTypeLabel(questions[i].type)
                  << "] " << questions[i].prompt << "\n";
    }
    std::cout << "\n";
}

void addQuestion(std::vector<Question>& questions) {
    std::cout << "\n";
    printSeparator();
    std::cout << "ADD A QUESTION\n";
    printSeparator();

    Question question;
    question.prompt = promptLine("Enter the question: ");
    if (question.prompt.empty()) {
        std::cout << "Question cannot be empty.\n";
        return;
    }

    bool skipped = false;
    bool valid = false;
    question.type = promptQuestionType(false, QuestionType::FreeText, skipped, valid);
    if (!valid) {
        std::cout << "Question type is required.\n";
        return;
    }

    if (!configureQuestionByType(question, false)) {
        std::cout << "Question was not added due to invalid details.\n";
        return;
    }

    question.hint = promptLine("Enter a hint (optional, press Enter to skip): ");
    questions.push_back(question);

    if (saveQuestions(kQuestionFile, questions)) {
        std::cout << "Question saved.\n";
    } else {
        std::cout << "Question added in memory, but failed to save file.\n";
    }
}

void editQuestion(std::vector<Question>& questions) {
    if (questions.empty()) {
        std::cout << "No questions available.\n";
        return;
    }

    listQuestions(questions);
    std::string indexStr = promptLine("Enter question number to edit: ");

    try {
        int index = std::stoi(indexStr) - 1;
        if (index < 0 || index >= static_cast<int>(questions.size())) {
            std::cout << "Invalid question number.\n";
            return;
        }

        Question& question = questions[static_cast<std::size_t>(index)];

        std::cout << "\nCurrent question: " << question.prompt << "\n";
        std::string newPrompt = promptLine("Enter new question (or press Enter to keep): ");
        if (!newPrompt.empty()) {
            question.prompt = newPrompt;
        }

        std::cout << "Current type: " << questionTypeLabel(question.type) << "\n";
        bool skipped = false;
        bool valid = false;
        QuestionType newType = promptQuestionType(true, question.type, skipped, valid);
        if (!valid) {
            std::cout << "Edit canceled.\n";
            return;
        }
        if (!skipped) {
            question.type = newType;
        }

        if (!configureQuestionByType(question, true)) {
            std::cout << "Question details were not updated due to invalid input.\n";
            return;
        }

        std::cout << "Current hint: " << (question.hint.empty() ? "(none)" : question.hint) << "\n";
        std::string newHint = promptLine("Enter new hint (or press Enter to keep): ");
        if (!newHint.empty()) {
            question.hint = newHint;
        }

        if (saveQuestions(kQuestionFile, questions)) {
            std::cout << "Question updated.\n";
        }
    } catch (...) {
        std::cout << "Invalid input.\n";
    }
}

void deleteQuestion(std::vector<Question>& questions) {
    if (questions.empty()) {
        std::cout << "No questions available.\n";
        return;
    }

    listQuestions(questions);
    std::string indexStr = promptLine("Enter question number to delete: ");

    try {
        int index = std::stoi(indexStr) - 1;
        if (index < 0 || index >= static_cast<int>(questions.size())) {
            std::cout << "Invalid question number.\n";
            return;
        }

        std::cout << "Deleting: " << questions[static_cast<std::size_t>(index)].prompt << "\n";
        std::string confirm = promptLine("Are you sure? (y/n): ");
        if (toLower(confirm) == "y") {
            questions.erase(questions.begin() + index);
            if (saveQuestions(kQuestionFile, questions)) {
                std::cout << "Question deleted.\n";
            }
        }
    } catch (...) {
        std::cout << "Invalid input.\n";
    }
}

QuizStats loadStats(const std::string& path) {
    QuizStats stats;
    std::ifstream input(path);
    if (input) {
        input >> stats.bestScore >> stats.totalQuizzes >> stats.averageScore
              >> stats.totalCorrect >> stats.totalAttempted;
    }
    return stats;
}

bool saveStats(const std::string& path, const QuizStats& stats) {
    std::ofstream output(path);
    if (!output) {
        return false;
    }
    output << stats.bestScore << " " << stats.totalQuizzes << " "
           << stats.averageScore << " " << stats.totalCorrect << " "
           << stats.totalAttempted;
    return true;
}

std::vector<QuizHistoryEntry> loadHistory(const std::string& path) {
    std::vector<QuizHistoryEntry> history;
    std::ifstream input(path);
    if (!input) {
        return history;
    }

    std::string line;
    while (std::getline(input, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::vector<std::string> fields = splitEscapedFields(line);
        if (fields.size() < 9) {
            continue;
        }

        try {
            QuizHistoryEntry entry;
            entry.epochSeconds = std::stoll(fields[0]);
            entry.score = std::stod(fields[1]);
            entry.correct = std::stoi(fields[2]);
            entry.attempted = std::stoi(fields[3]);
            entry.totalQuestions = std::stoi(fields[4]);
            entry.durationSeconds = std::stoi(fields[5]);
            entry.hintsUsed = std::stoi(fields[6]);
            entry.timedMode = (toLower(fields[7]) == "timed");
            entry.timedOutOverall = (fields[8] == "1");
            entry.difficulty = (fields.size() >= 10) ? toLower(trim(fields[9])) : "normal";
            history.push_back(entry);
        } catch (...) {
        }
    }

    return history;
}

bool appendHistory(const std::string& path, const QuizHistoryEntry& entry) {
    std::ofstream output(path, std::ios::app);
    if (!output) {
        return false;
    }

    output << entry.epochSeconds << '|'
           << entry.score << '|'
           << entry.correct << '|'
           << entry.attempted << '|'
           << entry.totalQuestions << '|'
           << entry.durationSeconds << '|'
           << entry.hintsUsed << '|'
           << (entry.timedMode ? "timed" : "normal") << '|'
            << (entry.timedOutOverall ? "1" : "0") << '|'
            << entry.difficulty << '\n';
    return true;
}

std::string formatTimestamp(long long epochSeconds) {
    std::time_t timestamp = static_cast<std::time_t>(epochSeconds);
    std::tm localTime = {};
#ifdef _WIN32
    localtime_s(&localTime, &timestamp);
#else
    localtime_r(&timestamp, &localTime);
#endif

    std::ostringstream stream;
    stream << std::put_time(&localTime, "%Y-%m-%d %H:%M");
    return stream.str();
}

double averageHistoryRange(
    const std::vector<QuizHistoryEntry>& history,
    std::size_t start,
    std::size_t end
) {
    if (start >= end || end > history.size()) {
        return 0.0;
    }

    double total = 0.0;
    for (std::size_t i = start; i < end; ++i) {
        total += history[i].score;
    }
    return total / static_cast<double>(end - start);
}

void showStats() {
    QuizStats stats = loadStats(kStatsFile);
    std::vector<QuizHistoryEntry> history = loadHistory(kHistoryFile);

    std::cout << "\n";
    printSeparator();
    std::cout << "STATISTICS\n";
    printSeparator();

    if (stats.totalQuizzes == 0) {
        std::cout << "No quizzes taken yet.\n";
    } else {
        std::cout << "Total Quizzes: " << stats.totalQuizzes << "\n";
        std::cout << "Best Score: " << stats.bestScore << "%\n";
        std::cout << "Average Score: " << stats.averageScore << "%\n";
        std::cout << "Total Correct: " << stats.totalCorrect << "\n";
        std::cout << "Total Attempted: " << stats.totalAttempted << "\n";
    }

    std::cout << "\n";
    printSeparator();
    std::cout << "PROGRESS TRACKER\n";
    printSeparator();

    if (history.empty()) {
        std::cout << "No quiz history recorded yet.\n";
    } else {
        const QuizHistoryEntry& latest = history.back();
        std::cout << "History Entries: " << history.size() << "\n";
        std::cout << "Latest Run: " << formatTimestamp(latest.epochSeconds) << "\n";
        std::cout << "Latest Score: " << latest.score << "% ("
                  << latest.correct << "/" << latest.attempted << ")\n";

        if (history.size() >= 2) {
            const QuizHistoryEntry& previous = history[history.size() - 2];
            const double delta = latest.score - previous.score;
            std::cout << "Change vs Previous: "
                      << (delta >= 0.0 ? "+" : "") << delta << "%\n";
        }

        if (history.size() >= 6) {
            const std::size_t split = history.size() - 3;
            double previousAvg = averageHistoryRange(history, split - 3, split);
            double recentAvg = averageHistoryRange(history, split, history.size());
            double trend = recentAvg - previousAvg;
            std::cout << "Trend (last 3 vs previous 3): "
                      << (trend >= 0.0 ? "+" : "") << trend << "%\n";
        }

        std::cout << "\nRecent Runs:\n";
        std::size_t startIndex = history.size() > 5 ? history.size() - 5 : 0;
        for (std::size_t i = startIndex; i < history.size(); ++i) {
            const QuizHistoryEntry& entry = history[i];
            std::cout << "- " << formatTimestamp(entry.epochSeconds)
                      << " | " << entry.score << "% (" << entry.correct << "/" << entry.attempted << ")"
                      << " | " << (entry.timedMode ? "Timed" : "Normal")
                      << " | difficulty: " << entry.difficulty
                      << " | " << entry.durationSeconds << "s"
                      << (entry.timedOutOverall ? " | total timer expired" : "")
                      << "\n";
        }
    }
    std::cout << "\n";
}

void takeQuiz(std::vector<Question>& questions) {
    if (questions.empty()) {
        std::cout << "No questions available. Add some first.\n";
        return;
    }

    clearScreen();

    QuizTiming timing = promptQuizTiming();

    std::vector<std::size_t> order(questions.size());
    for (std::size_t i = 0; i < questions.size(); ++i) {
        order[i] = i;
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(order.begin(), order.end(), gen);

    auto startTime = std::chrono::steady_clock::now();

    printSeparator();
    std::cout << "C++ QUIZ START\n";
    std::cout << "Total Questions: " << questions.size() << "\n";
    if (timing.timedMode) {
        std::cout << "Mode: Timed\n";
        std::cout << "Difficulty: " << timing.difficulty << "\n";
        std::cout << "Total Time Limit: " << timing.totalSeconds << " seconds\n";
        std::cout << "Per-Question Limit: " << timing.perQuestionSeconds << " seconds\n";
        std::cout << "Type 'pause' to pause/resume the timer.\n";
    }
    printSeparator();

    int correct = 0;
    int attempted = 0;
    std::vector<int> incorrectIndices;
    int hintsUsed = 0;
    bool timedOutOverall = false;

    for (std::size_t i = 0; i < order.size(); ++i) {
        if (timing.timedMode) {
            const auto elapsedOverall = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - startTime).count();
            if (elapsedOverall >= timing.totalSeconds) {
                timedOutOverall = true;
                break;
            }
        }

        const Question& question = questions[order[i]];
        auto questionStart = std::chrono::steady_clock::now();

        std::cout << "\n[Question " << (i + 1) << "/" << questions.size() << "]\n";
        std::cout << question.prompt << "\n";
        if (question.type != QuestionType::FreeText) {
            for (std::size_t optionIndex = 0; optionIndex < question.options.size(); ++optionIndex) {
                std::cout << "  " << optionLabel(optionIndex) << ") "
                          << question.options[optionIndex] << "\n";
            }
        }

        if (timing.timedMode) {
            const auto elapsedOverall = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - startTime).count();
            int totalLeft = timing.totalSeconds - static_cast<int>(elapsedOverall);
            if (totalLeft < 0) {
                totalLeft = 0;
            }
            std::cout << "Time left (quiz): " << totalLeft << "s\n";
        }

        std::string answerPrompt;
        if (question.type == QuestionType::MultipleChoice) {
            answerPrompt = "Your answer (one letter/number, 'h' for hint, 'quit' to exit): ";
        } else if (question.type == QuestionType::MultipleAnswer) {
            answerPrompt = "Your answer (comma-separated choices, 'h' for hint, 'quit' to exit): ";
        } else {
            answerPrompt = "Your answer ('h' for hint, 'quit' to exit): ";
        }

        if (timing.timedMode) {
            answerPrompt = answerPrompt.substr(0, answerPrompt.size() - 2) + ", 'pause' to pause): ";
        }

        std::string userAnswer;
        bool questionResolved = false;

        while (!questionResolved) {
            if (!timing.timedMode) {
                userAnswer = promptLine(answerPrompt);
            } else {
                const auto elapsedOverall = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - startTime).count();
                const auto elapsedQuestion = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - questionStart).count();

                int totalLeft = timing.totalSeconds - static_cast<int>(elapsedOverall);
                int questionLeft = timing.perQuestionSeconds - static_cast<int>(elapsedQuestion);
                int limit = std::min(totalLeft, questionLeft);

                if (limit <= 0) {
                    std::cout << "Time is up for this question.\n";
                    ++attempted;
                    incorrectIndices.push_back(static_cast<int>(order[i]));
                    questionResolved = true;
                    break;
                }

                TimedInputResult timedInput = promptLineTimed(answerPrompt, limit);
                if (timedInput.timedOut) {
                    std::cout << "Time is up for this question.\n";
                    ++attempted;
                    incorrectIndices.push_back(static_cast<int>(order[i]));
                    questionResolved = true;
                    break;
                }
                userAnswer = timedInput.input;
            }

            if (toLower(userAnswer) == "quit") {
                std::cout << "\nQuiz exited early.\n";
                return;
            }

            if (timing.timedMode && (toLower(userAnswer) == "pause" || toLower(userAnswer) == "p")) {
                std::cout << "\nQuiz paused. Press Enter to resume...";
                auto pauseStart = std::chrono::steady_clock::now();
                std::string ignored;
                std::getline(std::cin, ignored);
                auto pauseEnd = std::chrono::steady_clock::now();
                auto pauseDuration = pauseEnd - pauseStart;
                startTime += pauseDuration;
                questionStart += pauseDuration;
                std::cout << "Timers resumed.\n";
                continue;
            }

            if (toLower(userAnswer) == "h") {
                if (!question.hint.empty()) {
                    std::cout << "Hint: " << question.hint << "\n";
                    ++hintsUsed;
                } else {
                    std::cout << "No hint available for this question.\n";
                }
                continue;
            }

            ++attempted;
            if (isCorrectAnswer(question, userAnswer)) {
                std::cout << "Correct!\n";
                ++correct;
            } else {
                std::cout << "Not quite. Expected: " << formatExpectedAnswer(question) << "\n";
                incorrectIndices.push_back(static_cast<int>(order[i]));
            }
            questionResolved = true;
        }
    }

    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime);
    double timeTaken = static_cast<double>(duration.count());

    const double score = (attempted == 0)
        ? 0.0
        : (100.0 * static_cast<double>(correct) / static_cast<double>(attempted));

    QuizStats stats = loadStats(kStatsFile);
    stats.totalQuizzes++;
    stats.totalCorrect += correct;
    stats.totalAttempted += attempted;
    stats.averageScore = (100.0 * stats.totalCorrect) / stats.totalAttempted;
    if (score > stats.bestScore) {
        stats.bestScore = score;
    }
    saveStats(kStatsFile, stats);

    QuizHistoryEntry historyEntry;
    historyEntry.epochSeconds = static_cast<long long>(std::time(nullptr));
    historyEntry.score = score;
    historyEntry.correct = correct;
    historyEntry.attempted = attempted;
    historyEntry.totalQuestions = static_cast<int>(questions.size());
    historyEntry.durationSeconds = static_cast<int>(timeTaken);
    historyEntry.hintsUsed = hintsUsed;
    historyEntry.timedMode = timing.timedMode;
    historyEntry.timedOutOverall = timedOutOverall;
    historyEntry.difficulty = timing.timedMode ? timing.difficulty : "normal";
    appendHistory(kHistoryFile, historyEntry);

    printSeparator();
    std::cout << "QUIZ COMPLETE\n";
    printSeparator();
    std::cout << "Score: " << correct << " / " << attempted << " (" << score << "%)\n";
    std::cout << "Questions Seen: " << attempted << " / " << questions.size() << "\n";
    std::cout << "Time: " << timeTaken << " seconds\n";
    std::cout << "Hints Used: " << hintsUsed << "\n";
    if (timedOutOverall) {
        std::cout << "Quiz timer expired before all questions were shown.\n";
    }

    if (!incorrectIndices.empty()) {
        std::cout << "\nYou got " << incorrectIndices.size() << " question(s) wrong.\n";
        std::string reviewChoice = promptLine("Review incorrect answers? (y/n): ");
        if (toLower(trim(reviewChoice)) == "y") {
            std::cout << "\n--- Review ---\n";
            for (int idx : incorrectIndices) {
                const Question& wrongQuestion = questions[static_cast<std::size_t>(idx)];
                std::cout << "\nQ: " << wrongQuestion.prompt << "\n";
                std::cout << "A: " << formatExpectedAnswer(wrongQuestion) << "\n";
                if (!wrongQuestion.hint.empty()) {
                    std::cout << "Hint: " << wrongQuestion.hint << "\n";
                }
            }
        }
    }

    std::cout << "\n";
    std::string retakeChoice = promptLine("Take quiz again? (y/n): ");
    if (toLower(trim(retakeChoice)) == "y") {
        takeQuiz(questions);
    }
}

} // namespace

int main() {
    std::vector<Question> questions = loadQuestions(kQuestionFile);

    if (questions.empty()) {
        questions = defaultQuestions();
        if (saveQuestions(kQuestionFile, questions)) {
            std::cout << "No question file found. Loaded default C++ questions.\n\n";
        } else {
            std::cout << "Loaded default C++ questions in memory. Failed to write file.\n\n";
        }
    }

    while (true) {
        printMenu();
        std::string option;
        std::getline(std::cin, option);
        option = trim(option);

        if (option == "1") {
            takeQuiz(questions);
        } else if (option == "2") {
            addQuestion(questions);
        } else if (option == "3") {
            listQuestions(questions);
        } else if (option == "4") {
            editQuestion(questions);
        } else if (option == "5") {
            deleteQuestion(questions);
        } else if (option == "6") {
            showStats();
        } else if (option == "7") {
            std::cout << "\n";
            printSeparator();
            std::cout << "Good luck studying C++!\n";
            printSeparator();
            break;
        } else {
            std::cout << "Invalid option. Try again.\n";
        }
    }

    return 0;
}