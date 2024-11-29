#include <arpa/inet.h> // для функций преобразования адресов и работы с сетевыми адресами (inet_pton)
#include <cstring> // для работы со строками (memset)
#include <iostream> // вывод и ввод
#include <string> 
#include <sys/socket.h> // для создания и управления сокетами
#include <unistd.h> // для функций POSIX (close)
#include <netinet/in.h> // для структур и констант протокола IP (struct sockaddr_in)
#include <fcntl.h> // для управления файловыми дескрипторами (установка неблокирующего режима)
#define DEFAULT_PORT 8080 // порт по умолчанию, если пользователь его не задал

const int buffer_size = 1024; // размер буфера для приема и отправки данных

// Подключение к серверу по TCP
void use_tcp_client(const std::string& ip_address, int port) {
    int sock = 0;
    struct sockaddr_in serv_addr; // храним IP и порт сервера
    char buffer[buffer_size + 1] = {0}; // Буфер для передачи данных; +1 для символа окончания строкиcal

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) { // Создаем сокет в домене ipv4, тип сокета-потоковый,
    // что соответствует TCP
        std::cout << "Ошибка создания сокета\n";
        return;
    }
    // Заполнение структуры с адресом сервера
    serv_addr.sin_family = AF_INET; // домен IPv4
    serv_addr.sin_port = htons(port); // порт сервера в сетевом порядке байт

    // Преобразование Ip из строки в числовой формат
    if (inet_pton(AF_INET, ip_address.c_str(), &serv_addr.sin_addr) <= 0) {
        std::cout << "Неверный IP-адрес или неподдерживаемый адрес\n";
        close(sock);
        return;
    }
    // Подключение к серверу
    if (connect(sock, reinterpret_cast<struct sockaddr*>(&serv_addr), sizeof(serv_addr)) < 0) {
        std::cout << "Ошибка подключения к серверу!\n";
        close(sock);
        return;
    }
    // Цикл работы клиента
    while (true) {
        std::string expression;
        std::cout << "Введите выражение или 'exit' для завершения: ";
        std::getline(std::cin, expression);

        if (expression == "exit") {
            std::cout << "Завершение работы клиента\n";
            break;
        }
        // Отправка выражения на сервер
        send(sock, expression.c_str(), expression.length(), 0);
        std::cout << "Выражение отправлено на сервер\n";
        
        //Ожидание и получение результат от сервера
        memset(buffer, 0, sizeof(buffer)); // очистка буфера
        int valread = read(sock, buffer, buffer_size); // чтение ответа от сервера
        if (valread <= 0) {
            std::cout << "Потеряна связь с сервером.\n";
            break;
        }
        buffer[valread] = '\0';  // Установка конца строки

        // Вывод результата
        std::cout << "Результат от сервера: " << buffer << "\n";
    }
    //Закрываем сокет после завершения работы
    close(sock);
}

