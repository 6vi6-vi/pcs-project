#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <stack>
#include <conio.h>
#include <windows.h>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <cassert>
#include <ctime>

using namespace std;

// ==================== ПРЕДВАРИТЕЛЬНЫЕ ОБЪЯВЛЕНИЯ ====================
class Canvas;
class Command;
class CommandHistory;
class ToolState;
class EditorContext;
class CanvasObserver;

// ==================== КЛАСС MEMENTO ====================
class Memento {
private:
    const vector<vector<char>> snapshot;
    const int cursorX;
    const int cursorY;
    const char currentChar;

public:
    Memento(vector<vector<char>>&& grid, int x, int y, char ch) noexcept
        : snapshot(std::move(grid)), cursorX(x), cursorY(y), currentChar(ch) {
    }

    Memento(const Memento&) = delete;
    Memento& operator=(const Memento&) = delete;
    Memento(Memento&&) = default;
    Memento& operator=(Memento&&) = default;

    const vector<vector<char>>& getGrid() const { return snapshot; }
    int getCursorX() const { return cursorX; }
    int getCursorY() const { return cursorY; }
    char getCurrentChar() const { return currentChar; }
};

// ==================== ИНТЕРФЕЙС НАБЛЮДАТЕЛЯ ====================
class CanvasObserver {
public:
    virtual ~CanvasObserver() = default;
    virtual void onCanvasChanged(const Canvas& canvas) = 0;
    virtual void onStateChanged(const string& message) = 0;
    virtual void onToolChanged(const string& toolName, const string& statusMsg) = 0;
};

// ==================== SINGLETON - ЛОГГЕР ПРИЛОЖЕНИЯ ====================
class AppLogger {
private:
    static unique_ptr<AppLogger> instance;
    ofstream logFile;
    bool consoleOutput;

    // Приватный конструктор
    AppLogger() : consoleOutput(true) {
        logFile.open("ascii_draw.log", ios::out | ios::app);
        if (logFile.is_open()) {
            logFile << "=== ASCII Draw Studio Session Started ===" << endl;
        }
    }

    // Запрет копирования и присваивания
    AppLogger(const AppLogger&) = delete;
    AppLogger& operator=(const AppLogger&) = delete;

    // Вспомогательный метод для безопасного получения времени
    string getCurrentTime() {
        time_t now = time(0);
        char buffer[26];
        ctime_s(buffer, sizeof(buffer), &now);
        // Убираем перевод строки в конце
        string timeStr(buffer);
        if (!timeStr.empty() && timeStr.back() == '\n') {
            timeStr.pop_back();
        }
        return timeStr;
    }

public:
    ~AppLogger() {
        if (logFile.is_open()) {
            logFile << "=== Session Ended ===" << endl;
            logFile.close();
        }
    }

    // Получение единственного экземпляра
    static AppLogger* getInstance() {
        if (!instance) {
            // Используем new вместо make_unique для обхода приватного конструктора
            instance.reset(new AppLogger());
        }
        return instance.get();
    }

    void log(const string& message) {
        string timestamp = getCurrentTime();
        if (logFile.is_open()) {
            logFile << "[" << timestamp << "] " << message << endl;
        }
        if (consoleOutput) {
            cout << "[LOG] " << message << endl;
        }
    }

    void setConsoleOutput(bool enabled) {
        consoleOutput = enabled;
    }

    void warning(const string& message) {
        log("WARNING: " + message);
    }

    void error(const string& message) {
        log("ERROR: " + message);
    }
};

// Инициализация статического члена
unique_ptr<AppLogger> AppLogger::instance = nullptr;

// ==================== БАЗОВЫЙ КЛАСС COMMAND ====================
class Command {
protected:
    Canvas* canvas;
    unique_ptr<Memento> backup;

public:
    explicit Command(Canvas* c) : canvas(c), backup(nullptr) {
        if (!canvas) {
            AppLogger::getInstance()->error("Command created with null Canvas pointer");
            throw invalid_argument("Canvas cannot be null");
        }
    }

    virtual ~Command() = default;

    void saveBackup();
    void undo();

    virtual void execute() = 0;
    virtual string getDescription() const = 0;

    Command(const Command&) = delete;
    Command& operator=(const Command&) = delete;
    Command(Command&&) = default;
    Command& operator=(Command&&) = default;
};

