// ==============================
// 【开机军火库 · 豆包定制 · 命门全开】
// ESP-IDF 纯标准 · 无Arduino · 永不炸机
// ==============================

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

// ==============================
// 硬件配置
// ==============================
#define KEY_PLAY GPIO_NUM_18
#define KEY_VOL_UP GPIO_NUM_5
#define KEY_VOL_DOWN GPIO_NUM_4
#define KEY_MUTE GPIO_NUM_22
#define KEY_PREV GPIO_NUM_16
#define KEY_NEXT GPIO_NUM_17
#define KEY_POWER GPIO_NUM_21

#define KEY_TRIG_LEVEL 0
#define DEBOUNCE_TIME_MS 50
#define LONG_PRESS_TIME_MS 2000

#define VOLUME_MIN 0
#define VOLUME_MAX 100
#define VOLUME_STEP 5

// ==============================
// 系统状态
// ==============================
typedef enum
{
    kSysStateInit,  // 【命门】系统初始状态 → 必须先走这里
    kSysStatePause, // 暂停
    kSysStatePlay   // 播放
} SysState;

// ==============================
// 按键状态机
// ==============================
typedef enum
{
    kKeyStateIDLE,     // 空闲
    kKeyStateDEBOUNCE, // 防抖
    kKeyStatePRESS,    // 短按
    kKeyStateLONGPRESS // 长按
} KeyState;

// ==============================
// 单个按键结构体
// ==============================
typedef struct
{
    KeyState state;      // 【命门】状态错 → 整个按键逻辑崩溃
    uint32_t time_stamp; // 【命门】计时基准 → 错了就乱触发
} KeyControl;

// ==============================
// 【超级命门 · 整个设备的灵魂】
// 所有状态、所有按键、所有功能都在这里
// ==============================
typedef struct AiBox
{
    SysState sys_state; // 系统总状态
    uint8_t vol;        // 当前音量
    uint8_t vol_backup; // 静音前音量备份
    uint16_t song_id;   // 当前歌曲序号
    bool power_en;      // 电源开关状态

    // 【命门】7个按键 → 少一个都不行
    KeyControl key_play;
    KeyControl key_vol_up;
    KeyControl key_vol_down;
    KeyControl key_mute;
    KeyControl key_prev;
    KeyControl key_next;
    KeyControl key_power;

    // 【命门】函数指针接口（模块化设计）
    void (*init)(struct AiBox *hdl);
    void (*process_play)(struct AiBox *hdl);
    void (*process_play_long)(struct AiBox *hdl);
    void (*process_vol_up)(struct AiBox *hdl);
    void (*process_vol_down)(struct AiBox *hdl);
    void (*process_mute)(struct AiBox *hdl);
    void (*process_prev)(struct AiBox *hdl);
    void (*process_next)(struct AiBox *hdl);
    void (*process_power)(struct AiBox *hdl);
} AiBox;

// ==============================
// 【全局三命门 · 项目必带三件套】
// 1. 设备实体
// 2. 初始化标志（防重复初始化）
// 3. 互斥锁（防并发炸机）
// ==============================
static AiBox ai_device;
static bool is_initialized = false; // 【命门】防重复初始化
static SemaphoreHandle_t dev_mutex; // 【命门】防多任务炸机

// ==============================
// 函数声明
// ==============================
static bool key_read(gpio_num_t pin);
static void dev_lock(void);
static void dev_unlock(void);
static esp_err_t ai_box_init(AiBox *hdl);
static void key_scan(AiBox *hdl);

static void process_play(AiBox *hdl) {}
static void process_play_long(AiBox *hdl) {}
static void process_vol_up(AiBox *hdl) {}
static void process_vol_down(AiBox *hdl) {}
static void process_mute(AiBox *hdl) {}
static void process_prev(AiBox *hdl) {}
static void process_next(AiBox *hdl) {}
static void process_power(AiBox *hdl) {}

// ==============================
// 按键读取（底层命门）
// ==============================
static bool key_read(gpio_num_t pin)
{
    return gpio_get_level(pin) == KEY_TRIG_LEVEL; // 【命门】电平错 → 按键不工作
}

