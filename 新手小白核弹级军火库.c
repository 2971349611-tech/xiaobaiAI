// ==============================================================
// 【项目名称】ESP32-WeaponCore | ESP32军火库内核
// 【项目定位】军工级嵌入式稳定框架 | 上电即跑 | 杜绝炸机
// 【作者信息】xiaobaiAI 小白
// 【GitHub仓库】https://github.com/2971349611-tech/xiaobaiAI.git
// 【版本号】V1.0.0
//
// 【版权与使用声明】
// 1. 本代码开源用于个人学习、技术研究、非商业用途。
// 2. 严禁将本框架用于商业售卖、付费课程、打包教程、割韭菜课程。
// 3. 严禁删除、篡改作者水印、版权信息、代码头部声明。
// 4. 二次开发、转载、分享必须注明原作者与原仓库地址。
// 5. 违反以上条款，将保留法律追责权利。
// ==============================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_err.h"

// ====================== 【作者防伪水印】 ======================
// 作用：固件编译后永久存在，用于识别作者，防止盗用
#define WEAPON_CORE_SIGNATURE "ESP32-WeaponCore_V1.0_Author:xiaobaiAI"
#define WEAPON_CORE_VERSION "1.0.0"
#define WEAPON_CORE_GITHUB_URL "https://github.com/2971349611-tech/xiaobaiAI.git"
#define WEAPON_CORE_COPYRIGHT "Copyright © 2026 xiaobaiAI All Rights Reserved."

// 深层隐藏水印（删不掉，反盗版终极手段）
const char *deep_sign = "Powered by xiaobaiAI | ESP32-WeaponCore | 禁止盗用";

// ====================== 【硬件引脚定义】 ======================
// 作用：根据实际硬件电路修改引脚号
#define KEY_PLAY GPIO_NUM_18
#define KEY_VOL_UP GPIO_NUM_5
#define KEY_VOL_DOWN GPIO_NUM_4
#define KEY_MUTE GPIO_NUM_22
#define KEY_PREV GPIO_NUM_16
#define KEY_NEXT GPIO_NUM_17
#define KEY_POWER GPIO_NUM_21

// ====================== 【按键参数配置】 ======================
#define KEY_TRIG_LEVEL 0        // 0=低电平触发（上拉输入）
#define DEBOUNCE_TIME_MS 50     // 按键防抖时间（消抖用）
#define LONG_PRESS_TIME_MS 2000 // 长按判定时间（2秒）

// ====================== 【音量参数】 ======================
#define VOLUME_MIN 0
#define VOLUME_MAX 100
#define VOLUME_STEP 5

// ====================== 【系统运行状态】 ======================
// 作用：管理设备整体工作模式，实现状态机
typedef enum
{
    SYS_STATE_INIT,  // 系统初始化状态
    SYS_STATE_PAUSE, // 暂停状态
    SYS_STATE_PLAY   // 正常播放状态
} SysState;

// ====================== 【按键状态机】 ======================
// 作用：实现按键防抖、短按、长按逻辑
typedef enum
{
    KEY_STATE_IDLE,        // 空闲：按键未按下
    KEY_STATE_DEBOUNCE,    // 防抖中：消除机械抖动
    KEY_STATE_SHORT_PRESS, // 短按触发
    KEY_STATE_LONG_PRESS   // 长按触发
} KeyState;

// ====================== 【单个按键结构体】 ======================
// 作用：保存每个按键的当前状态与计时，实现独立按键控制
typedef struct
{
    KeyState state; // 按键当前状态
    uint32_t tick;  // 按下起始时间戳
} Key;

// ====================== 【设备全局控制结构体】 ======================
// 作用：整个框架的核心，集中管理所有系统资源
typedef struct Device
{
    SysState sys_state;    // 系统总状态
    uint8_t volume;        // 当前音量
    uint8_t volume_backup; // 静音前音量备份
    uint16_t song_id;      // 当前歌曲序号
    bool power_on;         // 电源开关状态

    // 7个按键独立控制块
    Key key_play;
    Key key_vol_up;
    Key key_vol_down;
    Key key_mute;
    Key key_prev;
    Key key_next;
    Key key_power;

    // 函数指针接口（模块化设计）
    esp_err_t (*init)(struct Device *dev);
    void (*on_play)(struct Device *dev);
    void (*on_play_long)(struct Device *dev);
    void (*on_vol_up)(struct Device *dev);
    void (*on_vol_down)(struct Device *dev);
    void (*on_mute)(struct Device *dev);
    void (*on_prev)(struct Device *dev);
    void (*on_next)(struct Device *dev);
    void (*on_power)(struct Device *dev);
} Device;

// ====================== 【全局系统资源】 ======================
static Device g_dev;                     // 全局设备实例
static bool g_is_init = false;           // 初始化标志（防重复初始化）
static SemaphoreHandle_t g_mutex = NULL; // 互斥锁（多任务资源安全）

// ====================== 【按键电平读取】 ======================
// 作用：读取按键物理电平，屏蔽硬件差异
static bool key_read(gpio_num_t pin)
{
    return gpio_get_level(pin) == KEY_TRIG_LEVEL;
}

// ====================== 【互斥锁加锁】 ======================
// 作用：进入临界区，防止多任务同时修改全局变量
static void dev_lock(void)
{
    if (g_mutex != NULL)
        xSemaphoreTake(g_mutex, portMAX_DELAY);
}

// ====================== 【互斥锁解锁】 ======================
// 作用：退出临界区，释放资源
static void dev_unlock(void)
{
    if (g_mutex != NULL)
        xSemaphoreGive(g_mutex);
}