// ==================== КЛАСС CANVAS ====================
class Canvas {
private:
    int width;
    int height;
    vector<vector<char>> grid;
    int cursorX;
    int cursorY;
    char currentChar;
    vector<CanvasObserver*> observers;

public:
    Canvas(int w, int h) : width(w), height(h), cursorX(0), cursorY(0), currentChar('@') {
        if (width < 40) width = 40;
        if (width > 200) width = 200;
        if (height < 20) height = 20;
        if (height > 100) height = 100;
        grid = vector<vector<char>>(height, vector<char>(width, '.'));

        AppLogger::getInstance()->log("Canvas created: " + to_string(width) + "x" + to_string(height));
    }

    void attachObserver(CanvasObserver* observer) {
        if (observer) {
            observers.push_back(observer);
            AppLogger::getInstance()->log("Observer attached");
        }
    }

    void detachObserver(CanvasObserver* observer) {
        auto it = find(observers.begin(), observers.end(), observer);
        if (it != observers.end()) {
            observers.erase(it);
            AppLogger::getInstance()->log("Observer detached");
        }
    }

    void notifyCanvasChanged() {
        for (auto observer : observers) {
            observer->onCanvasChanged(*this);
        }
    }

    void notifyStateChanged(const string& message) {
        AppLogger::getInstance()->log("State: " + message);
        for (auto observer : observers) {
            observer->onStateChanged(message);
        }
    }

    void notifyToolChanged(const string& toolName, const string& statusMsg) {
        AppLogger::getInstance()->log("Tool changed: " + toolName);
        for (auto observer : observers) {
            observer->onToolChanged(toolName, statusMsg);
        }
    }

    int getWidth() const { return width; }
    int getHeight() const { return height; }
    int getCursorX() const { return cursorX; }
    int getCursorY() const { return cursorY; }
    char getCurrentChar() const { return currentChar; }
    char getPixel(int x, int y) const {
        if (x >= 0 && x < width && y >= 0 && y < height) {
            return grid[y][x];
        }
        return '.';
    }

    vector<vector<char>> getGridSnapshot() const { return grid; }

    void restoreFromMemento(const Memento& memento) {
        grid = memento.getGrid();
        cursorX = memento.getCursorX();
        cursorY = memento.getCursorY();
        currentChar = memento.getCurrentChar();
        notifyCanvasChanged();
    }

    void setCurrentChar(char ch) {
        currentChar = ch;
        notifyStateChanged("Текущий символ изменён на '" + string(1, ch) + "'");
    }

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
        notifyCanvasChanged();
    }

    void setCursorPosition(int x, int y) {
        if (x >= 0 && x < width) cursorX = x;
        if (y >= 0 && y < height) cursorY = y;
        notifyCanvasChanged();
    }

    void drawLineImpl(int x1, int y1, int x2, int y2, char ch) {
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
        notifyCanvasChanged();
    }

    void drawRectImpl(int x1, int y1, int x2, int y2, bool fill, char ch) {
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
            for (int x = x1; x <= x2; x++) {
                setPixel(x, y1, ch);
                setPixel(x, y2, ch);
            }
            for (int y = y1; y <= y2; y++) {
                setPixel(x1, y, ch);
                setPixel(x2, y, ch);
            }
        }
        notifyCanvasChanged();
    }

    void floodFillImpl(int x, int y, char newChar) {
        if (x < 0 || x >= width || y < 0 || y >= height) return;

        char targetChar = grid[y][x];
        if (targetChar == newChar) return;

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
        notifyCanvasChanged();
        notifyStateChanged("Область залита символом '" + string(1, newChar) + "'");
    }

    void clearImpl() {
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                grid[y][x] = '.';
            }
        }
        notifyCanvasChanged();
        notifyStateChanged("Холст очищен");
    }

    bool saveToFile(const string& filename) {
        ofstream file(filename);
        if (!file.is_open()) {
            AppLogger::getInstance()->error("Cannot open file for saving: " + filename);
            return false;
        }

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                file << grid[y][x];
            }
            file << endl;
        }
        AppLogger::getInstance()->log("Canvas saved to file: " + filename);
        notifyStateChanged("Сохранено в файл: " + filename);
        return true;
    }

    bool loadFromFile(const string& filename) {
        ifstream file(filename);
        if (!file.is_open()) {
            AppLogger::getInstance()->error("Cannot open file for loading: " + filename);
            return false;
        }

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
        AppLogger::getInstance()->log("Canvas loaded from file: " + filename);
        notifyCanvasChanged();
        notifyStateChanged("Загружено из файла: " + filename);
        return true;
    }
};

