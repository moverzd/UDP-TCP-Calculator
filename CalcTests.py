import socket # работа с сокетами
import subprocess # серверные процессы
import time # для работы с задержками

def start_server(server_executable, server_ip, server_port): # (путь,айпи,порт)
    try:
        print("Запуск сервера...")
        # запуск сервера с передачей параметров через cmd
        process = subprocess.Popen(
            [server_executable, "-a", server_ip, "-p", str(server_port)],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )
        # даем серверу время на инициализацию
        time.sleep(2)
        print("Сервер запущен.")
        return process
    #случай когда не получается поднять сервер
    except Exception as e:
        print(f"Ошибка при запуске сервера: {e}")
        return None

def stop_server(server_process):
    if server_process:
        server_process.terminate()
        server_process.wait()
        print("Сервер остановлен.")

def test_calculator(server_ip, server_port, test_cases): 
    passed_tests = 0  # Счетчик пройденных тестов
    total_tests = len(test_cases)  # Общее количество тестов
    try:
        # Подключение к серверу TCP
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as client_socket:
            client_socket.connect((server_ip, server_port))
            print(f"Подключено к серверу {server_ip}:{server_port}")
            
            for expression, expected in test_cases.items():
                # Отправка выражения на сервер
                client_socket.sendall(expression.encode())
                print(f"Отправлено: {expression}")
                
                # Получение ответа от сервера
                response = client_socket.recv(1024).decode().strip() # bytes -> str
                print(f"Получено: {response}")
                
                # Проверка результата
                if response == expected:
                    print(f"Тест пройден: {expression} = {response}")
                    passed_tests += 1
                else:
                    print(f"Ошибка: {expression} ожидалось '{expected}', но получили '{response}'")
            
            print(f"\nРезультаты тестирования:")
            print(f"Пройдено тестов: {passed_tests}/{total_tests}")
    except Exception as e:
        print(f"Ошибка подключения или тестирования: {e}")

if __name__ == "__main__":
    server_ip = "127.0.0.1"  # Локальный сервер
    server_port = 8080  # Порт сервера

    # Путь к исполняемому файлу сервера
    server_executable = "./server"

    # Тестовые примеры
    test_cases = {
        "3+5": "Результат: 8",
        "10-7": "Результат: 3",
        "-1+2": "Результат: 1",
        "-1-1": "Результат: -2",
        "-4*2": "Результат: -8",
        "4*-2": "Результат: -8",
        "-4*-4": "Результат: 16",
        "-20/4": "Результат: -5",
        "20/-4": "Результат: -5",
        "-20/-4": "Результат: 5",
        "0/10": "Результат: 0",
        "-10/0": "Ошибка: Деление на ноль!",
        "0/-10": "Результат: -0",
        "3+5-2": "Результат: 6",
        "10-5+2": "Результат: 7",
        "(3+5)*2": "Результат: 16",
        "3+(5*2)": "Результат: 13",
        "(10-2)/2": "Результат: 4",
        "10-(5+3)": "Результат: 2",
        "(8/2)*4": "Результат: 16",
        "3*(5-(2+1))": "Ошибка: Превышено максимальное количество операндов (3)!",
        "1e308+1e308": "Результат: inf",
        "-1e308+-1e308": "Результат: inf",
        "1e-308*1e-308": "Ошибка: Некорректное число!",
        "abc + edf": "Ошибка: Ожидалось число!"
    }

    # Запуск сервера
    server_process = start_server(server_executable, server_ip, server_port)
    if server_process:
        try:
            # Запуск тестирования
            test_calculator(server_ip, server_port, test_cases)
        finally:
            # Остановка сервера
            stop_server(server_process)