// Функия для подключения к серверу по UDP
void use_udp_client(const std::string& ip_address, int port) {
    int sock = 0;
    struct sockaddr_in serv_addr; // структура для адреса сервера
    char buffer[buffer_size + 1] = {0}; // Буфер для передачи данных
    
    // Создание UDP сокета
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { // ipv4-AF_INET, SOCK_DGRAM-UDP
        std::cout << "Ошибка создания сокета\n";
        return;
    }
    // Настройка структуры с адресом сервера
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    //Преобразование Ip
    if (inet_pton(AF_INET, ip_address.c_str(), &serv_addr.sin_addr) <= 0) {
        std::cout << "Неверный IP-адрес или неподдерживаемый адрес\n";
        close(sock);
        return;
    }

    // Перевод сокета в неблокирующий режим для реализации таймаутов
    // Нужно для того, чтобы программа не зависала в ожидании данных, а вместо этого
    // возвращаться сразу, даже если данных нет.
    int flags = fcntl(sock, F_GETFL, 0); 
    fcntl(sock, F_SETFL, flags | O_NONBLOCK); // неблокирующий режим 
    
    // Основной цикл работы клиента
    while (true) {
        std::string expression;
        std::cout << "Введите выражение или 'exit' для завершения: ";
        std::getline(std::cin, expression);

        if (expression == "exit") {
            std::cout << "Завершение работы клиента\n";
            break;
        }

        struct sockaddr_in from_addr; // Хранение адреса сервера при приеме ответного сообщения
        socklen_t from_len = sizeof(from_addr);

        int attempts = 0; // Счетчик попыток
        bool ack_received = false; // Флаг, который определяет подтверждение
        
        // Цикл отправки выражения с контролем доставки
        while (attempts < 3 && !ack_received) {
            //Отправка выражения на сервер
            sendto(sock, expression.c_str(), expression.length(), 0, // отправка данных на сервер
                   reinterpret_cast<struct sockaddr*>(&serv_addr), sizeof(serv_addr));
            std::cout << "Выражение отправлено на сервер (попытка " << attempts + 1 << ")\n";
            // (strcut sockaddr*)&from_addr - указатель на структуру, где будет сохранен адрес отправителя
            
            // Ожидание подтверждения от сервера
            fd_set readfds; // Набор дескрипторов, для проверки на входящее сообщение 
            FD_ZERO(&readfds); // Инициализация readfds, очищая его
            FD_SET(sock, &readfds); // Добавление сокета в readfds, чтобы select мог отслеживать состояние

            struct timeval timeout; // структура, которая задает макимальное время ожидания событий на сокете
            timeout.tv_sec = 3; // Таймаут 3 секунды
            timeout.tv_usec = 0; // Микросекунды равны 0, так как нужны полные секунды

            int retval = select(sock + 1, &readfds, NULL, NULL, &timeout); // Мониторнг дескрипторов
            // на предмет готовности к операциям ввода-вывода
            // sock+1 - максимальный номер дескриптора
            // &readfds - набор дескрипторов для отслеживания на чтение
            // NULL - не отслеживаем дескрипторы на запись
            // NULL - не отслеживаем дескрипторы на исключения 
            // &timeout - таймаут ожидания событиый 
            
            if (retval > 0 && FD_ISSET(sock, &readfds)) { // есть дескрипторы
                // Чтение данных из сокета 
                memset(buffer, 0, sizeof(buffer));
                // Получаем данные из сокета sock
                int len = recvfrom(sock, buffer , buffer_size, 0,
                                   reinterpret_cast<struct sockaddr*>(&from_addr),&from_len);
                                  // buffer - куда запишем данные
                                  // (strcut sockaddr*)&from_addr - указатель на структуру, где будет
                                  // сохранен адрес отправителя
                                  // указатель на размер структуры адреса 
                if (len > 0) {
                    buffer[len] = '\0';  // Добавляем для корректной работы строки 
                    
                    // Обработка полученного сообщения
                    std::string response(buffer);
                    if (response == "ACK") {
                        ack_received = true;
                        std::cout << "Подтверждение от сервера получено\n";
                    } else {
                        std::cout << "Результат от сервера: " << response << "\n";
                        ack_received = true;
                        break;
                    }
                }
            } else {
                // Таймаут или ошибка
                attempts++;
                if (attempts == 3) {
                    std::cout << "Потеряна связь с сервером.\n";
                    close(sock);
                    return;
                }
            }
        }

        if (!ack_received) {
            // Если не получили подтверждение после 3 попыток, переходим к следующему выражению
            continue;
        }

        // Ожидание результата вычисления от сервера
        bool result_received = false;
        attempts = 0;

        while (!result_received && attempts < 3) {
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(sock, &readfds);

            struct timeval timeout;
            timeout.tv_sec = 3;
            timeout.tv_usec = 0;

            int retval = select(sock + 1, &readfds, NULL, NULL, &timeout);

            if (retval > 0 && FD_ISSET(sock, &readfds)) {
                memset(buffer, 0, sizeof(buffer));
                int len = recvfrom(sock, buffer, buffer_size, 0,
                                  reinterpret_cast<struct sockaddr*>(&from_addr), &from_len);

                if (len > 0) {
                    buffer[len] = '\0';  // Добавляем нуль-терминатор

                    std::string response(buffer);
                    std::cout << "Результат от сервера: " << response << "\n";

                    // Отправляем подтверждение серверу
                    const char* ack = "ACK";
                    sendto(sock, ack, strlen(ack), 0,
                           reinterpret_cast<struct sockaddr*>(&serv_addr), sizeof(serv_addr));
                    result_received = true;
                }
            } else {
                // Таймаут или ошибка, увеличиваем счетчик попыток
                attempts++;
                if (attempts == 3) {
                    std::cout << "Потеряна связь с сервером.\n";
                    close(sock);
                    return;
                }
            }
        }
    }
    // закрываем сокет после завершения работы
    close(sock);
}


int main(int argc,const char* const argv[]) { // число аргументов, массив аргументов
    std::string ip_address = "127.0.0.1"; // адрес сервера по умолчанию
    int port = DEFAULT_PORT; // порт
    bool use_udp = false; // флаг, который указывает использовать ли протокол use_udp
  // По умолчанию используется TCP

  //Обработка аргументов строки
    int arg_idx = 1;
    while (arg_idx < argc) {
        std::string arg = argv[arg_idx];
        if (arg == "-u") {
            use_udp = true;
            arg_idx++;
        } else if (arg == "-a") {
            // Проверяем, указан ли IP-адрес после опции -a
            if (arg_idx + 1 < argc) {
                ip_address = argv[++arg_idx];
                arg_idx++;
            } else {
                std::cout << "Ошибка: не указан IP-адрес после опции -a\n";
                return -1;
            }
          // Проверяем, указан ли порт после опции -p
        } else if (arg == "-p") {
            if (arg_idx + 1 < argc) {
                port = std::stoi(argv[++arg_idx]);
                arg_idx++;
            } else {
                std::cout << "Ошибка: не указан порт после опции -p\n";
                return -1;
            }
        } else {
            // Обработка некорректных аргументов
            std::cout << "Некорректные аргументы командной строки.\n";
            std::cout << "Использование: ./CalcClient [-u] [-a IP-адрес] [-p порт]\n";
            return -1;
        }
    }
    // Вывод информации о выбранном протоколе и адресе сервера
    std::cout << "Использование " << (use_udp ? "UDP" : "TCP") << " протокола.\n";
    std::cout << "Подключение к серверу по адресу " << ip_address << ":" << port << "\n";
    // Выбор функции в зависимости от протокола
    if (use_udp) {
        use_udp_client(ip_address, port);
    } else {
        use_tcp_client(ip_address, port);
    }

    return 0;
}



