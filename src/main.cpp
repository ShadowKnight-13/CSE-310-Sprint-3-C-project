#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <vector>
#include <chrono>
#include <cstdlib>

struct Question {
    std::string prompt;
    std::string answer;
    std::string hint;
};

struct QuizStats {
    double bestScore = 0.0;
    int totalQuizzes = 0;
    double averageScore = 0.0;
    int totalCorrect = 0;
    int totalAttempted = 0;
};

namespace {

const std::string kQuestionFile = "questions.txt";
const std::string kStatsFile = "quiz_stats.txt";

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

std::size_t findDelimiter(const std::string& line) {
    bool escaped = false;

    for (std::size_t i = 0; i < line.size(); ++i) {
        if (!escaped && line[i] == '|') {
            return i;
        }

        if (!escaped && line[i] == '\\') {
            escaped = true;
        } else {
            escaped = false;
        }
    }

    return std::string::npos;
}

std::vector<Question> defaultQuestions() {
    return {
        {"What does RAII stand for in C++?", "Resource Acquisition Is Initialization", "Think about resources and initialization..."},
        {"Which STL container stores unique keys in sorted order?", "set", "It's similar to map but only stores keys..."},
        {"What keyword is used to derive one class from another?", "public", "This controls access visibility..."},
        {"Which smart pointer allows shared ownership of an object?", "shared_ptr", "Multiple pointers can share one object..."},
        {"What is the return type of main in standard C++?", "int", "It returns a status code to the OS..."},
        {"Which operator is commonly overloaded for stream output?", "<<", "This is the insertion operator..."},
        {"What does std::move enable in modern C++?", "Move semantics", "It transfers ownership efficiently..."},
        {"Which header defines std::vector?", "<vector>", "The angle brackets contain the header name..."},
        {"What C++ feature allows one function name to have multiple parameter signatures?", "Function overloading", "Multiple functions with same name..."},
        {"What keyword prevents a method from being overridden in derived classes?", "final", "Stops inheritance in C++11+..."}
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

        std::size_t delimiter1 = findDelimiter(line);
        if (delimiter1 == std::string::npos) {
            continue;
        }

        std::string prompt = unescapePipes(trim(line.substr(0, delimiter1)));
        std::string rest = line.substr(delimiter1 + 1);

        std::size_t delimiter2 = findDelimiter(rest);
        std::string answer;
        std::string hint = "";

        if (delimiter2 == std::string::npos) {
            answer = unescapePipes(trim(rest));
        } else {
            answer = unescapePipes(trim(rest.substr(0, delimiter2)));
            hint = unescapePipes(trim(rest.substr(delimiter2 + 1)));
        }

        if (!prompt.empty() && !answer.empty()) {
            questions.push_back({prompt, answer, hint});
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
               << escapePipes(question.hint) << '\n';
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
        std::cout << (i + 1) << ") " << questions[i].prompt << "\n";
    }
    std::cout << "\n";
}

void addQuestion(std::vector<Question>& questions) {
    std::cout << "\n";
    printSeparator();
    std::cout << "ADD A QUESTION\n";
    printSeparator();
    
    std::string prompt = promptLine("Enter the question: ");
    if (prompt.empty()) {
        std::cout << "Question cannot be empty.\n";
        return;
    }

    std::string answer = promptLine("Enter the answer: ");
    if (answer.empty()) {
        std::cout << "Answer cannot be empty.\n";
        return;
    }

    std::string hint = promptLine("Enter a hint (optional, press Enter to skip): ");

    questions.push_back({prompt, answer, hint});
    if (saveQuestions(kQuestionFile, questions)) {
        std::cout << "✓ Question saved.\n";
    } else {
        std::cout << "✓ Question added in memory, but failed to save file.\n";
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

        std::cout << "\nCurrent question: " << questions[index].prompt << "\n";
        std::string newPrompt = promptLine("Enter new question (or press Enter to keep): ");
        if (!newPrompt.empty()) {
            questions[index].prompt = newPrompt;
        }

        std::cout << "Current answer: " << questions[index].answer << "\n";
        std::string newAnswer = promptLine("Enter new answer (or press Enter to keep): ");
        if (!newAnswer.empty()) {
            questions[index].answer = newAnswer;
        }

        std::cout << "Current hint: " << (questions[index].hint.empty() ? "(none)" : questions[index].hint) << "\n";
        std::string newHint = promptLine("Enter new hint (or press Enter to keep): ");
        if (!newHint.empty()) {
            questions[index].hint = newHint;
        }

        if (saveQuestions(kQuestionFile, questions)) {
            std::cout << "✓ Question updated.\n";
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

        std::cout << "Deleting: " << questions[index].prompt << "\n";
        std::string confirm = promptLine("Are you sure? (y/n): ");
        if (toLower(confirm) == "y") {
            questions.erase(questions.begin() + index);
            if (saveQuestions(kQuestionFile, questions)) {
                std::cout << "✓ Question deleted.\n";
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
    if (!output) return false;
    output << stats.bestScore << " " << stats.totalQuizzes << " " 
           << stats.averageScore << " " << stats.totalCorrect << " " 
           << stats.totalAttempted;
    return true;
}

void showStats() {
    QuizStats stats = loadStats(kStatsFile);
    
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
}

void takeQuiz(std::vector<Question>& questions) {
    if (questions.empty()) {
        std::cout << "No questions available. Add some first.\n";
        return;
    }

    clearScreen();

    std::vector<std::size_t> order(questions.size());
    for (std::size_t i = 0; i < questions.size(); ++i) {
        order[i] = i;
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(order.begin(), order.end(), gen);

    auto startTime = std::chrono::high_resolution_clock::now();

    printSeparator();
    std::cout << "C++ QUIZ START\n";
    std::cout << "Total Questions: " << questions.size() << "\n";
    printSeparator();

    int correct = 0;
    std::vector<int> incorrectIndices;
    int hintsUsed = 0;

    for (std::size_t i = 0; i < order.size(); ++i) {
        const Question& q = questions[order[i]];
        
        std::cout << "\n[Question " << (i + 1) << "/" << questions.size() << "]\n";
        std::cout << q.prompt << "\n";
        
        std::string userAnswer = promptLine("Your answer ('h' for hint, 'quit' to exit): ");

        if (toLower(userAnswer) == "quit") {
            std::cout << "\nQuiz exited early.\n";
            return;
        }

        if (toLower(userAnswer) == "h") {
            if (!q.hint.empty()) {
                std::cout << "💡 Hint: " << q.hint << "\n";
                ++hintsUsed;
                userAnswer = promptLine("Your answer: ");
            } else {
                std::cout << "No hint available for this question.\n";
                userAnswer = promptLine("Your answer: ");
            }
        }

        std::string normalizedUser = toLower(trim(userAnswer));
        std::string normalizedAnswer = toLower(trim(q.answer));

        if (normalizedUser == normalizedAnswer) {
            std::cout << "✓ Correct!\n";
            ++correct;
        } else {
            std::cout << "✗ Not quite. Expected: " << q.answer << "\n";
            incorrectIndices.push_back(order[i]);
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime);
    double timeTaken = duration.count();

    const double score = questions.empty() ? 0.0
                                           : (100.0 * static_cast<double>(correct) /
                                              static_cast<double>(questions.size()));

    // Update statistics
    QuizStats stats = loadStats(kStatsFile);
    stats.totalQuizzes++;
    stats.totalCorrect += correct;
    stats.totalAttempted += questions.size();
    stats.averageScore = (100.0 * stats.totalCorrect) / stats.totalAttempted;
    if (score > stats.bestScore) {
        stats.bestScore = score;
    }
    saveStats(kStatsFile, stats);

    printSeparator();
    std::cout << "QUIZ COMPLETE\n";
    printSeparator();
    std::cout << "Score: " << correct << " / " << questions.size() 
              << " (" << score << "%)\n";
    std::cout << "Time: " << timeTaken << " seconds\n";
    std::cout << "Hints Used: " << hintsUsed << "\n";

    // Show review option for incorrect answers
    if (!incorrectIndices.empty()) {
        std::cout << "\nYou got " << incorrectIndices.size() 
                  << " question(s) wrong.\n";
        std::string reviewChoice = promptLine("Review incorrect answers? (y/n): ");
        if (toLower(trim(reviewChoice)) == "y") {
            std::cout << "\n--- Review ---\n";
            for (int idx : incorrectIndices) {
                std::cout << "\nQ: " << questions[idx].prompt << "\n";
                std::cout << "A: " << questions[idx].answer << "\n";
                if (!questions[idx].hint.empty()) {
                    std::cout << "Hint: " << questions[idx].hint << "\n";
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
            std::cout << "✓ No question file found. Loaded default C++ questions.\n\n";
        } else {
            std::cout << "✓ Loaded default C++ questions in memory. Failed to write file.\n\n";
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
