#include "EvaluateExpression.cpp" // Включаем файл с логикой вычисления выражений
#include <arpa/inet.h> // Для преобразования и работы с сетевыми адресами
#include <csignal> // Для обработки сигналов, чтобы корректно завершить сервер
#include <cstring> // Для работы с строками (например, memset)
#include <fcntl.h> // Для управления файловыми дескрипторами (например, режим неблокировки)
#include <iostream> // Для вывода на консоль
#include <netinet/in.h> // Для работы с сетевыми адресами и сокетами (структура sockaddr_in)
#include <sstream> // Для преобразования чисел в строки
#include <string> // Для использования класса std::string
#include <sys/socket.h> // Для создания и управления сокетами
#include <unistd.h> // Для POSIX-функций, таких как close
#include <cctype> // Для работы с символами
#include <cmath> // Для проверки числовых значений, таких как inf и nan
#include <cerrno> // Для обработки ошибок
#include <ctime> // Для использования nanosleep

#define DEFAULT_PORT 8080 // Порт по умолчанию, на котором будет слушать сервер

const int buffer_size = 1024; // Размер буфера для приема и отправки данных

bool running = true; // Флаг для указания, что сервер работает

// Функция-обработчик сигнала для завершения сервера
void signalHandler(int /*signum*/) { // намеренно не использую параметр (для того чтобы не было warning'ов)
    running = false; // Устанавливает флаг, чтобы завершить основной цикл сервера
}

// Реализация сервера TCP
// Создает сокет (accept), прослушивает и принимает подключение,
//  отдельный процесс для клиента(fork), получение и обработка
//
void use_tcp_server(const std::string& ip_address, int port) {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    char buffer[buffer_size + 1] = {0}; // Буфер для приема данных

    // Создание сокета для TCP
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Ошибка создания сокета");
        exit(EXIT_FAILURE);
    }

    // Настройка параметров сокета для повторного использования адреса
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("Ошибка настройки параметров сокета");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Настройка адреса сервера
    address.sin_family = AF_INET;
    address.sin_port = htons(port);

    // Преобразование IP-адреса в двоичный формат
    if (inet_pton(AF_INET, ip_address.c_str(), &address.sin_addr) <= 0) {
        std::cout << "Неверный IP-адрес или неподдерживаемый адрес\n";
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Привязка сокета к указанному IP-адресу и порту
    if (bind(server_fd, reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) < 0){
        perror("Ошибка привязки сокета");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Перевод сокета в режим прослушивания
    if (listen(server_fd, 3) < 0) {
        perror("Ошибка при попытке прослушивания");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    std::cout << "Сервер ожидает подключения...\n";

    while (running) {
        int new_socket;
        struct sockaddr_in client_address;
        socklen_t client_addrlen = sizeof(client_address);

        // Ожидание и принятие нового подключения от клиента

        if ((new_socket = accept(server_fd, reinterpret_cast<struct sockaddr*>(&client_address), &client_addrlen)) < 0) {
            if (errno == EINTR) {
                break;
            }
            perror("Ошибка при принятии подключения");
            continue;
        }
        // Вывод информации о клиенте
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_address.sin_addr), client_ip, INET_ADDRSTRLEN);

        std::cout << "Клиент подключился: " << client_ip << ":" << ntohs(client_address.sin_port) << "\n";
        
        // Дочерний процесс для обслуживания нескольких клиентов
        pid_t pid = fork(); // Создание скопированного процесса
        if (pid == 0) { // Ребенок обрабатывает запрос клиента
            close(server_fd);

            while (true) {
                memset(buffer, 0, sizeof(buffer));
                int valread = read(new_socket, buffer, buffer_size);
                if (valread <= 0) {
                    std::cout << "Клиент отключился.\n";
                    break;
                }
                buffer[valread] = '\0';
                
                // Обработка данных и отправка ответов
                std::cout << "Получено от клиента: " << buffer << "\n";
                std::string expression(buffer); // Буфер в строку
                std::string errorMessage;
                double result = evaluateExpression(expression, errorMessage);

                std::string response;
                if (!errorMessage.empty()) {
                    response = errorMessage;
                } else {
                    if (std::isinf(result)) {
                        response = "Результат: inf";
                    } else if (std::isnan(result)) {
                        response = "Результат: nan";
                    } else {
                        std::ostringstream oss;
                        oss.precision(10);
                        oss << "Результат: " << result;
                        response = oss.str();
                    }
                }
                // Отправка
                send(new_socket, response.c_str(), response.length(), 0);
                std::cout << "Результат отправлен клиенту: " << response << "\n";
            }

            close(new_socket);
            exit(0); // Завершение дочернего процесса
        } else if (pid > 0) { // Родительный процесс
            close(new_socket); // Закрываем клиентский сокет
        } else {
            perror("Ошибка создания процесса");
            close(new_socket);
        }
    }

    close(server_fd);
}