// ==============================
// 锁操作（并发命门）
// ==============================
static void dev_lock(void)
{
    if (dev_mutex != NULL) // 【命门】锁不存在 → 绝不调用
        xSemaphoreTake(dev_mutex, portMAX_DELAY);
}

static void dev_unlock(void)
{
    if (dev_mutex != NULL)
        xSemaphoreGive(dev_mutex);
}

// ==============================
// 【超级命门 · 初始化函数】
// 这里错 → 系统直接炸机
// ==============================
static esp_err_t ai_box_init(AiBox *hdl)
{
    // 【命门1】空指针防御，防止程序崩溃
    if (hdl == NULL)
    {
        printf("[ERR] 设备句柄为空！\n");
        return ESP_ERR_INVALID_ARG;
    }

    // 【命门2】禁止重复初始化，避免资源冲突
    if (is_initialized)
    {
        printf("[WARN] 请勿重复初始化！\n");
        return ESP_ERR_INVALID_STATE;
    }

    // 【命门3】全局清零，杜绝脏数据导致玄学死机
    memset(hdl, 0, sizeof(AiBox));

    // 【命门4】调度器未启动 → 不获取时间（防卡死）
    uint32_t now = 0;
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
    {
        now = xTaskGetTickCount();
    }

    // 【命门5】初始化时间戳，防止上电误触发
    hdl->key_play.time_stamp = now;
    hdl->key_vol_up.time_stamp = now;
    hdl->key_vol_down.time_stamp = now;
    hdl->key_mute.time_stamp = now;
    hdl->key_prev.time_stamp = now;
    hdl->key_next.time_stamp = now;
    hdl->key_power.time_stamp = now;

    // GPIO配置：输入模式 + 上拉
    gpio_config_t gpio_conf = {0};
    gpio_conf.pin_bit_mask =
        (1ULL << KEY_PLAY) |
        (1ULL << KEY_VOL_UP) |
        (1ULL << KEY_VOL_DOWN) |
        (1ULL << KEY_MUTE) |
        (1ULL << KEY_PREV) |
        (1ULL << KEY_NEXT) |
        (1ULL << KEY_POWER);

    gpio_conf.mode = GPIO_MODE_INPUT;
    gpio_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_conf.intr_type = GPIO_INTR_DISABLE;

    esp_err_t ret = gpio_config(&gpio_conf);
    if (ret != ESP_OK)
    {
        printf("[ERR] GPIO配置失败！\n");
        return ret;
    }

    // 默认参数
    hdl->sys_state = kSysStatePause;
    hdl->vol = 5;
    hdl->vol_backup = 5;
    hdl->song_id = 1;
    hdl->power_en = false;

    // 【命门6】函数指针绑定 → 不绑就无法调用
    hdl->init = ai_box_init;
    hdl->process_play = process_play;
    hdl->process_play_long = process_play_long;
    hdl->process_vol_up = process_vol_up;
    hdl->process_vol_down = process_vol_down;
    hdl->process_mute = process_mute;
    hdl->process_prev = process_prev;
    hdl->process_next = process_next;
    hdl->process_power = process_power;

    // 【命门7】创建互斥锁，保证多线程安全
    dev_mutex = xSemaphoreCreateMutex();
    if (dev_mutex == NULL)
    {
        printf("[ERR] 互斥锁创建失败！\n");
        return ESP_ERR_NO_MEM;
    }

    is_initialized = true;
    printf("[INFO] 【军火库启动成功】系统已上膛！\n");
    return ESP_OK;
}

// ==============================
// 【命门核心 · 按键状态机】
// 逻辑错 → 乱跳、乱触发、死机
// ==============================
static void key_scan(AiBox *hdl)
{
    if (hdl == NULL)
        return;

    uint32_t now = xTaskGetTickCount();
    uint32_t debounce = pdMS_TO_TICKS(DEBOUNCE_TIME_MS);
    uint32_t long_press = pdMS_TO_TICKS(LONG_PRESS_TIME_MS);

    KeyControl *key = &hdl->key_play;
    bool pressed = key_read(KEY_PLAY);

    switch (key->state)
    {
    case kKeyStateIDLE:
        if (pressed)
        {
            key->time_stamp = now;
            key->state = kKeyStateDEBOUNCE;
        }
        break;

    case kKeyStateDEBOUNCE:
        if (pressed && (now - key->time_stamp >= debounce))
        {
            key->state = kKeyStatePRESS;
            key->time_stamp = now;

            dev_lock();
            hdl->process_play(hdl);
            dev_unlock();
        }
        else if (!pressed)
        {
            key->state = kKeyStateIDLE;
        }
        break;

    case kKeyStatePRESS:
        if (!pressed)
        {
            key->state = kKeyStateIDLE;
        }
        else if (now - key->time_stamp >= long_press)
        {
            key->state = kKeyStateLONGPRESS;

            dev_lock();
            hdl->process_play_long(hdl);
            dev_unlock();
        }
        break;

    case kKeyStateLONGPRESS:
        if (!pressed)
        {
            key->state = kKeyStateIDLE;
        }
        break;

    default:
        key->state = kKeyStateIDLE; // 【命门】异常复位 → 防卡死
        break;
    }
}

