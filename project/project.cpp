#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <stack>
#include <conio.h>  // для _getch() и _kbhit() на Windows
#include <windows.h> // для SetConsoleCursorPosition, GetStdHandle

using namespace std;

// ==================== КЛАСС MEMENTO ====================
class Memento
{
private:
    vector <vector <char>> snapshot;

public:
    Memento(const vector <vector <char>>& grid) : snapshot(grid) {}

    vector <vector <char>> restore() const {
        return snapshot;
    }
};

// ==================== КЛАСС CANVAS ====================
class Canvas {
private:
    int width;
    int height;
    vector <vector <char>> grid;
    vector <Memento> history;
    int cursorX;
    int cursorY;
    char currentChar;

    void saveToHistory() {
        if (history.size() >= 20) {
            history.erase(history.begin());
        }
        history.push_back(Memento(grid));
    }

public:
    Canvas(int w, int h) : width(w), height(h), cursorX(0), cursorY(0), currentChar('@') {
        // Ограничения размеров
        if (width < 40) width = 40;
        if (width > 200) width = 200;
        if (height < 20) height = 20;
        if (height > 100) height = 100;

        // Инициализация холста
        grid = vector <vector <char>>(height, vector <char>(width, '.'));
    }

    // Getters
    int getWidth() const { return width; }
    int getHeight() const { return height; }
    int getCursorX() const { return cursorX; }
    int getCursorY() const { return cursorY; }
    char getCurrentChar() const { return currentChar; }
    char getPixel(int x, int y) const { return grid[y][x]; }

    void setCurrentChar(char ch) { currentChar = ch; }

    void setPixel(int x, int y, char ch) {
        if (x >= 0 && x < width && y >= 0 && y < height) {
            grid[y][x] = ch;
        }
    }

    void moveCursor(int dx, int dy) {
        cursorX += dx;
        cursorY += dy;
        if (cursorX < 0) cursorX = 0;
        if (cursorX >= width) cursorX = width - 1;
        if (cursorY < 0) cursorY = 0;
        if (cursorY >= height) cursorY = height - 1;
    }

    void setCursorPosition(int x, int y) {
        if (x >= 0 && x < width) cursorX = x;
        if (y >= 0 && y < height) cursorY = y;
    }

    void drawLine(int x1, int y1, int x2, int y2, char ch) {
        saveToHistory();

        int dx = abs(x2 - x1);
        int dy = abs(y2 - y1);
        int sx = (x1 < x2) ? 1 : -1;
        int sy = (y1 < y2) ? 1 : -1;
        int err = dx - dy;

        int x = x1, y = y1;
        while (true) {
            setPixel(x, y, ch);
            if (x == x2 && y == y2) break;
            int e2 = 2 * err;
            if (e2 > -dy) { err -= dy; x += sx; }
            if (e2 < dx) { err += dx; y += sy; }
        }
    }

    void drawRect(int x1, int y1, int x2, int y2, bool fill, char ch) {
        saveToHistory();

        if (x1 > x2) swap(x1, x2);
        if (y1 > y2) swap(y1, y2);

        if (fill) {
            for (int y = y1; y <= y2; y++) {
                for (int x = x1; x <= x2; x++) {
                    setPixel(x, y, ch);
                }
            }
        }
        else {
            // Верхняя и нижняя границы
            for (int x = x1; x <= x2; x++) {
                setPixel(x, y1, ch);
                setPixel(x, y2, ch);
            }
            // Левая и правая границы
            for (int y = y1; y <= y2; y++) {
                setPixel(x1, y, ch);
                setPixel(x2, y, ch);
            }
        }
    }

    void floodFill(int x, int y, char newChar) {
        if (x < 0 || x >= width || y < 0 || y >= height) return;

        char targetChar = grid[y][x];
        if (targetChar == newChar) return;

        saveToHistory();

        stack<pair<int, int>> st;
        st.push({ x, y });

        while (!st.empty()) {
            auto [cx, cy] = st.top();
            st.pop();

            if (cx < 0 || cx >= width || cy < 0 || cy >= height) continue;
            if (grid[cy][cx] != targetChar) continue;

            grid[cy][cx] = newChar;

            st.push({ cx + 1, cy });
            st.push({ cx - 1, cy });
            st.push({ cx, cy + 1 });
            st.push({ cx, cy - 1 });
        }
    }

