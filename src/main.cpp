#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <vector>

struct Question {
    std::string prompt;
    std::string answer;
};

namespace {

const std::string kQuestionFile = "questions.txt";

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
        {"What does RAII stand for in C++?", "Resource Acquisition Is Initialization"},
        {"Which STL container stores unique keys in sorted order?", "set"},
        {"What keyword is used to derive one class from another?", "public"},
        {"Which smart pointer allows shared ownership of an object?", "shared_ptr"},
        {"What is the return type of main in standard C++?", "int"},
        {"Which operator is commonly overloaded for stream output?", "<<"},
        {"What does std::move enable in modern C++?", "Move semantics"},
        {"Which header defines std::vector?", "<vector>"},
        {"What C++ feature allows one function name to have multiple parameter signatures?", "Function overloading"},
        {"What keyword prevents a method from being overridden in derived classes?", "final"}
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

        std::size_t delimiter = findDelimiter(line);
        if (delimiter == std::string::npos) {
            continue;
        }

        std::string prompt = unescapePipes(trim(line.substr(0, delimiter)));
        std::string answer = unescapePipes(trim(line.substr(delimiter + 1)));

        if (!prompt.empty() && !answer.empty()) {
            questions.push_back({prompt, answer});
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
        output << escapePipes(question.prompt) << '|' << escapePipes(question.answer) << '\n';
    }

    return true;
}

void printMenu() {
    std::cout << "\n=== C++ Quizzer ===\n";
    std::cout << "1) Take quiz\n";
    std::cout << "2) Add a question\n";
    std::cout << "3) List questions\n";
    std::cout << "4) Exit\n";
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

    std::cout << "\nCurrent questions:\n";
    for (std::size_t i = 0; i < questions.size(); ++i) {
        std::cout << i + 1 << ") " << questions[i].prompt << "\n";
    }
}

void addQuestion(std::vector<Question>& questions) {
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

    questions.push_back({prompt, answer});
    if (saveQuestions(kQuestionFile, questions)) {
        std::cout << "Question saved.\n";
    } else {
        std::cout << "Question added in memory, but failed to save file.\n";
    }
}

void takeQuiz(const std::vector<Question>& questions) {
    if (questions.empty()) {
        std::cout << "No questions available. Add some first.\n";
        return;
    }

    std::vector<std::size_t> order(questions.size());
    for (std::size_t i = 0; i < questions.size(); ++i) {
        order[i] = i;
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(order.begin(), order.end(), gen);

    std::cout << "\nStarting quiz with " << questions.size() << " questions.\n";
    int correct = 0;

    for (std::size_t i = 0; i < order.size(); ++i) {
        const Question& q = questions[order[i]];
        std::cout << "\nQuestion " << (i + 1) << ": " << q.prompt << "\n";
        std::string userAnswer = promptLine("Your answer: ");

        std::string normalizedUser = toLower(trim(userAnswer));
        std::string normalizedAnswer = toLower(trim(q.answer));

        if (normalizedUser == normalizedAnswer) {
            std::cout << "Correct!\n";
            ++correct;
        } else {
            std::cout << "Not quite. Expected answer: " << q.answer << "\n";
        }
    }

    const double score = questions.empty() ? 0.0
                                           : (100.0 * static_cast<double>(correct) /
                                              static_cast<double>(questions.size()));

    std::cout << "\nQuiz complete.\n";
    std::cout << "Correct: " << correct << " / " << questions.size() << "\n";
    std::cout << "Score: " << score << "%\n";
}

} // namespace

int main() {
    std::vector<Question> questions = loadQuestions(kQuestionFile);

    if (questions.empty()) {
        questions = defaultQuestions();
        if (saveQuestions(kQuestionFile, questions)) {
            std::cout << "No question file found or it was empty. Loaded default C++ questions.\n";
        } else {
            std::cout << "Loaded default C++ questions in memory. Failed to write questions file.\n";
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
            std::cout << "Good luck studying C++!\n";
            break;
        } else {
            std::cout << "Invalid option. Try again.\n";
        }
    }

    return 0;
}
