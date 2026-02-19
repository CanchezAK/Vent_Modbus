# EEZ UI re-import: обязательные правки для этого проекта

Документ описывает, какие изменения нужно внести после каждой замены/генерации UI из EEZ, чтобы проект на ESP-IDF + LVGL9 снова собирался и работал.

## 1) Исправить include-пути LVGL в UI-заголовках

**Проблема:** EEZ генерирует `#include <lvgl/lvgl.h>`, а в этом проекте LVGL подключается как `#include <lvgl.h>`.

**Что менять:**

- `components/UI/include/ui.h`
- `components/UI/include/actions.h`
- `components/UI/include/screens.h`
- `components/UI/include/fonts.h`
- `components/UI/include/images.h`
- `components/UI/include/styles.h`
- `components/UI/include/eez-flow.h`

**Правка:**

- заменить `#include <lvgl/lvgl.h>` → `#include <lvgl.h>`

---

## 2) Исправить private include в `eez-flow.h`

**Проблема:** в нашей сборке нет пути `lvgl/src/lvgl_private.h`.

**Файл:**

- `components/UI/include/eez-flow.h`

**Правка (в блоке с проверкой версии LVGL):**

- заменить `#include <lvgl/src/lvgl_private.h>` → `#include <lvgl_private.h>`

---

## 3) Исправить LVGL9 API в `eez-flow.cpp` (`lv_part_t`)

**Проблема:** EEZ может генерировать вызовы вида `lv_obj_get_style_opa(..., 0)`,
но в LVGL9 второй аргумент должен быть типа `lv_part_t`.

**Файл:**

- `components/UI/src/eez-flow.cpp`

**Обязательные замены:**

1. `lv_obj_get_style_opa((lv_obj_t *)a->user_data, 0)`
   → `lv_obj_get_style_opa((lv_obj_t *)a->user_data, LV_PART_MAIN)`

2. `lv_obj_get_style_opa(obj, 0)`
   → `lv_obj_get_style_opa(obj, LV_PART_MAIN)`

---

## 4) Проверить `components/UI/CMakeLists.txt`

**Проблема:** после регенерации иногда остаются ссылки на файлы, которых уже нет.

**Файл:**

- `components/UI/CMakeLists.txt`

**Что проверить:**

- в `SRCS` перечислены только реально существующие файлы из `components/UI/src`
- нет старых удалённых файлов (например, старого `ui_font_*.c`, если его больше не генерируют)

Минимальный ожидаемый набор обычно такой:

- `UI.c`
- `src/ui.c`
- `src/screens.c`
- `src/images.c`
- `src/styles.c`
- `src/eez-flow.cpp`

---

## 5) Проверить action-обработчики

**Когда нужно:** только если новый UI вызывает C-action функции.

**Файлы:**

- объявления: `components/UI/include/actions.h`
- реализации: `components/UI/UI.c`

**Правило:**

- каждая функция, на которую есть ссылка в `screens.c`/`ui.c`, должна быть объявлена в `actions.h` и реализована в `UI.c`.

Если EEZ Flow используется без C-action (через `flowPropagateValueLVGLEvent`), массив `actions[]` в `src/ui.c` может содержать только `0`.

---

## 6) Поворот экрана (если UI сделан в landscape 1280x720)

В этом проекте для панели HX8394 аппаратный `swap/mirror` не поддерживается.
Используется программный поворот LVGL.

**Файл:**

- `components/Display/Display.c`

**Что должно быть включено:**

- в `lvgl_port_display_cfg_t.flags`: `.sw_rotate = 1`
- в `disp_cfg.rotation`: все `false` (без hardware rotate)
- после `lvgl_port_add_disp_dsi(...)`: `lv_disp_set_rotation(disp, LV_DISPLAY_ROTATION_90)`

---

## 7) Контрольная проверка после правок

1. Собрать проект (`build`).
2. Если есть ошибки линковки по action-функциям — синхронизировать `actions.h` и `UI.c`.
3. Прошить и проверить экран/тач.

---

## Типовые симптомы и причины

- `fatal error: lvgl/lvgl.h: No such file or directory`
  - не заменены include в UI headers.

- `fatal error: lvgl/src/lvgl_private.h: No such file or directory`
  - не исправлен private include в `eez-flow.h`.

- `invalid conversion from 'int' to 'lv_part_t'`
  - не исправлены вызовы `lv_obj_get_style_opa(..., 0)` в `eez-flow.cpp`.

- `undefined reference to action_*`
  - отсутствуют реализации/объявления action-функций.
