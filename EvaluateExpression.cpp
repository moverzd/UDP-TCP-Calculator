#include <string>
#include <cctype>
#include <limits>

double evaluateExpression(const std::string& expression, std::string& errorMessage) {
    class Parser { // Класс для парсинга выражения
    public:
        explicit Parser(const std::string& expr) : expr(expr), pos(0), operandCount(0), operatorCount(0) {}
        
        // Разбор выражения, обработка пробелов, проверка на существования символов после последнего операнда
        double parse(std::string& error) {
            double result = parseExpression(error);
            skipWhitespace();
            if (pos < expr.length() && error.empty()) {
                error = "Ошибка: Некорректное выражение!";
            }
            return result;
        }
    
    private:
        std::string expr;
        size_t pos;
        int operandCount;   // Счётчик операндов
        int operatorCount;  // Счётчик операций
        const int maxOperands = 3;
        const int maxOperators = 3;
        
        // Пропуск пробелов
        void skipWhitespace() {
            while (pos < expr.length() && isspace(expr[pos])) {
                pos++;
            }
        }
        // возвращает сивола, не изменяя pos
        char peek() {
            if (pos < expr.length()) {
                return expr[pos];
            }
            return '\0';
        }
        // возвращает текущий символ и сдвигает pos вправо
        char get() {
            if (pos < expr.length()) {
                return expr[pos++];
            }
            return '\0';
        }
        // Обработка выражений с + и -
        double parseExpression(std::string& error) {
            double result = parseTerm(error);
            while (true) {
                skipWhitespace();
                char op = peek();
                if (op == '+' || op == '-') {
                    if (++operatorCount > maxOperators) {
                        error = "Ошибка: Превышено максимальное количество операций (3)!";
                        return result;
                    }
                    get(); // Consume operator
                    double value = parseTerm(error);
                    if (!error.empty()) return result;
                    if (op == '+') {
                        result += value;
                    } else {
                        result -= value;
                    }
                } else {
                    break;
                }
            }
            return result;
        }
        // Обработка * и / 
        double parseTerm(std::string& error) {
            double result = parseFactor(error);
            while (true) {
                skipWhitespace();
                char op = peek();
                if (op == '*' || op == '/') {
                    if (++operatorCount > maxOperators) {
                        error = "Ошибка: Превышено максимальное количество операций (3)!";
                        return result;
                    }
                    get(); // Consume operator
                    double value = parseFactor(error);
                    if (!error.empty()) return result;
                    if (op == '*') {
                        result *= value;
                    } else {
                        if (value == 0) {
                            error = "Ошибка: Деление на ноль!";
                            return std::numeric_limits<double>::quiet_NaN();
                        }
                        result /= value;
                    }
                } else {
                    break;
                }
            }
            return result;
        }
        // Обработка унарных + и -, а также выражения в скобках
        double parseFactor(std::string& error) {
            skipWhitespace();
            char op = peek();
            if (op == '+' || op == '-') {
                get(); // Consume operator
                double value = parseFactor(error);
                if (!error.empty()) return value;
                return (op == '-') ? -value : value;
            } else if (op == '(') {
                get(); // Consume '('
                double value = parseExpression(error);
                skipWhitespace();
                if (peek() == ')') {
                    get(); // Consume ')'
                } else {
                    error = "Ошибка: Ожидалась закрывающая скобка!";
                }
                return value;
            } else {
                return parseNumber(error);
            }
        }
        // Обработка чисел
        double parseNumber(std::string& error) {
            skipWhitespace();
            size_t start = pos;
    
            if (peek() == '+' || peek() == '-') {
                get();
            }
    
            bool hasDecimal = false;
            bool hasExponent = false;
    
            while (pos < expr.length()) {
                char c = expr[pos];
                if (isdigit(c)) {
                    pos++;
                } else if (c == '.') {
                    if (hasDecimal) {
                        error = "Ошибка: Некорректное число!";
                        return 0;
                    }
                    hasDecimal = true;
                    pos++;
                } else if (c == 'e' || c == 'E') {
                    if (hasExponent) {
                        error = "Ошибка: Некорректное число!";
                        return 0;
                    }
                    hasExponent = true;
                    pos++;
                    if (pos < expr.length() && (expr[pos] == '+' || expr[pos] == '-')) {
                        pos++;
                    }
                } else {
                    break;
                }
            }
    
            if (start == pos) {
                error = "Ошибка: Ожидалось число!";
                return 0;
            }
    
            if (++operandCount > maxOperands) {
                error = "Ошибка: Превышено максимальное количество операндов (3)!";
                return 0;
            }
    
            std::string numberStr = expr.substr(start, pos - start);
            try {
                double value = std::stod(numberStr);
                return value;
            } catch (...) {
                error = "Ошибка: Некорректное число!";
                return 0;
            }
        }
    };
    
    Parser parser(expression);
    return parser.parse(errorMessage);
}