// Реализация сервера UDP
// Создает сокет, принимает данные без подключения с помощью recvfrom, повторная отправка и подтверждение
// Несколько раз отправляет ответ, пока не получит ACK от клиента, 
// select используется для ожидания подтверждения в течениче 3 секунд
void use_udp_server(const std::string& ip_address, int port) {
    int server_fd;
    struct sockaddr_in address;
    char buffer[buffer_size + 1] = {0}; // Буфер для приема данных

    // Создание сокета для UDP
    if ((server_fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("Ошибка создания сокета");
        exit(EXIT_FAILURE);
    }

    // Настройка адреса сервера
    address.sin_family = AF_INET;
    address.sin_port = htons(port);

    // Преобразование IP-адреса
    if (inet_pton(AF_INET, ip_address.c_str(), &address.sin_addr) <= 0) {
        std::cout << "Неверный IP-адрес или неподдерживаемый адрес\n";
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Привязка сокета к IP-адресу и порту
    if (bind(server_fd, reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) < 0) {
        perror("Ошибка привязки сокета");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    std::cout << "Сервер ожидает данных по UDP...\n";

    while (running) {
        struct sockaddr_in client_addr;
        socklen_t client_addrlen = sizeof(client_addr);

        memset(buffer, 0, sizeof(buffer));

        int len = recvfrom(server_fd, buffer, buffer_size, 0, reinterpret_cast<struct sockaddr*>(&client_addr), &client_addrlen);
        if (len > 0) {
            // Логирование данных клиента
            buffer[len] = '\0';
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
            int client_port = ntohs(client_addr.sin_port);
            std::cout << "Получено от клиента " << client_ip << ":" << client_port << ": " << buffer << "\n";

            // Обработка выражения и отправка подтверждения
            std::string expression(buffer);
            std::string errorMessage;
            double result = evaluateExpression(expression, errorMessage);

            std::string response;
            if (!errorMessage.empty()) {
                response = errorMessage;
            } else {
                if (std::isinf(result)) {
                    response = "Результат: inf";
                } else if (std::isnan(result)) {
                    response = "Результат: nan";
                } else {
                    std::ostringstream oss;
                    oss.precision(10);
                    oss << "Результат: " << result;
                    response = oss.str();
                }
            }

            // Отправка результата клиенту и получение подтверждения
              int attempts = 0;
              bool ack_received = false;
              // Разобраться с повторным отправлением
              while (attempts < 3 && !ack_received) {
                  sendto(server_fd, response.c_str(), response.length(), 0, reinterpret_cast<struct sockaddr*>(&client_addr),
                         client_addrlen);
                  std::cout << "Результат отправлен клиенту (попытка " << attempts + 1 << ")\n";
                  // Ожидания подтверждения от клиента
                  fd_set readfds;
                  FD_ZERO(&readfds);
                  FD_SET(server_fd, &readfds);

                  struct timeval timeout;
                  timeout.tv_sec = 3;
                  timeout.tv_usec = 0;

                  int retval = select(server_fd + 1, &readfds, NULL, NULL, &timeout);

                  if (retval > 0 && FD_ISSET(server_fd, &readfds)) {
                      char ack_buffer[buffer_size] = {0};
                      struct sockaddr_in ack_addr;
                      socklen_t ack_len = sizeof(ack_addr);

                      int ack_size = recvfrom(server_fd, ack_buffer, buffer_size, 0,reinterpret_cast<struct sockaddr*>(&ack_addr), &ack_len);

                      if (ack_size > 0) {
                          std::string ack_msg(ack_buffer, ack_size);
                          if (ack_msg == "ACK") {
                            ack_received = true;
                            std::cout << "Подтверждение от клиента получено.\n";
                        }
                    }
                } else {
                    attempts++;
                    if (attempts == 3) {
                        std::cout << "Потеряна связь с клиентом.\n";
                    }
                }
            }
        } else {
            struct timespec req = {};
            req.tv_sec = 0;
            req.tv_nsec = 100000000L; // 100 миллисекунд
            
            nanosleep(&req, nullptr);
        }
    }

    close(server_fd);
}
// Основная функция запуска сервера
int main(int argc, const char* const argv[]) {
    bool use_udp = false; // По умолчанию сервер использует TCP
    std::string ip_address = "0.0.0.0"; // Слушать на всех доступных IP-адресах
    int port = DEFAULT_PORT; // Порт по умолчанию

    // Обработка аргументов командной строки
    int arg_idx = 1;
    while (arg_idx < argc) {
        std::string arg = argv[arg_idx];
        if (arg == "-u") {
            use_udp = true;
            arg_idx++;
        } else if (arg == "-a") {
            if (arg_idx + 1 < argc) {
                ip_address = argv[++arg_idx];
                arg_idx++;
            } else {
                std::cout << "Ошибка: не указан IP-адрес после опции -a\n";
                return -1;
            }
        } else if (arg == "-p") {
            if (arg_idx + 1 < argc) {
                port = std::stoi(argv[++arg_idx]);
                arg_idx++;
            } else {
                std::cout << "Ошибка: не указан порт после опции -p\n";
                return -1;
            }
        } else {
            std::cout << "Некорректные аргументы командной строки.\n";
            std::cout << "Использование: ./CalcServer [-u] [-a IP-адрес] [-p порт]\n";
            return -1;
        }
    }

    std::cout << "Сервер запущен на адресе " << ip_address << ":" << port
              << " с использованием " << (use_udp ? "UDP" : "TCP") << " протокола.\n";

    signal(SIGINT, signalHandler); // Установка обработчика для завершения работы сервера

    if (use_udp) {
        use_udp_server(ip_address, port);
    } else {
        use_tcp_server(ip_address, port);
    }

    std::cout << "Сервер остановлен.\n";
    return 0;
}