    void clear() {
        saveToHistory();
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                grid[y][x] = '.';
            }
        }
    }

    void undo() {
        if (history.empty()) return;
        grid = history.back().restore();
        history.pop_back();
    }

    bool saveToFile(const string& filename) {
        ofstream file(filename);
        if (!file.is_open()) return false;

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                file << grid[y][x];
            }
            file << endl;
        }
        return true;
    }

    bool loadFromFile(const string& filename) {
        ifstream file(filename);
        if (!file.is_open()) return false;

        vector<vector<char>> newGrid(height, vector<char>(width, '.'));
        string line;
        int y = 0;

        while (getline(file, line) && y < height) {
            for (int x = 0; x < width && x < (int)line.length(); x++) {
                newGrid[y][x] = line[x];
            }
            y++;
        }

        grid = newGrid;
        return true;
    }

    void render(bool lineModeActive = false, bool rectModeActive = false, int step = 0) {
        // Сохраняем позицию курсора
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        COORD cursorPos;

        // Очищаем экран
        system("cls");

        // Выводим верхнюю границу
        cout << "=";
        for (int x = 0; x < width + 2; x++) cout << "=";
        cout << "=" << endl;

        // Выводим холст
        for (int y = 0; y < height; y++) {
            cout << "| ";
            for (int x = 0; x < width; x++) {
                if (x == cursorX && y == cursorY) {
                    // Инверсия для курсора
                    SetConsoleTextAttribute(hConsole, 112); // Белый на синем фоне
                    cout << grid[y][x];
                    SetConsoleTextAttribute(hConsole, 7); // Сброс
                }
                else {
                    cout << grid[y][x];
                }
            }
            cout << " |" << endl;
        }

        // Выводим нижнюю границу
        cout << "=";
        for (int x = 0; x < width + 2; x++) cout << "=";
        cout << "=" << endl;

        // Выводим панель управления
        cout << " ===============================================================================" << endl;
        cout << "| Текущий символ: [" << currentChar << "]                                       |" << endl;
        if (lineModeActive) {
            HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
            SetConsoleTextAttribute(hConsole, 14);
            cout << "| РЕЖИМ: РИСОВАНИЕ ЛИНИИ (Enter - точка, Escape - отмена)                  |" << endl;
            SetConsoleTextAttribute(hConsole, 7);
        }
        else if (rectModeActive) {
            HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
            SetConsoleTextAttribute(hConsole, 14);
            cout << "| РЕЖИМ: РИСОВАНИЕ ПРЯМОУГОЛЬНИКА (Enter - угол, Escape - отмена)         |" << endl;
            SetConsoleTextAttribute(hConsole, 7);
        }
        else {
            cout << "|                                                                               |" << endl;
        }
        cout << "| Управление:                                                                   |" << endl;
        cout << "|   [Стрелки] - движение курсора   [L] - линия        [R] - прямоугольник          |" << endl;
        cout << "|   [F] - заливка               [C] - очистить     [U] - отмена (Undo)          |" << endl;
        cout << "|   [S] - сохранить             [O] - загрузить    [H] - помощь                 |" << endl;
        cout << "|   [1-9, символы] - выбрать символ                                             |" << endl;
        cout << "|   [Q] - выход                                                                 |" << endl;
        cout << " ===============================================================================" << endl;
    }
};

// ==================== КЛАСС INPUT HANDLER ====================
class InputHandler {
private:
    Canvas* canvas;
    bool lineMode;
    bool rectMode;
    bool waitingForSecondPoint;
    int lineX1, lineY1;
    int rectX1, rectY1;

