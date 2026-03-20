/*
 * monitor.h
 * Display printing abstraction and typed print argument helpers.
 */
#ifndef MONITOR_H
#define MONITOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Display and LVGL initialization */
void monitor_init(void);
/* Plays startup animation and blocks for the specified duration in milliseconds. */
void monitor_show_boot_animation(uint32_t duration_ms);

typedef enum {
    MONITOR_MENU_ICON_WIFI = 0,
    MONITOR_MENU_ICON_BLUETOOTH,
    MONITOR_MENU_ICON_CUSTOM,
} monitor_menu_icon_t;

typedef struct {
    /* 1-bit rows, MSB-first. */
    const uint8_t *data;
    uint8_t width;
    uint8_t height;
} monitor_menu_icon_bitmap_t;

typedef struct {
    const char *title;
    uint8_t countdown_s;
    uint8_t countdown_phase;
    monitor_menu_icon_t icon;
    monitor_menu_icon_bitmap_t custom_icon;
} monitor_menu_view_t;

/* Shows pixel menu view and updates title/countdown/icon. */
void monitor_show_menu(const monitor_menu_view_t *view);

typedef enum {
    MONITOR_PRINT_ARG_STR,
    MONITOR_PRINT_ARG_SIGNED,
    MONITOR_PRINT_ARG_UNSIGNED,
    MONITOR_PRINT_ARG_FLOAT,
    MONITOR_PRINT_ARG_BOOL,
    MONITOR_PRINT_ARG_CHAR,
} monitor_print_arg_type_t;

typedef struct {
    monitor_print_arg_type_t type;
    union {
        const char *str;
        long long signed_value;
        unsigned long long unsigned_value;
        double float_value;
        bool bool_value;
        char char_value;
    } value;
} monitor_print_arg_t;

/* Internal printer used by print(...) macro. */
void monitor_print(size_t count, const monitor_print_arg_t *args);

static inline monitor_print_arg_t monitor_print_make_str(const char *value)
{
    return (monitor_print_arg_t){ .type = MONITOR_PRINT_ARG_STR, .value.str = value };
}

static inline monitor_print_arg_t monitor_print_make_signed(long long value)
{
    return (monitor_print_arg_t){ .type = MONITOR_PRINT_ARG_SIGNED, .value.signed_value = value };
}

static inline monitor_print_arg_t monitor_print_make_unsigned(unsigned long long value)
{
    return (monitor_print_arg_t){ .type = MONITOR_PRINT_ARG_UNSIGNED, .value.unsigned_value = value };
}

static inline monitor_print_arg_t monitor_print_make_float(double value)
{
    return (monitor_print_arg_t){ .type = MONITOR_PRINT_ARG_FLOAT, .value.float_value = value };
}

static inline monitor_print_arg_t monitor_print_make_bool(bool value)
{
    return (monitor_print_arg_t){ .type = MONITOR_PRINT_ARG_BOOL, .value.bool_value = value };
}

static inline monitor_print_arg_t monitor_print_make_char(char value)
{
    return (monitor_print_arg_t){ .type = MONITOR_PRINT_ARG_CHAR, .value.char_value = value };
}

#define MONITOR_PRINT_VALUE_CONVERTER(arg) _Generic((arg), \
    char: monitor_print_make_char, \
    signed char: monitor_print_make_signed, \
    short: monitor_print_make_signed, \
    int: monitor_print_make_signed, \
    long: monitor_print_make_signed, \
    long long: monitor_print_make_signed, \
    unsigned char: monitor_print_make_unsigned, \
    unsigned short: monitor_print_make_unsigned, \
    unsigned int: monitor_print_make_unsigned, \
    unsigned long: monitor_print_make_unsigned, \
    unsigned long long: monitor_print_make_unsigned, \
    float: monitor_print_make_float, \
    double: monitor_print_make_float, \
    bool: monitor_print_make_bool, \
    char *: monitor_print_make_str, \
    const char *: monitor_print_make_str, \
    default: monitor_print_make_signed \
)