// ====================== 【按键回调函数（空实现）】 ======================
// 作用：用户可重写这些函数实现自己的业务逻辑
static void on_play(Device *dev) {}
static void on_play_long(Device *dev) {}
static void on_vol_up(Device *dev) {}
static void on_vol_down(Device *dev) {}
static void on_mute(Device *dev) {}
static void on_prev(Device *dev) {}
static void on_next(Device *dev) {}
static void on_power(Device *dev) {}

// ====================== 【设备初始化函数】 ======================
// 作用：系统上电后第一个必须调用的函数
// 包含：空指针防护、防重复初始化、结构体清零、GPIO配置、互斥锁创建
esp_err_t dev_init(Device *dev)
{
    // 【防护1】空指针防御，防止程序崩溃
    if (dev == NULL)
    {
        printf("[错误] 设备指针为空\n");
        return ESP_ERR_INVALID_ARG;
    }

    // 【防护2】禁止重复初始化，避免资源冲突
    if (g_is_init)
    {
        printf("[警告] 请勿重复初始化\n");
        return ESP_ERR_INVALID_STATE;
    }

    // 【防护3】全局清零，杜绝脏数据导致玄学死机
    memset(dev, 0, sizeof(Device));

    // 【防护4】初始化按键时间戳，防止上电误触发
    uint32_t now = 0;
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
    {
        now = xTaskGetTickCount();
    }
    dev->key_play.tick = now;
    dev->key_vol_up.tick = now;
    dev->key_vol_down.tick = now;
    dev->key_mute.tick = now;
    dev->key_prev.tick = now;
    dev->key_next.tick = now;
    dev->key_power.tick = now;

    // GPIO配置：输入模式 + 上拉
    gpio_config_t gpio_cfg = {0};
    gpio_cfg.pin_bit_mask = (1ULL << KEY_PLAY) | (1ULL << KEY_VOL_UP) |
                            (1ULL << KEY_VOL_DOWN) | (1ULL << KEY_MUTE) |
                            (1ULL << KEY_PREV) | (1ULL << KEY_NEXT) |
                            (1ULL << KEY_POWER);
    gpio_cfg.mode = GPIO_MODE_INPUT;
    gpio_cfg.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_cfg.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&gpio_cfg);

    // 默认参数
    dev->volume = 5;
    dev->volume_backup = 5;
    dev->song_id = 1;
    dev->power_on = true;

    // 绑定回调函数
    dev->init = dev_init;
    dev->on_play = on_play;
    dev->on_play_long = on_play_long;
    dev->on_vol_up = on_vol_up;
    dev->on_vol_down = on_vol_down;
    dev->on_mute = on_mute;
    dev->on_prev = on_prev;
    dev->on_next = on_next;
    dev->on_power = on_power;

    // 【防护5】创建互斥锁，保证多线程安全
    g_mutex = xSemaphoreCreateMutex();
    if (g_mutex == NULL)
    {
        printf("[错误] 互斥锁创建失败\n");
        return ESP_ERR_NO_MEM;
    }

    g_is_init = true;

    // ====================== 开机打印作者水印 ======================
    printf("\n===============================================\n");
    printf("  ESP32-WeaponCore 军火库内核 启动成功\n");
    printf("  版本: %s\n", WEAPON_CORE_VERSION);
    printf("  作者: xiaobaiAI 小白\n");
    printf("  仓库: %s\n", WEAPON_CORE_GITHUB_URL);
    printf("  %s\n", WEAPON_CORE_COPYRIGHT);
    printf("  禁止商用 | 禁止盗用 | 侵权必究\n");
    printf("===============================================\n\n");

    return ESP_OK;
}

// ====================== 【播放按键状态机】 ======================
// 作用：实现按键防抖、短按、长按
void key_scan_play(Device *dev)
{
    if (dev == NULL)
        return;

    uint32_t now = xTaskGetTickCount();
    uint32_t debounce = pdMS_TO_TICKS(DEBOUNCE_TIME_MS);
    uint32_t long_press = pdMS_TO_TICKS(LONG_PRESS_TIME_MS);
    Key *key = &dev->key_play;
    bool pressed = key_read(KEY_PLAY);

    switch (key->state)
    {
    case KEY_STATE_IDLE:
        if (pressed)
        {
            key->tick = now;
            key->state = KEY_STATE_DEBOUNCE;
        }
        break;

    case KEY_STATE_DEBOUNCE:
        if (pressed && (now - key->tick >= debounce))
        {
            key->state = KEY_STATE_SHORT_PRESS;
            dev_lock();
            dev->on_play(dev);
            dev_unlock();
        }
        else if (!pressed)
        {
            key->state = KEY_STATE_IDLE;
        }
        break;

    case KEY_STATE_SHORT_PRESS:
        if (pressed && (now - key->tick >= long_press))
        {
            key->state = KEY_STATE_LONG_PRESS;
            dev_lock();
            dev->on_play_long(dev);
            dev_unlock();
        }
        else if (!pressed)
        {
            key->state = KEY_STATE_IDLE;
        }
        break;

    case KEY_STATE_LONG_PRESS:
        if (!pressed)
        {
            key->state = KEY_STATE_IDLE;
        }
        break;

    default:
        key->state = KEY_STATE_IDLE;
        break;
    }
}

// ====================== 【主函数】 ======================
// 作用：程序入口，初始化 + 主循环
void app_main(void)
{
    dev_init(&g_dev);

    while (1)
    {
        key_scan_play(&g_dev);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}