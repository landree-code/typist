#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <limits>
#include <CoreGraphics/CoreGraphics.h>

//常用虚拟键码
#ifndef kVK_Return
#define kVK_Return 0x24
#endif
#ifndef kVK_Delete
#define kVK_Delete 0x33
#endif
#ifndef kVK_Tab
#define kVK_Tab 0x30
#endif

// UTF-8 -> UTF-32
static std::u32string utf8_to_u32(const std::string &utf8) {
    std::u32string out;
    char32_t codepoint = 0;
    int remaining = 0;
    for (unsigned char ch : utf8) {
        if (remaining == 0) {
            if (ch <= 0x7F) {
                out.push_back((char32_t)ch);
            } else if ((ch & 0xE0) == 0xC0) {
                codepoint = ch & 0x1F;
                remaining = 1;
            } else if ((ch & 0xF0) == 0xE0) {
                codepoint = ch & 0x0F;
                remaining = 2;
            } else if ((ch & 0xF8) == 0xF0) {
                codepoint = ch & 0x07;
                remaining = 3;
            } else {
                // 非法起始字节，跳过
            }
        } else {
            codepoint = (codepoint << 6) | (ch & 0x3F);
            --remaining;
            if (remaining == 0) {
                out.push_back(codepoint);
                codepoint = 0;
            }
        }
    }
    // 若 remaining != 0 表示截断的 UTF-8，忽略残余
    return out;
}

// 直接发送单个 Unicode 字符（仅发送 keyDown/Unicode 注入，不发布额外物理 keycode）
static void sendUnicodeChar(char32_t cp) {
    // 转为 UTF-16 (UniChar) 单元
    UniChar buf[2];
    CFIndex len = 0;
    if (cp <= 0xFFFF) {
        buf[0] = static_cast<UniChar>(cp);
        len = 1;
    } else {
        cp -= 0x10000;
        buf[0] = static_cast<UniChar>(0xD800 + (cp >> 10));
        buf[1] = static_cast<UniChar>(0xDC00 + (cp & 0x3FF));
        len = 2;
    }

    // 创建 keyDown 事件并设置 Unicode 字符串，再发布
    CGEventRef ev = CGEventCreateKeyboardEvent(NULL, (CGKeyCode)0, true);
    if (!ev) return;
    CGEventKeyboardSetUnicodeString(ev, len, buf);
    CGEventPost(kCGHIDEventTap, ev);
    CFRelease(ev);

    // 短延时，避免事件队列重入或丢失
    std::this_thread::sleep_for(std::chrono::milliseconds(6));
}

// 发送物理特殊键：发送 keyDown + keyUp（用于 Enter/Delete/Tab）
static void sendKeyPress(CGKeyCode key) {
    CGEventRef down = CGEventCreateKeyboardEvent(NULL, key, true);
    CGEventRef up   = CGEventCreateKeyboardEvent(NULL, key, false);
    if (down) { CGEventPost(kCGHIDEventTap, down); CFRelease(down); }
    if (up)   { CGEventPost(kCGHIDEventTap, up);   CFRelease(up);   }
    std::this_thread::sleep_for(std::chrono::milliseconds(6));
}

// 将完整的 UTF-32 文本逐字符发送，'\n' 映射为 Return，'\t' 为 Tab，'\b' 为 Delete
static void typeText(const std::u32string &text, int delayMs = 8) {
    for (size_t i = 0; i < text.size(); ++i) {
        char32_t ch = text[i];
        if (ch == U'\r') {
            continue;
        } else if (ch == U'\n') {
            sendKeyPress(kVK_Return);
        } else if (ch == U'\t') {
            sendKeyPress(kVK_Tab);
        } else if (ch == U'\b') {
            sendKeyPress(kVK_Delete);
        } else {
            // BMP 直接发送；补充平面拆 surrogate 后分别发送
            if (ch <= 0xFFFF) {
                sendUnicodeChar(ch);
            } else {
                char32_t t = ch - 0x10000;
                char32_t high = 0xD800 + (t >> 10);
                char32_t low  = 0xDC00 + (t & 0x3FF);
                sendUnicodeChar(high);
                sendUnicodeChar(low);
            }
        }

        if (delayMs > 0) std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
        if ((i + 1) % 10 == 0) std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::cout << u8"请输入文本，按 Ctrl+D (EOF) 结束输入：\n";
    std::cout.flush();

    // 读取全部 stdin
    std::string line;
    std::string all;
    while (std::getline(std::cin, line)) {
        all += line;
        all.push_back('\n');
    }

    if (all.empty()) {
        std::cout << u8"无文本输入，程序退出。\n";
        return 0;
    }

    // UTF-8 -> UTF-32
    std::u32string text32 = utf8_to_u32(all);
    if (text32.empty()) {
        std::cout << u8"转换失败或文本为空，退出。\n";
        return 0;
    }

    std::cout << u8"\n五秒后开始模拟键盘输出，请将光标移至目标窗口...\n";
    std::cout.flush();
    for (int i = 5; i > 0; --i) {
        std::cout << u8"倒计时 " << i << u8" 秒\r" << std::flush;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    std::cout << u8"\n开始模拟键盘输出...\n";
    std::cout.flush();

    // 注入文本
    typeText(text32, 10);

    std::cout << u8"\n已完成，按回车退出。\n";
    std::cout.flush();

    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    return 0;
}
