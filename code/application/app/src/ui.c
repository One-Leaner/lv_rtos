#include "ui.h"
#include "debug.h"
#include "touch.h"
#include "lcd.h"
#include "buzzer.h"
#include "spi.h"
#include "fs.h"
#include "heap.h"
#include "lv_port_fs.h"
#include "encoder.h"
#include "my_nes_port.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include <string.h>

// LV_FONT_DECLARE(chinese_16_2) // 导入字体

#define MY_DISP_HOR_RES LCD_H
#define MY_DISP_VER_RES LCD_W
#define BYTE_PER_PIXEL (LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565)) /*will be 2 for RGB565 */

#define USER_DATA_CREATE_STATIC(name, idx) \
    static user_data_t name = {            \
        .id = idx,                         \
        .next_scr = NULL,                  \
        .other_data = NULL,                \
    }

struct UI_CTRL
{
    volatile uint8_t flush_flag;
    volatile uint8_t usb_flag;
    volatile uint8_t iap_flag;
    volatile uint8_t ota_flag;

    void (*disp_init)(UI_CTRL_t *);
    void (*indev_init)(UI_CTRL_t *);
    void (*draw)(UI_CTRL_t *);
    void (*nes_draw)(UI_CTRL_t *);
};

static lv_font_t *chinese_16_2 = NULL;    // 字体
static lv_font_t *chinese_12_2 = NULL;    // 字体
static lv_obj_t *g_menu_scr = NULL;       // 菜单界面
static lv_obj_t *g_usb_scr = NULL;        // USB界面
static lv_obj_t *g_nes_scr = NULL;        // NES界面
static lv_obj_t *g_nes_parent_scr = NULL; // 启动NES时的父界面（游戏目录）
static uint8_t g_volume_flag = 0;
static uint32_t g_volume_delay = 0;
lv_obj_t *g_nes_canvas = NULL;
// nes_t *g_nes = NULL;
// static uint8_t crr_pad1 = 0; // 当前按键1状态

static void ui_file_list_create(FS_TREE_t *tree, lv_obj_t *parent_file_list_scr, lv_obj_t *parent_file_list, FS_NODE_t *parent);

static void disp_flush_callback(lv_display_t *disp_drv, const lv_area_t *area, uint8_t *px_map)
{
    LV_UNUSED(disp_drv);

    if (ui.api->get_flush(ui.ctrl))
        return;

    LCD_AREA_t area_temp = {area->x1, area->x2, area->y1, area->y2};
    int size = lv_area_get_height(area) * lv_area_get_width(area);

    // 8位发送时需要交换高低字节位置
    // lv_draw_sw_rgb565_swap(px_map, size); // 注意：要高低字节位置!!!
    // size *= 2;
    // printf("size = %d\n", size);

    lcd.api->set_datawidth(lcd.ctrl, SPI_DATAWIDTH_8BIT);
    lcd.api->set_windows(lcd.ctrl, area_temp);
    lcd.api->set_datawidth(lcd.ctrl, SPI_DATAWIDTH_16BIT);

    uint32_t offset = 0;
    uint16_t chunk;

    while (size > 0)
    {
        chunk = (size > 0xffff) ? 0xffff : size;
        lcd.api->write(lcd.ctrl, px_map + offset * 2, chunk, LCD_DATA, 1);
        size -= chunk;
        offset += chunk;
    }

    // lcd.api->write(lcd.ctrl, px_map, size * 2, LCD_DATA, 0); // 非阻塞

    ui.api->set_flush(ui.ctrl, 1); // 需要刷新

    /*IMPORTANT!!!
     *Inform the graphics library that you are ready with the flushing*/
    // lv_display_flush_ready(disp_drv);// 在回调函数中调用
}