#define MONITOR_PRINT_VALUE(arg) MONITOR_PRINT_VALUE_CONVERTER(arg)(arg)

#define MONITOR_PRINT_COUNT_IMPL(_1, _2, _3, _4, _5, _6, _7, _8, N, ...) N
#define MONITOR_PRINT_COUNT(...) MONITOR_PRINT_COUNT_IMPL(__VA_ARGS__, 8, 7, 6, 5, 4, 3, 2, 1)

#define MONITOR_PRINT_MAP_1(a1) MONITOR_PRINT_VALUE(a1)
#define MONITOR_PRINT_MAP_2(a1, a2) MONITOR_PRINT_VALUE(a1), MONITOR_PRINT_VALUE(a2)
#define MONITOR_PRINT_MAP_3(a1, a2, a3) MONITOR_PRINT_VALUE(a1), MONITOR_PRINT_VALUE(a2), MONITOR_PRINT_VALUE(a3)
#define MONITOR_PRINT_MAP_4(a1, a2, a3, a4) MONITOR_PRINT_VALUE(a1), MONITOR_PRINT_VALUE(a2), MONITOR_PRINT_VALUE(a3), MONITOR_PRINT_VALUE(a4)
#define MONITOR_PRINT_MAP_5(a1, a2, a3, a4, a5) MONITOR_PRINT_VALUE(a1), MONITOR_PRINT_VALUE(a2), MONITOR_PRINT_VALUE(a3), MONITOR_PRINT_VALUE(a4), MONITOR_PRINT_VALUE(a5)
#define MONITOR_PRINT_MAP_6(a1, a2, a3, a4, a5, a6) MONITOR_PRINT_VALUE(a1), MONITOR_PRINT_VALUE(a2), MONITOR_PRINT_VALUE(a3), MONITOR_PRINT_VALUE(a4), MONITOR_PRINT_VALUE(a5), MONITOR_PRINT_VALUE(a6)
#define MONITOR_PRINT_MAP_7(a1, a2, a3, a4, a5, a6, a7) MONITOR_PRINT_VALUE(a1), MONITOR_PRINT_VALUE(a2), MONITOR_PRINT_VALUE(a3), MONITOR_PRINT_VALUE(a4), MONITOR_PRINT_VALUE(a5), MONITOR_PRINT_VALUE(a6), MONITOR_PRINT_VALUE(a7)
#define MONITOR_PRINT_MAP_8(a1, a2, a3, a4, a5, a6, a7, a8) MONITOR_PRINT_VALUE(a1), MONITOR_PRINT_VALUE(a2), MONITOR_PRINT_VALUE(a3), MONITOR_PRINT_VALUE(a4), MONITOR_PRINT_VALUE(a5), MONITOR_PRINT_VALUE(a6), MONITOR_PRINT_VALUE(a7), MONITOR_PRINT_VALUE(a8)

#define MONITOR_PRINT_MAP_PICK(_1, _2, _3, _4, _5, _6, _7, _8, NAME, ...) NAME
#define MONITOR_PRINT_MAP(...) MONITOR_PRINT_MAP_PICK(__VA_ARGS__, MONITOR_PRINT_MAP_8, MONITOR_PRINT_MAP_7, MONITOR_PRINT_MAP_6, MONITOR_PRINT_MAP_5, MONITOR_PRINT_MAP_4, MONITOR_PRINT_MAP_3, MONITOR_PRINT_MAP_2, MONITOR_PRINT_MAP_1)(__VA_ARGS__)

/*
 * Python-style print:
 * print("temp:", 23, "C");
 */
#define print(...) monitor_print(MONITOR_PRINT_COUNT(__VA_ARGS__), (monitor_print_arg_t[]){ MONITOR_PRINT_MAP(__VA_ARGS__) })

#endif /* MONITOR_H */
