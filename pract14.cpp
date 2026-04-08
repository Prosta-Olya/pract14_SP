// pract14.cpp : Этот файл содержит функцию "main". Здесь начинается и заканчивается выполнение программы.
//

#include <iostream>
#include <Windows.h>

#define MAX_CLIENTS 20
#define CLUB_CAPACITY 4
#define WAIT_TIMEOUT_MS 3000
#define OBSERVER_INTERVAL_MS 500
#define MIN_SERVICE_TIME 2000
#define MAX_SERVICE_TIME 5000
#define USE_SEMAPHORE 1

// Структура записи о посетителе
struct ClientRecord {
	DWORD threadId; // Идентификатор потока
	DWORD arriveTick; // Время прихода посетителя
	DWORD startTick; // Время начала обслуживания
	DWORD endTick; // Время завершения обслуживания
	BOOL served; // Был ли обслужен
	BOOL timeout; // Ушел ли по таймауту
};

// Состояние компьютерного клуба
struct ClubState {
	ClientRecord clients[MAX_CLIENTS]; // Информация о посетителях
	LONG currentVisitors; // Текущее число занятых мест
	LONG maxVisitors; // Максимум одновременно занятых мест
	LONG servedCount; // Количество обслуженных посетителей
	LONG timeoutCount; // Количество ушедших по таймауту
};

// --- Глобальные переменные ---
ClubState g_clubState = { 0 };
HANDLE g_hSemaphore = NULL;
CRITICAL_SECTION g_csState; // Для защиты данных состояния
BOOL g_bAllVisitorsFinished = FALSE;

// --- Прототипы функций ---
DWORD WINAPI VisitorThreadProc(LPVOID lpParam);
DWORD WINAPI ObserverThreadProc(LPVOID lpParam);
void PrintFinalStats();

// --- Основной поток ---
int main() {
    setlocale(LC_ALL, "Russian");
    // Инициализация генератора случайных чисел
    srand((unsigned int)time(NULL));

    // Инициализация критической секции
    InitializeCriticalSection(&g_csState);

    // Инициализация семафора
#if USE_SEMAPHORE
    g_hSemaphore = CreateSemaphore(NULL, CLUB_CAPACITY, CLUB_CAPACITY, NULL);
    if (g_hSemaphore == NULL) {
        std::cout << "Ошибка создания семафора: " << GetLastError() << std::endl;
        return 1;
    }
    std::cout << "=== ЗАПУСК В РЕЖИМЕ: С СЕМАФОРОМ ===" << std::endl;
#else
    std::cout << "=== ЗАПУСК В РЕЖИМЕ: БЕЗ СЕМАФОРА ===" << std::endl;
#endif

    // Сброс состояния
    g_clubState.currentVisitors = 0;
    g_clubState.maxVisitors = 0;
    g_clubState.servedCount = 0;
    g_clubState.timeoutCount = 0;
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        g_clubState.clients[i].served = FALSE;
        g_clubState.clients[i].timeout = FALSE;
    }

    // Массив дескрипторов потоков (вместо vector)
    HANDLE hVisitors[MAX_CLIENTS];
    HANDLE hObserver;

    // Создание потоков посетителей (T1 - T20)
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        int threadNum = i + 1;
        DWORD priority = THREAD_PRIORITY_NORMAL;

        // Установка приоритетов согласно заданию
        if (threadNum >= 1 && threadNum <= 8) {
            priority = THREAD_PRIORITY_NORMAL;
        }
        else if (threadNum >= 9 && threadNum <= 16) {
            priority = THREAD_PRIORITY_BELOW_NORMAL;
        }
        else if (threadNum >= 17 && threadNum <= 20) {
            priority = THREAD_PRIORITY_HIGHEST;
        }

        hVisitors[i] = CreateThread(NULL, 0, VisitorThreadProc, (LPVOID)(intptr_t)threadNum, 0, NULL);
        if (hVisitors[i]) {
            SetThreadPriority(hVisitors[i], priority);
        }
    }

    // Создание потока наблюдателя (T21)
    hObserver = CreateThread(NULL, 0, ObserverThreadProc, NULL, 0, NULL);
    if (hObserver) {
        SetThreadPriority(hObserver, THREAD_PRIORITY_LOWEST);
    }

    // Ожидание завершения всех посетителей
    WaitForMultipleObjects(MAX_CLIENTS, hVisitors, TRUE, INFINITE);

    // Сигнализируем наблюдателю, что все закончили
    g_bAllVisitorsFinished = TRUE;

    // Ожидание завершения наблюдателя
    WaitForSingleObject(hObserver, INFINITE);

    // Вывод итоговой статистики (Доп. задание)
    PrintFinalStats();

    // Очистка ресурсов
    if (g_hSemaphore) CloseHandle(g_hSemaphore);
    CloseHandle(hObserver);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (hVisitors[i]) CloseHandle(hVisitors[i]);
    }
    DeleteCriticalSection(&g_csState);

    std::cout << "\nНажмите любую клавишу для выхода..." << std::endl;
    std::cin.get();

    return 0;
}

