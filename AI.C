#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <windows.h>
#include <string.h>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

#define TEMP_THRESHOLD 70
#define SMOKE_THRESHOLD 300

typedef struct {
    int is_fire;
    float confidence;
    char message[512]; // 扩大缓冲区以容纳长回复
} AI_Result;

// ⚠️ 请在此处填入你的 DeepSeek API Key
const char* API_KEY = "your API";

AI_Result CallDeepSeek_API(int temperature, int smoke) {
    AI_Result result = {0, 0.0, "无结果"};

    // 1. 构建提示词
    char prompt[200];
    sprintf_s(prompt, sizeof(prompt),
        "当前传感器数据：温度 %d度，烟雾浓度 %d。请判断是否发生火灾？仅回答'是'或'否'，并给出简短理由。",
        temperature, smoke);

    // 2. 构建 JSON 请求体
    char json_data[512];
    sprintf_s(json_data, sizeof(json_data),
        "{\"model\": \"deepseek-chat\", \"messages\": [{\"role\": \"user\", \"content\": \"%s\"}], \"temperature\": 0.0}",
        prompt);

    // 3. 初始化 WinHTTP
    HINTERNET hSession = WinHttpOpen(L"C-FireMonitor/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        strcpy_s(result.message, sizeof(result.message), "WinHttpOpen 失败");
        return result;
    }

    // 4. 连接服务器 (注意：这里只写域名，不带 https://)
    HINTERNET hConnect = WinHttpConnect(hSession, L"api.deepseek.com", 443, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        strcpy_s(result.message, sizeof(result.message), "连接服务器失败 (Check Internet)");
        return result;
    }

    // 5. 打开请求
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/v1/chat/completions", NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);

    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        strcpy_s(result.message, sizeof(result.message), "打开请求失败");
        return result;
    }

    // 6. 添加请求头
    const wchar_t* headers = L"Content-Type: application/json\r\n";
    WinHttpAddRequestHeaders(hRequest, headers, -1, WINHTTP_ADDREQ_FLAG_ADD);

    // 添加 Authorization 头
    wchar_t authHeader[256];
    swprintf_s(authHeader, L"Authorization: Bearer %S", API_KEY);
    WinHttpAddRequestHeaders(hRequest, authHeader, -1, WINHTTP_ADDREQ_FLAG_ADD);

    // 7. 发送请求
    BOOL bRequestSent = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, (LPVOID)json_data, strlen(json_data), strlen(json_data), 0);

    if (bRequestSent) {
        // 8. 接收响应
        WinHttpReceiveResponse(hRequest, NULL);

        // 9. 读取数据
        DWORD dwSize = 0;
        DWORD dwDownloaded = 0;
        char* responseBuffer = (char*)malloc(8192); // 增大缓冲区
        ZeroMemory(responseBuffer, 8192);

        do {
            dwSize = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
            if (dwSize == 0) break;

            // 防止缓冲区溢出（简单处理）
            if (dwDownloaded + dwSize < 8190) {
                WinHttpReadData(hRequest, (LPVOID)(responseBuffer + dwDownloaded), dwSize, &dwDownloaded);
            } else {
                break;
            }
        } while (dwSize > 0);

        // --- 调试：打印原始响应 ---
        printf("[调试] 原始响应: %s\n", responseBuffer);

        // 10. 解析 JSON
        // 修正点：去掉了冒号后的空格，因为 DeepSeek 返回的是 "content":"xxx"
        char* contentStart = strstr(responseBuffer, "\"content\":\"");

        if (contentStart) {
            contentStart += 11; // 移动指针到内容的开始 ("content":" 的长度是 11)
            char* contentEnd = strstr(contentStart, "\""); // 查找结束引号

            if (contentEnd) {
                int len = contentEnd - contentStart;
                if (len > 500) len = 500;
                strncpy_s(result.message, sizeof(result.message), contentStart, len);

                // 判断是否火灾
                if (strstr(result.message, "是") || strstr(result.message, "火灾")) {
                    result.is_fire = 1;
                    result.confidence = 0.9;
                }
            }
        } else {
            // 如果解析失败，打印提示
            strcpy_s(result.message, sizeof(result.message), "解析失败 (Key错或网络错)");
        }

        free(responseBuffer);
    } else {
        strcpy_s(result.message, sizeof(result.message), "发送请求失败 (Error 5)");
    }

    // 11. 清理
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return result;
}

// ==========================================
// 主程序
// ==========================================

int readTemperature() { return rand() % 101; } // 模拟 0-100 度
int readSmokeLevel() { return rand() % 501; }  // 模拟 0-500 浓度

int main() {
    system("chcp 65001"); // 设置 UTF-8 编码
    srand(time(NULL));

    printf("=== 智能火灾监测系统 (DeepSeek版) ===\n");

    // 模拟传感器数据
    int temperature = readTemperature();
    int smoke = readSmokeLevel();

    printf("\n[传感器] 温度: %d°C, 烟雾: %d\n", temperature, smoke);
    printf("[系统] 正在询问 DeepSeek...\n");

    // 调用 AI
    AI_Result aiRes = CallDeepSeek_API(temperature, smoke);

    printf("\n--------------------------\n");
    printf("[AI回复] %s\n", aiRes.message);
    printf("--------------------------\n");

    if (strncmp(aiRes.message, "是", 3) == 0 || strncmp(aiRes.message, "发生火灾", 4) == 0) {
    printf(">>> 【警报】AI 确认发生火灾！\n");
} else {
    printf(">>> 【安全】AI 判断未发生火灾。\n");
}

    printf("\n按任意键退出...");
    getchar();
    return 0;
}