// ==================== КЛАСС HISTORY ====================
class CommandHistory {
private:
    stack<unique_ptr<Command>> undoStack;
    stack<unique_ptr<Command>> redoStack;

public:
    CommandHistory() = default;
    ~CommandHistory() = default;

    void executeCommand(unique_ptr<Command> cmd) {
        if (!cmd) {
            AppLogger::getInstance()->warning("Attempt to execute null command");
            return;
        }

        try {
            cmd->execute();
            undoStack.push(std::move(cmd));

            while (!redoStack.empty()) {
                redoStack.pop();
            }
            AppLogger::getInstance()->log("Command executed successfully");
        }
        catch (const exception& e) {
            string errorMsg = "Command execution failed: " + string(e.what());
            cerr << errorMsg << endl;
            AppLogger::getInstance()->error(errorMsg);
        }
    }

    void undo() {
        if (undoStack.empty()) {
            AppLogger::getInstance()->log("Nothing to undo");
            return;
        }

        unique_ptr<Command> cmd = std::move(undoStack.top());
        undoStack.pop();

        try {
            cmd->undo();
            redoStack.push(std::move(cmd));
            AppLogger::getInstance()->log("Undo performed");
        }
        catch (const exception& e) {
            string errorMsg = "Undo failed: " + string(e.what());
            cerr << errorMsg << endl;
            AppLogger::getInstance()->error(errorMsg);
            undoStack.push(std::move(cmd));
        }
    }

    void redo() {
        if (redoStack.empty()) {
            AppLogger::getInstance()->log("Nothing to redo");
            return;
        }

        unique_ptr<Command> cmd = std::move(redoStack.top());
        redoStack.pop();

        try {
            cmd->execute();
            undoStack.push(std::move(cmd));
            AppLogger::getInstance()->log("Redo performed");
        }
        catch (const exception& e) {
            string errorMsg = "Redo failed: " + string(e.what());
            cerr << errorMsg << endl;
            AppLogger::getInstance()->error(errorMsg);
            redoStack.push(std::move(cmd));
        }
    }

    bool canUndo() const { return !undoStack.empty(); }
    bool canRedo() const { return !redoStack.empty(); }
};

// ==================== БАЗОВЫЙ КЛАСС STATE ====================
class ToolState {
protected:
    EditorContext* context = nullptr;
public:
    virtual ~ToolState() = default;
    virtual void setContext(EditorContext* ctx) { context = ctx; }

    virtual void onKeyPress(char key) {}
    virtual void onCursorMove(int dx, int dy) {}
    virtual string getName() const { return "Инструмент"; }
    virtual string getStatusMessage() const { return ""; }
    virtual bool isDrawingMode() const { return false; }
};

// ==================== КЛАСС EDITOR CONTEXT ====================
class EditorContext {
private:
    unique_ptr<ToolState> currentState;
    Canvas* canvas;
    CommandHistory* history;

public:
    EditorContext(Canvas* c, CommandHistory* h) : canvas(c), history(h) {
        if (!canvas || !history) {
            AppLogger::getInstance()->error("EditorContext initialized with null pointers");
            throw invalid_argument("Null pointers in EditorContext");
        }
        AppLogger::getInstance()->log("EditorContext created");
    }

    ~EditorContext() = default;

    void setState(unique_ptr<ToolState> newState) {
        if (newState) {
            newState->setContext(this);
            currentState = std::move(newState);
        }
        if (canvas) {
            canvas->notifyToolChanged(getCurrentToolName(), getStatusMessage());
        }
    }

    ToolState* getState() const { return currentState.get(); }

    Canvas* getCanvas() const { return canvas; }
    CommandHistory* getHistory() const { return history; }

    void executeCommand(unique_ptr<Command> cmd) {
        if (history && cmd) {
            history->executeCommand(std::move(cmd));
        }
    }

    void undo() {
        if (history) history->undo();
    }

    void redo() {
        if (history) history->redo();
    }

    void moveCursor(int dx, int dy) {
        if (canvas) canvas->moveCursor(dx, dy);
    }

    void handleKeyPress(char key) {
        if (currentState) {
            currentState->onKeyPress(key);
        }
        if (canvas) {
            canvas->notifyToolChanged(getCurrentToolName(), getStatusMessage());
        }
    }

    void handleCursorMove(int dx, int dy) {
        if (currentState) {
            currentState->onCursorMove(dx, dy);
        }
        else if (canvas) {
            canvas->moveCursor(dx, dy);
        }
    }

    string getCurrentToolName() const {
        return currentState ? currentState->getName() : "Курсор";
    }