//========================= 播放按键 ==========================
static void process_play(AiBox *hdl)
{
    if (hdl == NULL)
        return; // 【命门】空指针防护，避免崩溃 防止野指针飞掉 嵌入式最经典、最必备的防御代码

    // 切换播放/暂停
    // 功能极简 这种代码想出BUG都很难
    // 稳准狠 量产级代码真相 能简单解决，绝不复杂化
    if (hdl->sys_state == kSysStatePlay)
    {
        hdl->sys_state = kSysStatePause;
        printf("[播放] Pause\n");
    }
    else
    {
        hdl->sys_state = kSysStatePlay;
        printf("[播放] Play\n");
    }
}

//========================= 音量+ ==========================
static void process_vol_up(AiBox *hdl)
{
    if (hdl == NULL)
        return; // 【命门】空指针防护

    // 音量增加，不超过最大值
    if (hdl->vol + VOLUME_STEP <= VOLUME_MAX)
    {
        hdl->vol += VOLUME_STEP;
    }
    else
    {
        hdl->vol = VOLUME_MAX;
    }
    printf("[音量+]%d\n", hdl->vol);
}

//========================= 音量- ==========================
static void process_vol_down(AiBox *hdl)
{
    if (hdl == NULL)
        return; // 【命门】空指针防护

    // 音量减少，不低于最小值
    if (hdl->vol >= VOLUME_STEP + VOLUME_MIN)
    {
        hdl->vol -= VOLUME_STEP;
    }
    else
    {
        hdl->vol = VOLUME_MIN;
    }
    printf("[音量-]%d\n", hdl->vol);
}

//========================= 上一曲 ==========================
static void process_prev(AiBox *hdl)
{
    if (hdl == NULL)
        return; // 【命门】空指针防护

    // 上一曲，不低于最小值
    if (hdl->song_id > 1)
    {
        hdl->song_id--; // 命门：先判断再减1，绝对不溢出
    }
    else
    {
        // 已经是第一首，保持第一首
        hdl->song_id = 1; // 命门：锁死在第一首，绝对不溢出
    }
    printf("[上一曲] %d\n", hdl->song_id);
}

//========================= 下一曲 ==========================
static void process_next(AiBox *hdl)
{
    if (hdl == NULL)
        return; // 【命门】空指针防护

    // 【非命门】下一曲无限自增
    //  溢出？这辈子都听不到65535首，不防？（不存在的！做就做到防患于未然！虽然这事挺看起来挺离谱的！）
    //  没必要！是程序员的傲慢！ 万一呢？是工程师的底线！
    if (hdl->song_id < UINT16_MAX)
    {
        hdl->song_id++;
    }
    else
    {
        hdl->song_id = UINT16_MAX; // 锁死在最大值，绝对不溢出
    }

    printf("[下一曲] %d\n", hdl->song_id);
}

//========================= 电源按键 ==========================
static void process_power(AiBox *hdl)
{
    if (hdl == NULL)
        return; // 【命门】空指针防护
    // 命门：长按有效动作
    // 短按：不执行任何操作
    // 长按 3秒：执行安全关机 / 重启
    printf("[电源] 长按触发 -> 系统安全关机\n");
}

//========================= 全局设备句柄 ==========================
AiBox g_aibox;

//========================= 系统入口 ==========================
void app_main(void)
{
    ai_box_init(&g_aibox);

    while (1)
    {
        key_scan(&g_aibox);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}