// --- Поток посетителя ---
DWORD WINAPI VisitorThreadProc(LPVOID lpParam) {
    int clientId = (int)(intptr_t)lpParam;
    int index = clientId - 1;

    // Фиксация прихода
    EnterCriticalSection(&g_csState);
    g_clubState.clients[index].threadId = GetCurrentThreadId();
    g_clubState.clients[index].arriveTick = GetTickCount();
    g_clubState.clients[index].served = FALSE;
    g_clubState.clients[index].timeout = FALSE;
    LeaveCriticalSection(&g_csState);

    // Попытка получения места
    DWORD waitResult = WAIT_OBJECT_0;
#if USE_SEMAPHORE
    waitResult = WaitForSingleObject(g_hSemaphore, WAIT_TIMEOUT_MS);
#else
    // Эмуляция отсутствия семафора (просто ждем немного)
    Sleep(rand() % 1000);
#endif

    if (waitResult == WAIT_OBJECT_0) {
        // Место получено
        EnterCriticalSection(&g_csState);
        g_clubState.clients[index].startTick = GetTickCount();
        g_clubState.clients[index].served = TRUE;
        g_clubState.currentVisitors++;
        if (g_clubState.currentVisitors > g_clubState.maxVisitors) {
            g_clubState.maxVisitors = g_clubState.currentVisitors;
        }
        LeaveCriticalSection(&g_csState);

        // Работа за компьютером (2000-5000 мс)
        int workTime = MIN_SERVICE_TIME + rand() % (MAX_SERVICE_TIME - MIN_SERVICE_TIME + 1);
        Sleep(workTime);

        // Освобождение места
        EnterCriticalSection(&g_csState);
        g_clubState.clients[index].endTick = GetTickCount();
        g_clubState.currentVisitors--;
        g_clubState.servedCount++;
        LeaveCriticalSection(&g_csState);

#if USE_SEMAPHORE
        ReleaseSemaphore(g_hSemaphore, 1, NULL);
#endif
    }
    else if (waitResult == WAIT_TIMEOUT) {
        // Таймаут ожидания
        EnterCriticalSection(&g_csState);
        g_clubState.clients[index].timeout = TRUE;
        g_clubState.timeoutCount++;
        LeaveCriticalSection(&g_csState);
    }

    return 0;
}

// --- Поток наблюдателя ---
DWORD WINAPI ObserverThreadProc(LPVOID lpParam) {
    std::cout << "\n--- Наблюдатель запущен ---" << std::endl;
    std::cout << "Время     | Занято | Обслужено | Таймауты" << std::endl;

    while (!g_bAllVisitorsFinished) {
        Sleep(OBSERVER_INTERVAL_MS);

        EnterCriticalSection(&g_csState);
        LONG curr = g_clubState.currentVisitors;
        LONG served = g_clubState.servedCount;
        LONG timeouts = g_clubState.timeoutCount;
        LeaveCriticalSection(&g_csState);

        // Вывод через std::cout с ручным форматированием
        std::cout << GetTickCount() << "   |   "
            << curr << "      |   "
            << served << "         |   "
            << timeouts << std::endl;
    }

    std::cout << "--- Наблюдатель завершил работу ---" << std::endl;
    return 0;
}

// --- Вывод итоговой статистики (Доп. задание) ---
void PrintFinalStats() {
    std::cout << "\n=== ИТОГОВАЯ СТАТИСТИКА ===" << std::endl;

    LONG totalWaitTime = 0;
    LONG totalServiceTime = 0;
    LONG servedCount = 0;
    int timedOutThreads[MAX_CLIENTS];
    int timeoutIndex = 0;

    EnterCriticalSection(&g_csState);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (g_clubState.clients[i].served) {
            servedCount++;
            totalWaitTime += (g_clubState.clients[i].startTick - g_clubState.clients[i].arriveTick);
            totalServiceTime += (g_clubState.clients[i].endTick - g_clubState.clients[i].startTick);
        }
        if (g_clubState.clients[i].timeout) {
            timedOutThreads[timeoutIndex++] = i + 1;
        }
    }
    LONG maxVisited = g_clubState.maxVisitors;
    LeaveCriticalSection(&g_csState);

    if (servedCount > 0) {
        std::cout << "1. Среднее время ожидания: " << (totalWaitTime / servedCount) << " мс." << std::endl;
        std::cout << "2. Среднее время обслуживания: " << (totalServiceTime / servedCount) << " мс." << std::endl;
    }
    else {
        std::cout << "1. Среднее время ожидания: 0 (не было обслуженных)" << std::endl;
        std::cout << "2. Среднее время обслуживания: 0 (не было обслуженных)" << std::endl;
    }

    std::cout << "3. Максимальное число одновременно занятых мест: " << maxVisited << std::endl;

    std::cout << "4. Потоки, не дождавшиеся места (Таймаут): ";
    if (timeoutIndex == 0) {
        std::cout << "Нет";
    }
    else {
        for (int k = 0; k < timeoutIndex; ++k) {
            std::cout << "T" << timedOutThreads[k] << " ";
        }
    }
    std::cout << std::endl;
}