    void showHelp() {
        system("cls");
        cout << "==================== ПОМОЩЬ ====================" << endl;
        cout << "Управление курсором:" << endl;
        cout << "  Стрелки - перемещение курсора" << endl;
        cout << endl;
        cout << "Инструменты:" << endl;
        cout << "  L - режим рисования линии" << endl;
        cout << "  R - режим рисования прямоугольника" << endl;
        cout << "  F - заливка области" << endl;
        cout << "  C - очистить весь холст" << endl;
        cout << "  U - отменить последнее действие" << endl;
        cout << endl;
        cout << "Работа с файлами:" << endl;
        cout << "  S - сохранить в файл (.ascii или .txt)" << endl;
        cout << "  O - загрузить из файла (.ascii или .txt)" << endl;
        cout << endl;
        cout << "Символы:" << endl;
        cout << "  Любой печатный символ - установить текущий символ" << endl;
        cout << "  1-9 - быстрый выбор: @ # % * + - = | /" << endl;
        cout << endl;
        cout << "Прочее:" << endl;
        cout << "  H - показать эту справку" << endl;
        cout << "  Q - выход" << endl;
        cout << endl;
        cout << "================================================" << endl;
        cout << "Нажмите любую клавишу для продолжения...";
        _getch();
    }

    void showModeStatus() {
        if (lineMode) {
            HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
            SetConsoleTextAttribute(hConsole, 14); // Желтый цвет
            cout << "\n[РЕЖИМ: РИСОВАНИЕ ЛИНИИ] - Нажмите Enter для выбора первой точки, затем Enter для второй точки" << endl;
            SetConsoleTextAttribute(hConsole, 7);
        }
        else if (rectMode) {
            HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
            SetConsoleTextAttribute(hConsole, 14);
            cout << "\n[РЕЖИМ: РИСОВАНИЕ ПРЯМОУГОЛЬНИКА] - Нажмите Enter для выбора первого угла, затем Enter для второго угла" << endl;
            SetConsoleTextAttribute(hConsole, 7);
        }
    }

public:
    InputHandler(Canvas* c) : canvas(c), lineMode(false), rectMode(false),
        waitingForSecondPoint(false), lineX1(0), lineY1(0),
        rectX1(0), rectY1(0) {
    }

