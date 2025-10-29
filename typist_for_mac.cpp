#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <limits>
#include <CoreGraphics/CoreGraphics.h>

// UTF-8 转换为 UTF-32
static std::u32string utf8_to_u32string(const std::string &utf8) {
    std::u32string out;
    char32_t ch = 0;
    int bytes = 0;
    for (unsigned char c : utf8) {
        if (c <= 0x7F) { // 单字节
            if (bytes) { out.push_back(ch); bytes = 0; }
            out.push_back(c);
        } else if ((c & 0xC0) == 0x80) { // 连续字节
            if (bytes) {
                ch = (ch << 6) | (c & 0x3F);
                if (--bytes == 0) out.push_back(ch);
            }
        } else if ((c & 0xE0) == 0xC0) { // 两字节
            ch = c & 0x1F;
            bytes = 1;
        } else if ((c & 0xF0) == 0xE0) { // 三字节
            ch = c & 0x0F;
            bytes = 2;
        } else if ((c & 0xF8) == 0xF0) { // 四字节
            ch = c & 0x07;
            bytes = 3;
        }
    }
    if (bytes == 0 && ch) out.push_back(ch);
    return out;
}

// --- 模拟按下与释放普通按键 ---
void sendKey(CGKeyCode keyCode) {
    CGEventRef down = CGEventCreateKeyboardEvent(NULL, keyCode, true);
    CGEventRef up   = CGEventCreateKeyboardEvent(NULL, keyCode, false);
    CGEventPost(kCGHIDEventTap, down);
    CGEventPost(kCGHIDEventTap, up);
    CFRelease(down);
    CFRelease(up);
    std::this_thread::sleep_for(std::chrono::milliseconds(8));
}

// --- 发送 Unicode 字符---
void sendUnicodeChar(char32_t codepoint) {
    UniChar chars[2];
    CFIndex len = 0;
    if (codepoint <= 0xFFFF) {
        chars[0] = static_cast<UniChar>(codepoint);
        len = 1;
    } else {
        codepoint -= 0x10000;
        chars[0] = static_cast<UniChar>(0xD800 + (codepoint >> 10));
        chars[1] = static_cast<UniChar>(0xDC00 + (codepoint & 0x3FF));
        len = 2;
    }

    CGEventRef eventDown = CGEventCreateKeyboardEvent(NULL, 0, true);
    CGEventKeyboardSetUnicodeString(eventDown, len, chars);
    CGEventPost(kCGHIDEventTap, eventDown);

    CGEventRef eventUp = CGEventCreateKeyboardEvent(NULL, 0, false);
    CGEventKeyboardSetUnicodeString(eventUp, len, chars);
    CGEventPost(kCGHIDEventTap, eventUp);

    CFRelease(eventDown);
    CFRelease(eventUp);
    std::this_thread::sleep_for(std::chrono::milliseconds(6));
}

// --- 键入整段文本 ---
void typeText(const std::u32string &text, int delayMs = 6) {
    for (size_t i = 0; i < text.size(); ++i) {
        char32_t ch = text[i];
        switch (ch) {
            case U'\r': break; // 忽略 CR
            case U'\n': sendKey((CGKeyCode)36); break; // Enter
            case U'\t': sendKey((CGKeyCode)48); break; // Tab
            case U'\b': sendKey((CGKeyCode)51); break; // Backspace
            default:
                sendUnicodeChar(ch);
                break;
        }
        if (delayMs > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
        if ((i + 1) % 10 == 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::cout << u8"请输入文本（Ctrl+D 结束输入）：\n";
    std::cout.flush();

    std::string input;
    char c;
    while (std::cin.get(c)) {
        input += c;
    }

    if (input.empty()) {
        std::cout << u8"无文本输入，程序退出。\n";
        return 0;
    }

    std::u32string text32 = utf8_to_u32string(input);
    if (text32.empty()) {
        std::cout << u8"UTF-8 转换失败，退出。\n";
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

    typeText(text32, 10);

    std::cout << u8"\n已完成，按回车退出。\n";
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    return 0;
}