static void indev_read_callback(lv_indev_t *indev, lv_indev_data_t *data)
{
    LV_UNUSED(indev);

    uint16_t x, y;

    if (touch.api->get_xy(touch.ctrl, &x, &y))
    {
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
    }
    else
    {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void ui_disp_init(UI_CTRL_t *ctrl)
{
    LV_UNUSED(ctrl);

    lv_init();
    uint16_t hor_res = lcd.api->get_width(lcd.ctrl);
    uint16_t ver_res = lcd.api->get_height(lcd.ctrl);

    lv_display_t *disp = lv_display_create(hor_res, ver_res);
    lv_display_set_flush_cb(disp, disp_flush_callback);

    lv_draw_buf_t *buf1 = lv_draw_buf_create(hor_res, ver_res, LV_COLOR_FORMAT_NATIVE, LV_STRIDE_AUTO);
    lv_draw_buf_t *buf2 = lv_draw_buf_create(hor_res, ver_res, LV_COLOR_FORMAT_NATIVE, LV_STRIDE_AUTO);
    // lv_draw_buf_t *buf3 = lv_draw_buf_create(hor_res, ver_res, LV_COLOR_FORMAT_NATIVE, LV_STRIDE_AUTO);
    lv_display_set_render_mode(disp, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_draw_buffers(disp, buf1, buf2);
    // lv_display_set_3rd_draw_buffer(disp, buf3);
    // LV_DISPLAY_RENDER_MODE_DIRECT这个渲染模式有问题
}

static void ui_indev_init(UI_CTRL_t *ctrl)
{
    LV_UNUSED(ctrl);

    printf("indev begin\n");
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, indev_read_callback);
    printf("indev ok\n");
}

static void ui_timer_callback(lv_timer_t *timer)
{
    lv_obj_t **clock_label = lv_timer_get_user_data(timer);
    static uint32_t count = 0;
    static uint8_t hour = 0;
    static uint8_t minute = 0;
    static uint8_t second = 0;

    count++;

    if (g_volume_flag)
    {
        g_volume_delay++;
        if (g_volume_delay >= 20)
        {
            buzzer.api->off(buzzer.ctrl);
        }
    }

    if (count >= 100)
    {
        count = 0;
        second++;
    }

    if (second >= 60)
    {
        second = 0;
        minute++;
    }
    if (minute >= 60)
    {
        minute = 0;
        hour++;
    }
    // printf("%02d:%02d:%02d\n", hour, minute, second);

    lv_label_set_text_fmt(clock_label[0], "%02d", hour);
    lv_label_set_text_fmt(clock_label[1], "%02d", minute);
    lv_label_set_text_fmt(clock_label[2], "%02d", second);
}

static void ui_callback(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    user_data_t *user = lv_event_get_user_data(e);
    lv_obj_t *obj = lv_event_get_target(e);

    static FS_TREE_t *tree = NULL;

    if (code == LV_EVENT_SHORT_CLICKED && g_volume_flag)
    {
        buzzer.api->on(buzzer.ctrl);
        g_volume_delay = 0;
    }

    switch (user->id)
    {
    case UI_ID_MENU:
        // 主界面加载
        if (code == LV_EVENT_SHORT_CLICKED)
        {
            lv_screen_load_anim(user->next_scr, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, false);
        }
        break;

    case UI_ID_RETURN: // 返回界面加载
        if (code == LV_EVENT_GESTURE)
        {
            lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
            if (dir == LV_DIR_LEFT)
            {
                lv_obj_t *target_scr = user->next_scr;

                if (nes.api->get_state(nes.ctrl) == NES_STATE_RUNNING)
                {
                    nes.api->stop(nes.ctrl);
                    lv_obj_clean(g_nes_canvas);
                    target_scr = g_nes_parent_scr;
                }

                lv_screen_load_anim(target_scr, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, false);
            }

            if (tree != NULL && g_menu_scr == user->next_scr) // 返回主界面时销毁树
            {
                fs_tree_destroy(tree);
                tree = NULL;
            }
        }
        break;

    case UI_ID_SETTING: // 设置界面加载
        if (code == LV_EVENT_SHORT_CLICKED)
            lv_screen_load_anim(user->next_scr, LV_SCR_LOAD_ANIM_FADE_IN, 300, 0, false);
        break;

    case UI_ID_FILE:
        if (tree == NULL)
        {
            lv_obj_clean(user->other_data);
            tree = fs_tree_create("0:");
            fs_tree_scan(tree);
            lv_obj_set_style_text_font(user->other_data, chinese_12_2, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_list_add_text(user->other_data, fs_node_get_full_path(fs_tree_get_root_node(tree)));
            ui_file_list_create(tree, user->next_scr, user->other_data, fs_tree_get_root_node(tree));
        }
        lv_screen_load_anim(user->next_scr, LV_SCR_LOAD_ANIM_FADE_IN, 0, 0, false);
        break;

    case UI_ID_BRIGHTNESS: // 亮度设置滑动事件
        if (code == LV_EVENT_VALUE_CHANGED)
        {
            uint16_t brightness = lv_slider_get_value(obj);
            lcd.api->set_light(lcd.ctrl, brightness);
        }
        break;

    case UI_ID_VOLUME: // 音量设置滑动事件
        if (code == LV_EVENT_VALUE_CHANGED)
        {
            uint16_t volume = lv_slider_get_value(obj);
            buzzer.api->set_volume(buzzer.ctrl, volume);
        }
        break;

    case UI_ID_SOUND: // 点击音效设置事件
        if (code == LV_EVENT_SHORT_CLICKED)
        {
            g_volume_flag = !g_volume_flag;
            if (!g_volume_flag)
            {
                buzzer.api->off(buzzer.ctrl);
            }
        }
        break;

    case UI_ID_USB:
        if (code == LV_EVENT_SHORT_CLICKED)
        {
            lv_screen_load_anim(user->next_scr, LV_SCR_LOAD_ANIM_FADE_IN, 0, 0, false);
            ui.api->set_usb_flag(ui.ctrl, 1);
        }
        break;

    case UI_ID_IAP:
        if (code == LV_EVENT_SHORT_CLICKED)
        {
            lv_screen_load_anim(user->next_scr, LV_SCR_LOAD_ANIM_FADE_IN, 0, 0, false);
            ui.api->set_iap_flag(ui.ctrl, 1);
        }
        break;

    case UI_ID_OTA:
        if (code == LV_EVENT_SHORT_CLICKED)
        {
            lv_screen_load_anim(user->next_scr, LV_SCR_LOAD_ANIM_FADE_IN, 0, 0, false);
            ui.api->set_ota_flag(ui.ctrl, 1);
        }
        break;

    case UI_ID_NES_RUN:
        if (code == LV_EVENT_SHORT_CLICKED)
        {
            g_nes_parent_scr = lv_scr_act(); // 记录启动NES前的当前界面（游戏目录）
            lv_screen_load_anim(user->next_scr, LV_SCR_LOAD_ANIM_FADE_IN, 0, 0, false);
            nes.api->start(nes.ctrl, user->other_data);
            // nes_load_file(g_nes, user->other_data);
            // 释放信号量
            // xSemaphoreGive(g_nes_sem);
        }
        break;

    default:
        break;
    }
}

static void ui_file_list_create(FS_TREE_t *tree, lv_obj_t *parent_file_list_scr, lv_obj_t *parent_file_list, FS_NODE_t *parent)
{
    FS_NODE_t **children = fs_node_get_children(parent);
    uint32_t child_count = fs_node_get_child_count(parent);

    for (uint32_t i = 0; i < child_count; i++)
    {
        FS_NODE_TYPE_t type = fs_node_get_type(children[i]);
        user_data_t *user = heap.api->malloc(heap.ctrl, sizeof(user_data_t));
        if (type == FS_NODE_FILE)
        {
            char *name = fs_node_get_name(children[i]);
            uint16_t name_len = strlen(name);
            lv_obj_t *btn = lv_list_add_button(parent_file_list, LV_SYMBOL_FILE, name);
            //.nes结尾的文件运行nes游戏
            if (strncmp(name + name_len - 4, ".nes", 4) == 0 ||
                strncmp(name + name_len - 4, ".NES", 4) == 0)
            {
                user->id = UI_ID_NES_RUN;
                user->next_scr = g_nes_scr;
                user->other_data = fs_node_get_full_path(children[i]);
                lv_obj_add_event_cb(btn, ui_callback, LV_EVENT_SHORT_CLICKED, user);
            }
        }
        else
        {
            lv_obj_t *btn = lv_list_add_button(parent_file_list, LV_SYMBOL_DIRECTORY, fs_node_get_name(children[i]));
            user->id = UI_ID_FILE;
            user->next_scr = lv_list_create(NULL);
            lv_obj_t *file_list = lv_list_create(user->next_scr);
            lv_obj_set_size(file_list, 240, 320);
            lv_obj_add_event_cb(btn, ui_callback, LV_EVENT_SHORT_CLICKED, user);

            // 左滑返回上一级
            user_data_t *gesture_user = heap.api->malloc(heap.ctrl, sizeof(user_data_t));
            gesture_user->id = UI_ID_RETURN;
            gesture_user->next_scr = parent_file_list_scr;
            gesture_user->other_data = NULL;
            lv_obj_add_event_cb(user->next_scr, ui_callback, LV_EVENT_GESTURE, gesture_user);

            lv_obj_set_style_text_font(file_list, chinese_12_2, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_list_add_text(file_list, fs_node_get_full_path(children[i]));
            ui_file_list_create(tree, user->next_scr, file_list, children[i]);
        }
    }
}

static void ui_draw(UI_CTRL_t *ctrl)
{
    LV_UNUSED(ctrl);

    // 文件系统初始化
    lv_port_fs_init();

    // 转换为 GBK 字符串
    static char font_path[256];
    encoder_utf8_to_gbk("S:/font/思源宋体.ttf", font_path, sizeof(font_path));
    // 配置 FreeType 字体信息
    chinese_12_2 = lv_freetype_font_create(
        font_path, // 字体文件路径
        LV_FREETYPE_FONT_RENDER_MODE_BITMAP,
        12,
        LV_FREETYPE_FONT_STYLE_ITALIC);

    if (!chinese_12_2)
    {
        printf("chinese_12_2 create failed\n");
        return;
    }

    chinese_16_2 = lv_freetype_font_create(
        font_path, // 字体文件路径
        LV_FREETYPE_FONT_RENDER_MODE_BITMAP,
        16,
        LV_FREETYPE_FONT_STYLE_ITALIC);
    if (!chinese_16_2)
    {
        printf("chinese_16_2 create failed\n");
        return;
    }

    // chinese_12_2 = lv_binfont_create("S:/font/chinese_12_2.bin");
    // chinese_16_2 = lv_binfont_create("S:/font/chinese_16_2.bin");

    //////////////////// 欢迎界面 BEGIN ////////////////////
    lv_obj_t *welcome_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_grad_color(welcome_scr, lv_color_hex(0x31acdd), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(welcome_scr, LV_GRAD_DIR_VER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_t *welcome_btn = lv_btn_create(welcome_scr);
    lv_obj_set_scroll_dir(welcome_btn, LV_DIR_NONE);
    lv_obj_set_align(welcome_btn, LV_ALIGN_CENTER);
    lv_obj_set_size(welcome_btn, 100, 50);
    // 设置样式
    static lv_style_t style0;
    lv_style_init(&style0);
    lv_style_set_radius(&style0, 10); // 圆角半径
    lv_style_set_arc_opa(&style0, LV_OPA_COVER);
    lv_style_set_bg_grad_color(&style0, lv_color_hex(0xff00b3));
    lv_style_set_bg_grad_dir(&style0, LV_GRAD_DIR_VER);
    static lv_style_t style1;
    lv_style_init(&style1);
    lv_style_set_shadow_color(&style1, lv_color_hex(0x3626e2)); // 阴影颜色
    lv_style_set_shadow_width(&style1, 35);
    lv_obj_add_style(welcome_btn, &style0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_style(welcome_btn, &style1, LV_PART_MAIN | LV_STATE_PRESSED);
    // 标签设置
    lv_obj_t *welcome_label = lv_label_create(welcome_btn);
    lv_label_set_text(welcome_label, "#2ec9e9 欢迎使用LVGL");
    lv_label_set_recolor(welcome_label, 1);
    lv_obj_set_width(welcome_label, 100);
    lv_obj_set_style_text_font(welcome_label, chinese_16_2, LV_PART_MAIN | LV_STATE_DEFAULT);
    // lv_obj_set_align(welcome_label, LV_ALIGN_CENTER);
    lv_obj_set_style_align(welcome_label, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_long_mode(welcome_label, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
    // 加载欢迎界面
    lv_scr_load(welcome_scr);
    //////////////////// 欢迎界面 END ////////////////////

    ///////////////主界面BEGIN////////////////////////
    g_menu_scr = lv_obj_create(NULL);
    // 主界面背景
    lv_obj_set_style_bg_grad_color(g_menu_scr, lv_color_hex(0xdd8a31), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(g_menu_scr, LV_GRAD_DIR_VER, LV_PART_MAIN | LV_STATE_DEFAULT);
    // 创建时钟
    lv_obj_t *clock = lv_obj_create(g_menu_scr);
    lv_obj_set_scroll_dir(clock, LV_DIR_NONE);
    lv_obj_set_align(clock, LV_ALIGN_TOP_LEFT);
    lv_obj_set_size(clock, 60, 150);
    lv_obj_set_style_bg_grad_color(clock, lv_color_hex(0x3a3935), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(clock, LV_GRAD_DIR_VER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(clock, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    // 时标签
    lv_obj_t *clock_hour_label = lv_label_create(clock);
    lv_obj_align(clock_hour_label, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_text_align(clock_hour_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(clock_hour_label, chinese_16_2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(clock_hour_label, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    // 分标签
    lv_obj_t *clock_minute_label = lv_label_create(clock);
    lv_obj_align(clock_minute_label, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_text_align(clock_minute_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(clock_minute_label, chinese_16_2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(clock_minute_label, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    // 秒标签
    lv_obj_t *clock_second_label = lv_label_create(clock);
    // lv_obj_set_align(clock_second_label, LV_ALIGN_BOTTOM_LEFT);
    lv_obj_align(clock_second_label, LV_ALIGN_BOTTOM_MID, 0, -40);
    lv_obj_set_style_text_align(clock_second_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(clock_second_label, chinese_16_2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(clock_second_label, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    // 创建设置图标 #5a46dc
    lv_obj_t *setting_btn = lv_btn_create(g_menu_scr);
    lv_obj_set_style_radius(setting_btn, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(setting_btn, LV_ALIGN_BOTTOM_MID, 0, -40);
    lv_obj_set_size(setting_btn, 50, 50);
    lv_obj_set_style_bg_grad_color(setting_btn, lv_color_hex(0xd8e488), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(setting_btn, LV_GRAD_DIR_VER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_t *setting_label = lv_label_create(setting_btn);
    lv_obj_set_style_text_font(setting_label, chinese_16_2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(setting_label, "设置");
    // lv_obj_set_style_text_align(setting_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_align(setting_label, LV_ALIGN_CENTER);
    // 创建文件图标 #50e052
    lv_obj_t *file_list_btn = lv_btn_create(g_menu_scr);
    lv_obj_set_style_radius(file_list_btn, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(file_list_btn, LV_ALIGN_BOTTOM_LEFT, 20, -40);
    lv_obj_set_size(file_list_btn, 50, 50);
    lv_obj_set_style_bg_grad_color(file_list_btn, lv_color_hex(0x50e052), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(file_list_btn, LV_GRAD_DIR_VER, LV_PART_MAIN | LV_STATE_DEFAULT);
    // 文件图标标签
    lv_obj_t *file_label = lv_label_create(file_list_btn);
    lv_obj_set_style_text_font(file_label, chinese_16_2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(file_label, "文件");
    // lv_obj_set_style_text_align(file_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_align(file_label, LV_ALIGN_CENTER);
    lv_obj_set_style_text_color(file_label, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    // USB图标
    lv_obj_t *usb_btn = lv_btn_create(g_menu_scr);
    lv_obj_set_style_radius(usb_btn, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(usb_btn, LV_ALIGN_BOTTOM_RIGHT, -20, -40);
    lv_obj_set_size(usb_btn, 50, 50);
    lv_obj_set_style_bg_grad_color(usb_btn, lv_color_hex(0x5a46dc), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(usb_btn, LV_GRAD_DIR_VER, LV_PART_MAIN | LV_STATE_DEFAULT);
    // USB图标标签
    lv_obj_t *usb_label = lv_label_create(usb_btn);
    lv_obj_set_style_text_font(usb_label, chinese_16_2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(usb_label, "USB");
    lv_obj_set_align(usb_label, LV_ALIGN_CENTER);
    lv_obj_set_style_text_color(usb_label, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    // IAP图标
    lv_obj_t *iap_btn = lv_btn_create(g_menu_scr);
    lv_obj_set_style_radius(iap_btn, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(iap_btn, LV_ALIGN_RIGHT_MID, -20, 0);
    lv_obj_set_size(iap_btn, 50, 50);
    lv_obj_set_style_bg_grad_color(iap_btn, lv_color_hex(0x5a46dc), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(iap_btn, LV_GRAD_DIR_VER, LV_PART_MAIN | LV_STATE_DEFAULT);
    // IAP图标标签
    lv_obj_t *iap_label = lv_label_create(iap_btn);
    lv_obj_set_style_text_font(iap_label, chinese_16_2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(iap_label, "IAP");
    lv_obj_set_align(iap_label, LV_ALIGN_CENTER);
    lv_obj_set_style_text_color(iap_label, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    // OTA图标
    lv_obj_t *ota_btn = lv_btn_create(g_menu_scr);
    lv_obj_set_style_radius(ota_btn, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(ota_btn, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_size(ota_btn, 50, 50);
    lv_obj_set_style_bg_grad_color(ota_btn, lv_color_hex(0x5a46dc), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ota_btn, LV_GRAD_DIR_VER, LV_PART_MAIN | LV_STATE_DEFAULT);
    // OTA图标标签
    lv_obj_t *ota_label = lv_label_create(ota_btn);
    lv_obj_set_style_text_font(ota_label, chinese_16_2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(ota_label, "OTA");
    lv_obj_set_align(ota_label, LV_ALIGN_CENTER);
    lv_obj_set_style_text_color(ota_label, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    ///////////////主界面END////////////////////////

    ////////////////////////// 设置页面BEGIN//////////////////////////
    lv_obj_t *setting_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_grad_dir(setting_scr, LV_GRAD_DIR_VER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(setting_scr, lv_color_hex(0x00ff5e), LV_PART_MAIN | LV_STATE_DEFAULT);
    // 亮度设置 #d5235f
    lv_obj_t *brightness_slider = lv_slider_create(setting_scr);
    // lv_obj_add_flag(brightness_slider, LV_OBJ_FLAG_ADV_HITTEST);// 禁用点击
    lv_obj_set_style_bg_grad_color(brightness_slider, lv_color_hex(0xe06550), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(brightness_slider, LV_GRAD_DIR_VER, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_remove_style(brightness_slider, NULL, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_size(brightness_slider, 50, 150);
    lv_slider_set_mode(brightness_slider, LV_SLIDER_MODE_NORMAL);
    lv_slider_set_range(brightness_slider, 20, 100); // 屏幕不能全黑
    lv_slider_set_value(brightness_slider, 50, LV_ANIM_ON);
    lv_obj_align(brightness_slider, LV_ALIGN_TOP_RIGHT, -20, 10);
    lv_slider_set_orientation(brightness_slider, LV_SLIDER_ORIENTATION_VERTICAL);
    // 亮度标签
    lv_obj_t *brightness_label = lv_label_create(setting_scr);
    lv_obj_set_style_text_font(brightness_label, chinese_16_2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(brightness_label, "☀");
    lv_obj_align_to(brightness_label, brightness_slider, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_text_color(brightness_label, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    // 音量设置
    lv_obj_t *volume_slider = lv_slider_create(setting_scr);
    lv_obj_set_style_bg_grad_color(volume_slider, lv_color_hex(0x5ce050), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(volume_slider, LV_GRAD_DIR_VER, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_remove_style(volume_slider, NULL, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_size(volume_slider, 150, 50);
    lv_slider_set_mode(volume_slider, LV_SLIDER_MODE_NORMAL);
    lv_slider_set_range(volume_slider, 0, 100);
    lv_slider_set_value(volume_slider, 50, LV_ANIM_ON);
    lv_obj_align_to(volume_slider, brightness_slider, LV_ALIGN_TOP_RIGHT, -60, 0);
    lv_slider_set_orientation(volume_slider, LV_SLIDER_ORIENTATION_HORIZONTAL);
    // 音量标签
    lv_obj_t *volume_label = lv_label_create(setting_scr);
    lv_obj_set_style_text_font(volume_label, chinese_16_2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(volume_label, "♫");
    lv_obj_align_to(volume_label, volume_slider, LV_ALIGN_LEFT_MID, 20, 0);
    lv_obj_set_style_text_color(volume_label, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    // 点击音效设置 #be20ea
    lv_obj_t *click_sound_btn = lv_btn_create(setting_scr);
    lv_obj_align_to(click_sound_btn, volume_slider, LV_ALIGN_BOTTOM_LEFT, 0, 20);
    lv_obj_set_size(click_sound_btn, 100, 50);
    lv_obj_add_flag(click_sound_btn, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_set_style_bg_grad_color(click_sound_btn, lv_color_hex(0xbe20ea), LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_bg_grad_dir(click_sound_btn, LV_GRAD_DIR_VER, LV_PART_MAIN | LV_STATE_CHECKED);
    // 点击音效标签
    lv_obj_t *click_sound_label = lv_label_create(click_sound_btn);
    lv_obj_set_style_text_font(click_sound_label, chinese_16_2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_align(click_sound_label, LV_ALIGN_CENTER);
    lv_label_set_text(click_sound_label, "点击音效");
    lv_obj_set_style_text_color(click_sound_label, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    ////////////////////////// 设置页面END//////////////////////////

    ////////////////////////// 文件列表页面BEGIN//////////////////////////
    lv_obj_t *file_list_scr = lv_obj_create(NULL);       // 画布
    lv_obj_t *file_list = lv_list_create(file_list_scr); // 列表
    lv_obj_set_size(file_list, 240, 320);                // 列表大小
                                                         ////////////////////////// 文件列表页面END//////////////////////////

    ////////////////////////// USB页面BEGIN//////////////////////////
    // USB页面
    g_usb_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_grad_dir(g_usb_scr, LV_GRAD_DIR_VER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(g_usb_scr, lv_color_hex(0x5a46dc), LV_PART_MAIN | LV_STATE_DEFAULT);
    // 标签
    lv_obj_t *usb_scr_label = lv_label_create(g_usb_scr);
    lv_obj_set_style_text_font(usb_scr_label, chinese_16_2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(usb_scr_label, "拔出USB退出该界面");
    lv_obj_align_to(usb_scr_label, g_usb_scr, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(usb_scr_label, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    ////////////////////////// USB页面END//////////////////////////

    ////////////////////////// 更新页面BEGIN//////////////////////////
    lv_obj_t *update_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_grad_dir(update_scr, LV_GRAD_DIR_VER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(update_scr, lv_color_hex(0x5a46dc), LV_PART_MAIN | LV_STATE_DEFAULT);
    // 标签
    lv_obj_t *update_scr_label = lv_label_create(update_scr);
    lv_obj_set_style_text_font(update_scr_label, chinese_16_2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(update_scr_label, "正在下载更新资源包...");
    lv_obj_align_to(update_scr_label, update_scr, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(update_scr_label, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    ////////////////////////// 更新页面END//////////////////////////

    //////////////// 定时器 BEGIN////////////////
    static lv_obj_t *clock_label_arry[3];
    clock_label_arry[0] = clock_hour_label;
    clock_label_arry[1] = clock_minute_label;
    clock_label_arry[2] = clock_second_label;
    lv_timer_create(ui_timer_callback, 10, clock_label_arry);
    //////////////// 定时器 END////////////////

    ////////////////////////// 添加事件BEGIN//////////////////////////
    // 进入主界面
    USER_DATA_CREATE_STATIC(welcome_btn_user, UI_ID_MENU);
    welcome_btn_user.next_scr = g_menu_scr;
    lv_obj_add_event_cb(welcome_btn, ui_callback, LV_EVENT_SHORT_CLICKED, &welcome_btn_user);
    // 点击进入设置界面
    USER_DATA_CREATE_STATIC(setting_btn_user, UI_ID_SETTING);
    setting_btn_user.next_scr = setting_scr;
    lv_obj_add_event_cb(setting_btn, ui_callback, LV_EVENT_SHORT_CLICKED, &setting_btn_user);
    // 左滑返回主界面
    USER_DATA_CREATE_STATIC(setting_scr_gesture_user, UI_ID_RETURN);
    setting_scr_gesture_user.next_scr = g_menu_scr;
    lv_obj_add_event_cb(setting_scr, ui_callback, LV_EVENT_GESTURE, &setting_scr_gesture_user);
    // 点击进入文件列表
    USER_DATA_CREATE_STATIC(file_list_btn_user, UI_ID_FILE);
    file_list_btn_user.next_scr = file_list_scr;
    file_list_btn_user.other_data = file_list;
    lv_obj_add_event_cb(file_list_btn, ui_callback, LV_EVENT_SHORT_CLICKED, &file_list_btn_user);
    // 左滑返回主界面
    USER_DATA_CREATE_STATIC(file_list_gesture_user, UI_ID_RETURN);
    file_list_gesture_user.next_scr = g_menu_scr;
    lv_obj_add_event_cb(file_list_scr, ui_callback, LV_EVENT_GESTURE, &file_list_gesture_user);
    // 亮度设置滑动事件
    USER_DATA_CREATE_STATIC(brightness_slider_user, UI_ID_BRIGHTNESS);
    lv_obj_add_event_cb(brightness_slider, ui_callback, LV_EVENT_VALUE_CHANGED, &brightness_slider_user);
    // 音量设置滑动事件
    USER_DATA_CREATE_STATIC(volume_slider_user, UI_ID_VOLUME);
    lv_obj_add_event_cb(volume_slider, ui_callback, LV_EVENT_VALUE_CHANGED, &volume_slider_user);
    // 点击音效设置事件
    USER_DATA_CREATE_STATIC(click_sound_btn_user, UI_ID_SOUND);
    lv_obj_add_event_cb(click_sound_btn, ui_callback, LV_EVENT_SHORT_CLICKED, &click_sound_btn_user);
    // 点击USB图标事件
    USER_DATA_CREATE_STATIC(usb_btn_user, UI_ID_USB);
    usb_btn_user.next_scr = g_usb_scr;
    lv_obj_add_event_cb(usb_btn, ui_callback, LV_EVENT_SHORT_CLICKED, &usb_btn_user);
    // 点击IAP图标事件
    USER_DATA_CREATE_STATIC(iap_btn_user, UI_ID_IAP);
    iap_btn_user.next_scr = update_scr;
    lv_obj_add_event_cb(iap_btn, ui_callback, LV_EVENT_SHORT_CLICKED, &iap_btn_user);
    // 点击OTA图标事件
    USER_DATA_CREATE_STATIC(ota_btn_user, UI_ID_OTA);
    ota_btn_user.next_scr = update_scr;
    lv_obj_add_event_cb(ota_btn, ui_callback, LV_EVENT_SHORT_CLICKED, &ota_btn_user);
    ////////////////////////// 添加事件END//////////////////////////
}

static void ui_nes_draw(UI_CTRL_t *ctrl)
{
    LV_UNUSED(ctrl);

    // 初始化信号量
    // g_nes_sem = xSemaphoreCreateBinary();
    // 初始化NES
    // g_nes = nes_init();
    // if (!g_nes)
    // {
    //     printf("\nnes_init failed\n");
    //     return;
    // }

    g_nes_scr = lv_obj_create(NULL);
    g_nes_canvas = lv_canvas_create(g_nes_scr);
    lv_canvas_set_buffer(g_nes_canvas, nes.api->get_workframe(nes.ctrl), nes.api->get_width(nes.ctrl), nes.api->get_height(nes.ctrl), LV_COLOR_FORMAT_RGB565);
    lv_obj_set_size(g_nes_canvas, nes.api->get_width(nes.ctrl), nes.api->get_height(nes.ctrl));
    lv_obj_align(g_nes_canvas, LV_ALIGN_CENTER, 0, 0);
    // 禁用滚动条
    lv_obj_set_scroll_dir(g_nes_scr, LV_DIR_NONE);
    // 左滑退出NES界面
    USER_DATA_CREATE_STATIC(nes_scr_gesture_user, UI_ID_RETURN);
    lv_obj_add_event_cb(g_nes_scr, ui_callback, LV_EVENT_GESTURE, &nes_scr_gesture_user);
}

static void ui_init(UI_CTRL_t *ctrl)
{
    ctrl->disp_init(ctrl);
    ctrl->indev_init(ctrl);
    ctrl->draw(ctrl);
    ctrl->nes_draw(ctrl);
}

static void ui_set_flush(UI_CTRL_t *ctrl, uint8_t flush_flag)
{
    ctrl->flush_flag = flush_flag;
}

static uint8_t ui_get_flush(UI_CTRL_t *ctrl)
{
    return ctrl->flush_flag;
}

static void ui_set_usb_flag(UI_CTRL_t *ctrl, uint8_t usb_flag)
{
    ctrl->usb_flag = usb_flag;
}

static uint8_t ui_get_usb_flag(UI_CTRL_t *ctrl)
{
    return ctrl->usb_flag;
}

static void ui_set_iap_flag(UI_CTRL_t *ctrl, uint8_t iap_flag)
{
    ctrl->iap_flag = iap_flag;
}

static uint8_t ui_get_iap_flag(UI_CTRL_t *ctrl)
{
    return ctrl->iap_flag;
}

static void ui_set_ota_flag(UI_CTRL_t *ctrl, uint8_t ota_flag)
{
    ctrl->ota_flag = ota_flag;
}

static uint8_t ui_get_ota_flag(UI_CTRL_t *ctrl)
{
    return ctrl->ota_flag;
}

static void ui_back_menu(UI_CTRL_t *ctrl)
{
    LV_UNUSED(ctrl);
    lv_screen_load_anim(g_menu_scr, LV_SCR_LOAD_ANIM_FADE_IN, 0, 0, false);
}

static void ui_update(UI_CTRL_t *ctrl)
{
    LV_UNUSED(ctrl);
    lv_timer_handler();
}

////////////////////////////////////////////////////////////////////

static UI_CTRL_t ui_ctrl = {
    .flush_flag = 0,
    .usb_flag = 0,
    .iap_flag = 0,
    .ota_flag = 0,

    .disp_init = ui_disp_init,
    .indev_init = ui_indev_init,
    .draw = ui_draw,
    .nes_draw = ui_nes_draw,
};

static UI_API_t ui_api = {
    .init = ui_init,
    .get_flush = ui_get_flush,
    .set_flush = ui_set_flush,
    .set_usb_flag = ui_set_usb_flag,
    .get_usb_flag = ui_get_usb_flag,
    .set_iap_flag = ui_set_iap_flag,
    .get_iap_flag = ui_get_iap_flag,
    .set_ota_flag = ui_set_ota_flag,
    .get_ota_flag = ui_get_ota_flag,
    .back_menu = ui_back_menu,
    .update = ui_update,
};

const UI_t ui = {
    .ctrl = &ui_ctrl,
    .api = &ui_api,
};