    string getStatusMessage() const {
        return currentState ? currentState->getStatusMessage() : "";
    }

    bool isDrawingMode() const {
        return currentState ? currentState->isDrawingMode() : false;
    }
};

// ==================== КОНКРЕТНЫЕ КОМАНДЫ ====================
class DrawLineCommand : public Command {
private:
    int x1, y1, x2, y2;
    char ch;

public:
    DrawLineCommand(Canvas* c, int x1, int y1, int x2, int y2, char ch)
        : Command(c), x1(x1), y1(y1), x2(x2), y2(y2), ch(ch) {
    }

    void execute() override {
        saveBackup();
        canvas->drawLineImpl(x1, y1, x2, y2, ch);
        canvas->notifyStateChanged("Линия нарисована");
    }

    string getDescription() const override {
        return "Рисование линии от (" + to_string(x1) + "," + to_string(y1) +
            ") до (" + to_string(x2) + "," + to_string(y2) + ")";
    }
};

class DrawRectCommand : public Command {
private:
    int x1, y1, x2, y2;
    bool fill;
    char ch;

public:
    DrawRectCommand(Canvas* c, int x1, int y1, int x2, int y2, bool fill, char ch)
        : Command(c), x1(x1), y1(y1), x2(x2), y2(y2), fill(fill), ch(ch) {
    }

    void execute() override {
        saveBackup();
        canvas->drawRectImpl(x1, y1, x2, y2, fill, ch);
        canvas->notifyStateChanged("Прямоугольник нарисован");
    }

    string getDescription() const override {
        return "Рисование прямоугольника";
    }
};

class FloodFillCommand : public Command {
private:
    int x, y;
    char newChar;

public:
    FloodFillCommand(Canvas* c, int x, int y, char newChar)
        : Command(c), x(x), y(y), newChar(newChar) {
    }

    void execute() override {
        saveBackup();
        canvas->floodFillImpl(x, y, newChar);
    }

    string getDescription() const override {
        return "Заливка в точке (" + to_string(x) + "," + to_string(y) + ")";
    }
};

class ClearCommand : public Command {
public:
    explicit ClearCommand(Canvas* c) : Command(c) {}

    void execute() override {
        saveBackup();
        canvas->clearImpl();
    }

    string getDescription() const override {
        return "Очистка всего холста";
    }
};

// ==================== КЛАСС ДЛЯ ОТОБРАЖЕНИЯ СПРАВКИ ====================
class HelpDisplay {
public:
    static void show(Canvas* canvas = nullptr) {
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
        cout << "  Ctrl+Z - отменить (альтернатива)" << endl;
        cout << "  Ctrl+Y - повторить" << endl;
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
        if (canvas) canvas->notifyCanvasChanged();
    }
};

// ==================== РЕАЛИЗАЦИЯ МЕТОДОВ COMMAND ====================
void Command::saveBackup() {
    backup = make_unique<Memento>(
        canvas->getGridSnapshot(),
        canvas->getCursorX(),
        canvas->getCursorY(),
        canvas->getCurrentChar()
    );
}

void Command::undo() {
    if (backup) {
        canvas->restoreFromMemento(*backup);
        canvas->notifyStateChanged("Действие отменено");
    }
}

// ==================== КОНКРЕТНЫЙ НАБЛЮДАТЕЛЬ - ОТОБРАЖЕНИЕ ====================
class ConsoleRenderer : public CanvasObserver {
private:
    int width;
    int height;
    string currentToolName;
    string currentStatusMsg;

public:
    ConsoleRenderer(int w, int h) : width(w), height(h) {}