    void handleKeyPress(char key) {
        // Обработка числовых клавиш для быстрого выбора символа
        switch (key) {
        case '1': canvas->setCurrentChar('@'); break;
        case '2': canvas->setCurrentChar('#'); break;
        case '3': canvas->setCurrentChar('%'); break;
        case '4': canvas->setCurrentChar('*'); break;
        case '5': canvas->setCurrentChar('+'); break;
        case '6': canvas->setCurrentChar('-'); break;
        case '7': canvas->setCurrentChar('='); break;
        case '8': canvas->setCurrentChar('|'); break;
        case '9': canvas->setCurrentChar('/'); break;
        }

        // Обработка режимов рисования
        if (lineMode) {
            if (key == 13) { // Enter
                if (!waitingForSecondPoint) {
                    // Первая точка
                    lineX1 = canvas->getCursorX();
                    lineY1 = canvas->getCursorY();
                    waitingForSecondPoint = true;

                    // Визуальная обратная связь
                    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
                    SetConsoleTextAttribute(hConsole, 10);
                    cout << "\n[Первая точка выбрана] Переместите курсор ко второй точке и нажмите Enter" << endl;
                    SetConsoleTextAttribute(hConsole, 7);
                    _getch(); // Пауза для чтения сообщения
                }
                else {
                    // Вторая точка - рисуем линию
                    canvas->drawLine(lineX1, lineY1, canvas->getCursorX(), canvas->getCursorY(), canvas->getCurrentChar());
                    lineMode = false;
                    waitingForSecondPoint = false;

                    // Визуальная обратная связь
                    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
                    SetConsoleTextAttribute(hConsole, 10);
                    cout << "\n[Линия нарисована! Режим рисования завершен]" << endl;
                    SetConsoleTextAttribute(hConsole, 7);
                    _getch();
                }
            }
            else if (key == 27) { // Escape для отмены
                lineMode = false;
                waitingForSecondPoint = false;
                cout << "\n[Режим рисования линии отменен]" << endl;
                _getch();
            }
            return;
        }

        if (rectMode) {
            if (key == 13) { // Enter
                if (!waitingForSecondPoint) {
                    // Первый угол
                    rectX1 = canvas->getCursorX();
                    rectY1 = canvas->getCursorY();
                    waitingForSecondPoint = true;

                    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
                    SetConsoleTextAttribute(hConsole, 10);
                    cout << "\n[Первый угол выбран] Переместите курсор ко второму углу и нажмите Enter" << endl;
                    SetConsoleTextAttribute(hConsole, 7);
                    _getch();
                }
                else {
                    // Второй угол - рисуем прямоугольник
                    canvas->drawRect(rectX1, rectY1, canvas->getCursorX(), canvas->getCursorY(), false, canvas->getCurrentChar());
                    rectMode = false;
                    waitingForSecondPoint = false;

                    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
                    SetConsoleTextAttribute(hConsole, 10);
                    cout << "\n[Прямоугольник нарисован! Режим рисования завершен]" << endl;
                    SetConsoleTextAttribute(hConsole, 7);
                    _getch();
                }
            }
            else if (key == 27) { // Escape для отмены
                rectMode = false;
                waitingForSecondPoint = false;
                cout << "\n[Режим рисования прямоугольника отменен]" << endl;
                _getch();
            }
            return;
        }

        // Обработка обычных команд
        switch (key) {
            // Инструменты
        case 'L': case 'l':
            lineMode = true;
            waitingForSecondPoint = false;
            showModeStatus();
            _getch(); // Пауза для чтения сообщения
            break;
        case 'R': case 'r':
            rectMode = true;
            waitingForSecondPoint = false;
            showModeStatus();
            _getch(); // Пауза для чтения сообщения
            break;
        case 'F': case 'f':
            canvas->floodFill(canvas->getCursorX(), canvas->getCursorY(), canvas->getCurrentChar());
            break;
        case 'C': case 'c':
            canvas->clear();
            break;
        case 'U': case 'u':
            canvas->undo();
            break;

            // Работа с файлами
        case 'S': case 's': {
            string filename;
            canvas->render();
            cout << "Имя файла для сохранения (.ascii): ";
            cin >> filename;
            if (filename.find('.') == string::npos) filename += ".ascii";
            if (canvas->saveToFile(filename)) {
                cout << "Сохранено в " << filename << endl;
            }
            else {
                cout << "Ошибка сохранения!" << endl;
            }
            _getch();
            break;
        }
        case 'O': case 'o': {
            string filename;
            canvas->render();
            cout << "Имя файла для загрузки (.ascii или .txt): ";
            cin >> filename;
            if (canvas->loadFromFile(filename)) {
                cout << "Загружено из " << filename << endl;
            }
            else {
                cout << "Ошибка загрузки!" << endl;
            }
            _getch();
            break;
        }

                // Справка и выход
        case 'H': case 'h':
            showHelp();
            break;
        case 'Q': case 'q':
            exit(0);
            break;

            // Печатные символы (установка текущего символа)
        default:
            if (key >= 32 && key <= 126) { // Печатные символы ASCII
                canvas->setCurrentChar(key);
            }
            break;
        }
    }
};

// ==================== ГЛАВНАЯ ФУНКЦИЯ ====================
int main() {
    // Установка кодировки для корректного отображения псевдографики
    SetConsoleOutputCP(CP_UTF8);
    setlocale(LC_ALL, "Russian");

    int width, height;

    cout << "==========================================================" << endl;
    cout << "|                   ASCII DRAW STUDIO                    |" << endl;
    cout << "|                 Редактор псевдографики                 |" << endl;
    cout << "==========================================================" << endl;
    cout << endl;
    cout << "Введите размеры холста (от 40x20 до 200x100)" << endl;
    cout << "Ширина (40-200): ";
    cin >> width;
    cout << "Высота (20-100): ";
    cin >> height;

    Canvas canvas(width, height);
    InputHandler handler(&canvas);

    // Основной цикл программы
    while (true) {
        canvas.render();

        char key = _getch();

        // Обработка специальных клавиш (стрелки)
        if (key == -32 || key == 224) {
            key = _getch();
            switch (key) {
            case 72: canvas.moveCursor(0, -1); break;  // вверх
            case 80: canvas.moveCursor(0, 1); break;   // вниз
            case 75: canvas.moveCursor(-1, 0); break;  // влево
            case 77: canvas.moveCursor(1, 0); break;   // вправо
            }
        }
        else {
            handler.handleKeyPress(key);
        }
    }

    return 0;
}