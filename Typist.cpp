#define NOMINMAX
#include <iostream>
#include <string>
#include <vector>
#include <windows.h>
#include <thread>
#include <chrono>
#include <limits>

//将 UTF-8 字符串转为 UTF-16 (std::wstring)
static std::wstring utf8_to_wstring(const std::string& utf8) {
    if (utf8.empty()) return std::wstring();
    int required = ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), nullptr, 0);
    if (required == 0) return std::wstring();
    std::wstring out;
    out.resize(required);
    ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), &out[0], required);
    return out;
}

// 发送虚拟键
void sendVkKey(WORD vk, DWORD extraFlags = 0) {
    INPUT input = { 0 };
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vk;
    input.ki.dwFlags = extraFlags; // 0 表示按下
    SendInput(1, &input, sizeof(INPUT));
    std::this_thread::sleep_for(std::chrono::milliseconds(8));
    input.ki.dwFlags = KEYEVENTF_KEYUP | extraFlags;
    SendInput(1, &input, sizeof(INPUT));
    std::this_thread::sleep_for(std::chrono::milliseconds(8));
}

// 通过 KEYEVENTF_UNICODE 发送单个 UTF-16 code unit（wchar_t）
void sendUnicodeUnit(wchar_t wc) {
    INPUT input = { 0 };
    input.type = INPUT_KEYBOARD;
    input.ki.wScan = wc;
    input.ki.dwFlags = KEYEVENTF_UNICODE;
    input.ki.wVk = 0;
    SendInput(1, &input, sizeof(INPUT));
    std::this_thread::sleep_for(std::chrono::milliseconds(4));
    input.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
    SendInput(1, &input, sizeof(INPUT));
    std::this_thread::sleep_for(std::chrono::milliseconds(4));
}

// 将一个完整的 wstring 输出到目标，把 '\n' 映射为 Enter
void typeText(const std::wstring& text, int delayMs = 6) {
    for (size_t i = 0; i < text.size(); ++i) {
        wchar_t ch = text[i];
        if (ch == L'\r') {
            // 忽略 CR，等 \n 触发 Enter
            continue;
        }
        else if (ch == L'\n') {
            sendVkKey(VK_RETURN);
        }
        else if (ch == 8) {
            sendVkKey(VK_BACK);
        }
        else if (ch == L'\t') {
            sendVkKey(VK_TAB);
        }
        else {
            sendUnicodeUnit(ch);
        }

        if (delayMs > 0) std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
        if ((i + 1) % 10 == 0) std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

int main() {
    // 保存控制台原始输入模式
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD originalMode;
    GetConsoleMode(hStdin, &originalMode);

    SetConsoleMode(hStdin, originalMode & ~ENABLE_PROCESSED_INPUT);

    // 切换控制台到 UTF-8，避免中文提示乱码
    system("chcp 65001 > nul");
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    std::cout << u8"请输入文本，输入 Ctrl+Z 后回车结束输入：\n";
    std::cout.flush();

    // 逐个字符读取输入，检测行内的Ctrl+Z
    std::string all_utf8;
    int c;
    bool sawCtrlZ = false;

    while ((c = std::cin.get()) != EOF) {
        if (c == 26) {
            sawCtrlZ = true;
            break;
        }
        all_utf8 += static_cast<char>(c);
    }

    // 恢复控制台原始模式
    SetConsoleMode(hStdin, originalMode);

    if (all_utf8.empty() && !sawCtrlZ) {
        std::cout << u8"无文本输入，程序退出。\n";
        return 0;
    }

    // 把 UTF-8 转为 UTF-16（wstring），确保正确解码所有中文与符号
    std::wstring wtext = utf8_to_wstring(all_utf8);
    if (wtext.empty() && !all_utf8.empty()) {
        std::cout << u8"转换为宽字符失败，退出。\n";
        return 0;
    }

    std::cout << u8"\n五秒后开始模拟键盘输出，请将光标移至目标窗口...\n";
    std::cout.flush();
    for (int i = 5; i > 0; --i) {
        std::cout << u8"倒计时 " << i << u8" 秒\r";
        std::cout.flush();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    std::cout << u8"\n开始模拟键盘输出...\n";
    std::cout.flush();

    // 速度控制：每个字符间隔 10 毫秒
    typeText(wtext, 10);

    std::cout << u8"\n已完成，按回车退出。\n";
    std::cout.flush();

    // 清理并等待回车
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    return 0;
}