    void onCanvasChanged(const Canvas& canvas) override {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        system("cls");

        cout << "+";
        for (int x = 0; x < width + 2; x++) cout << "-";
        cout << "+" << endl;

        for (int y = 0; y < height; y++) {
            cout << "| ";
            for (int x = 0; x < width; x++) {
                if (x == canvas.getCursorX() && y == canvas.getCursorY()) {
                    SetConsoleTextAttribute(hConsole, 112);
                    cout << canvas.getPixel(x, y);
                    SetConsoleTextAttribute(hConsole, 7);
                }
                else {
                    cout << canvas.getPixel(x, y);
                }
            }
            cout << " |" << endl;
        }

        cout << "+";
        for (int x = 0; x < width + 2; x++) cout << "-";
        cout << "+" << endl;

        cout << " ===============================================================================" << endl;
        cout << "| Текущий символ: [" << canvas.getCurrentChar() << "]     Активный инструмент: " << currentToolName;
        for (int i = 0; i < 35 - (int)currentToolName.length(); i++) cout << " ";
        cout << "|" << endl;

        if (!currentStatusMsg.empty()) {
            SetConsoleTextAttribute(hConsole, 14);
            cout << "| [СТАТУС] " << currentStatusMsg;
            for (int i = 0; i < 58 - (int)currentStatusMsg.length(); i++) cout << " ";
            cout << "|" << endl;
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

    void onStateChanged(const string& message) override {
        currentStatusMsg = message;
    }

    void onToolChanged(const string& toolName, const string& statusMsg) override {
        currentToolName = toolName;
        currentStatusMsg = statusMsg;
    }
};

// ==================== КОНКРЕТНЫЕ СОСТОЯНИЯ ====================
class CursorState : public ToolState {
public:
    void onCursorMove(int dx, int dy) override {
        if (context) context->getCanvas()->moveCursor(dx, dy);
    }

    string getName() const override { return "Курсор"; }
    string getStatusMessage() const override { return "Режим выбора"; }
};

class LineToolState : public ToolState {
private:
    bool waitingForSecondPoint = false;
    int startX = 0, startY = 0;
public:
    void onKeyPress(char key) override;
    void onCursorMove(int dx, int dy) override;
    string getName() const override { return "Линия"; }
    string getStatusMessage() const override {
        return waitingForSecondPoint ? "Выберите вторую точку линии (Enter)" : "Выберите первую точку линии (Enter)";
    }
    bool isDrawingMode() const override { return true; }
};

class RectToolState : public ToolState {
private:
    bool waitingForSecondPoint = false;
    int startX = 0, startY = 0;
public:
    void onKeyPress(char key) override;
    void onCursorMove(int dx, int dy) override;
    string getName() const override { return "Прямоугольник"; }
    string getStatusMessage() const override {
        return waitingForSecondPoint ? "Выберите второй угол (Enter)" : "Выберите первый угол (Enter)";
    }
    bool isDrawingMode() const override { return true; }
};

// ==================== РЕАЛИЗАЦИЯ МЕТОДОВ СОСТОЯНИЙ ====================
void LineToolState::onKeyPress(char key) {
    if (!context) return;
    Canvas* canvas = context->getCanvas();

    if (key == 13) {
        if (!waitingForSecondPoint) {
            startX = canvas->getCursorX();
            startY = canvas->getCursorY();
            waitingForSecondPoint = true;
            canvas->notifyStateChanged("Первая точка выбрана. Переместите курсор ко второй точке и нажмите Enter");
        }
        else {
            // Создаём команду напрямую, без фабрики
            context->executeCommand(make_unique<DrawLineCommand>(
                canvas, startX, startY,
                canvas->getCursorX(), canvas->getCursorY(),
                canvas->getCurrentChar()
            ));
            context->setState(make_unique<CursorState>());
            canvas->notifyStateChanged("Линия нарисована. Возврат в режим курсора");
        }
    }
    else if (key == 27) {
        waitingForSecondPoint = false;
        context->setState(make_unique<CursorState>());
        canvas->notifyStateChanged("Режим рисования линии отменен");
    }
}

void LineToolState::onCursorMove(int dx, int dy) {
    if (context) context->getCanvas()->moveCursor(dx, dy);
}

void RectToolState::onKeyPress(char key) {
    if (!context) return;
    Canvas* canvas = context->getCanvas();

    if (key == 13) {
        if (!waitingForSecondPoint) {
            startX = canvas->getCursorX();
            startY = canvas->getCursorY();
            waitingForSecondPoint = true;
            canvas->notifyStateChanged("Первый угол выбран. Переместите курсор ко второму углу и нажмите Enter");
        }
        else {
            // Создаём команду напрямую, без фабрики
            context->executeCommand(make_unique<DrawRectCommand>(
                canvas, startX, startY,
                canvas->getCursorX(), canvas->getCursorY(),
                false, canvas->getCurrentChar()
            ));
            context->setState(make_unique<CursorState>());
            canvas->notifyStateChanged("Прямоугольник нарисован. Возврат в режим курсора");
        }
    }
    else if (key == 27) {
        waitingForSecondPoint = false;
        context->setState(make_unique<CursorState>());
        canvas->notifyStateChanged("Режим рисования прямоугольника отменен");
    }
}

void RectToolState::onCursorMove(int dx, int dy) {
    if (context) context->getCanvas()->moveCursor(dx, dy);
}

// ==================== КЛАСС INPUT HANDLER ====================
class InputHandler {
private:
    EditorContext* context;

public:
    explicit InputHandler(EditorContext* ctx) : context(ctx) {
        AppLogger::getInstance()->log("InputHandler created");
    }

    void handleKeyPress(char key) {
        if (!context) return;
        Canvas* canvas = context->getCanvas();
        if (!canvas) return;

        

        if ((key == 'L' || key == 'l') && !context->isDrawingMode()) {
            context->setState(make_unique<LineToolState>());
            canvas->notifyCanvasChanged();
            return;
        }

        if ((key == 'R' || key == 'r') && !context->isDrawingMode()) {
            context->setState(make_unique<RectToolState>());
            canvas->notifyCanvasChanged();
            return;
        }

        if (!context->isDrawingMode()) {
            switch (key) {
            case 'F': case 'f': {
                // Создаём команду напрямую, без фабрики
                context->executeCommand(make_unique<FloodFillCommand>(
                    canvas, canvas->getCursorX(), canvas->getCursorY(),
                    canvas->getCurrentChar()
                ));
                canvas->notifyCanvasChanged();
                return;
            }
            case 'C': case 'c': {
                // Создаём команду напрямую, без фабрики
                context->executeCommand(make_unique<ClearCommand>(canvas));
                canvas->notifyCanvasChanged();
                return;
            }
            case 'U': case 'u':
                context->undo();
                canvas->notifyCanvasChanged();
                return;
            case 'S': case 's': {
                string filename;
                cout << "Имя файла для сохранения (.ascii): ";
                cin >> filename;
                if (filename.find('.') == string::npos) filename += ".ascii";
                canvas->saveToFile(filename);
                _getch();
                canvas->notifyCanvasChanged();
                return;
            }
            case 'O': case 'o': {
                string filename;
                cout << "Имя файла для загрузки (.ascii или .txt): ";
                cin >> filename;
                canvas->loadFromFile(filename);
                _getch();
                canvas->notifyCanvasChanged();
                return;
            }
            case 'H': case 'h':
                HelpDisplay::show(canvas);
                return;
            case 'Q': case 'q':
                AppLogger::getInstance()->log("Application closed by user");
                exit(0);
                return;
            default:
                if (key >= 32 && key <= 126) {
                    canvas->setCurrentChar(key);
                    canvas->notifyCanvasChanged();
                }
                return;
            }
        }

        context->handleKeyPress(key);
        canvas->notifyCanvasChanged();
    }

    void handleCursorMove(int dx, int dy) {
        if (context) context->handleCursorMove(dx, dy);
    }
};

// ==================== ГЛАВНАЯ ФУНКЦИЯ ====================
int main() {
    try {
        SetConsoleOutputCP(CP_UTF8);
        setlocale(LC_ALL, "Russian");

        // Singleton логгер уже готов к использованию
        AppLogger::getInstance()->log("Application started");
        AppLogger::getInstance()->setConsoleOutput(false); // Логи в консоль отключаем, только в файл

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

        AppLogger::getInstance()->log("User selected canvas size: " + to_string(width) + "x" + to_string(height));

        Canvas canvas(width, height);
        CommandHistory history;
        EditorContext context(&canvas, &history);

        ConsoleRenderer renderer(width, height);
        canvas.attachObserver(&renderer);

        context.setState(make_unique<CursorState>());
        InputHandler handler(&context);

        canvas.notifyCanvasChanged();

        while (true) {
            if (!_kbhit()) {
                Sleep(10);
                continue;
            }

            char key = _getch();

            if (key == -32 || key == 224) {
                key = _getch();
                switch (key) {
                case 72: handler.handleCursorMove(0, -1); break;
                case 80: handler.handleCursorMove(0, 1); break;
                case 75: handler.handleCursorMove(-1, 0); break;
                case 77: handler.handleCursorMove(1, 0); break;
                }
            }
            else {
                if (key == 26) {
                    context.undo();
                    canvas.notifyCanvasChanged();
                }
                else if (key == 25) {
                    context.redo();
                    canvas.notifyCanvasChanged();
                }
                else {
                    handler.handleKeyPress(key);
                }
            }
        }
    }
    catch (const exception& e) {
        string errorMsg = "Fatal error: " + string(e.what());
        cerr << errorMsg << endl;
        AppLogger::getInstance()->error(errorMsg);
        system("pause");
        return 1;
    }

    return 0;
}