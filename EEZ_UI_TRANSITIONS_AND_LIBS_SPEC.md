# EEZ UI spec: переходы и библиотеки (для ручной синхронизации с EEZ Studio)

## Ограничение среды
Я не могу читать/редактировать файлы вне текущего workspace VS Code.
Поэтому файл `.eez-project` по пути вне проекта изменён быть не может из этой сессии.

---

## 1) Структура экранов и переходов

### Экраны
- `MAIN`
- `SETTINGS`
- `MODBUS`
- `SERVICE`

### Целевое правило навигации
На **каждом** экране добавить кнопку `BACK_TO_MAIN` в левом верхнем углу:
- позиция: `x=16`, `y=16`
- размер: `56x56` (или по размеру ассета стрелки)
- вид: иконка стрелки влево (`img_back`)
- действие по нажатию: `GO_TO_MAIN`

### Матрица переходов
- `MAIN` -> `MAIN` (кнопка back, допустима как no-op)
- `SETTINGS` -> `MAIN`
- `MODBUS` -> `MAIN`
- `SERVICE` -> `MAIN`

### Реализация действия
Использовать уже существующую action-функцию:
- `action_go_to_main(lv_event_t *e)`

Если в текущем `.eez-project` action не привязан, привязать `onClick` каждой кнопки `BACK_TO_MAIN` к `action_go_to_main`.

---

## 2) Ассеты для кнопки назад

Добавить в EEZ image asset:
- `img_back` (ARGB8888, с альфой)

Рекомендации:
- формат экспорта: `ARGB8888`
- прозрачный фон
- размер 24..40 px внутри кнопки

После генерации в C проверить, что descriptor остаётся ARGB8888 и данные соответствуют этому формату.

---

## 3) Пути/библиотеки, которые должны быть в UI-сборке (ESP-IDF)

### UI component
Файл: `components/UI/CMakeLists.txt`

Требуемое:
- `INCLUDE_DIRS`: `include`, `src`
- `REQUIRES`: `lvgl`, `System_Config`, `Modbus_RTU`

Ожидаемые ключевые `SRCS`:
- `UI.c`
- `src/ui.c`
- `src/screens.c`
- `src/images.c`
- `src/styles.c`
- `src/ui_image_*.c`
- `src/ui_font_*.c`

### Display component
Файл: `components/Display/CMakeLists.txt`

Требуемое `REQUIRES`:
- `Peripherials`
- `UI`
- `System_Config`
- `Modbus_RTU`
- `waveshare__esp_lcd_hx8394`
- `espressif__esp_lvgl_port`
- `espressif__esp_lcd_touch_gt911`

---

## 4) Обязательные post-import фиксы для EEZ (чтобы генерация собиралась)

После каждого re-import из EEZ Studio проверить:

1. include-пути LVGL в headers:
- заменить `#include <lvgl/lvgl.h>` на `#include <lvgl.h>`

2. private include в `eez-flow.h`:
- `#include <lvgl/src/lvgl_private.h>` -> `#include <lvgl_private.h>`

3. LVGL9 API в `eez-flow.cpp`:
- `lv_obj_get_style_opa(..., 0)` -> `lv_obj_get_style_opa(..., LV_PART_MAIN)`

4. Согласованность actions:
- объявления в `components/UI/include/actions.h`
- реализации в `components/UI/UI.c`

Подробности: см. `EEZ_UI_IMPORT_FIXES.md`.

---

## 5) Порядок ручной синхронизации `.eez-project`

1. Открыть `.eez-project` в EEZ Studio.
2. Добавить `img_back` в assets.
3. На всех 4 экранах добавить кнопку `BACK_TO_MAIN` в `(16,16)`.
4. Привязать `onClick` -> `action_go_to_main`.
5. Re-generate C code в `components/UI`.
6. Применить post-import фиксы (раздел 4).
7. Собрать проект и проверить навигацию на устройстве.

---

## 6) Проверка после генерации

- Все страницы показывают кнопку «назад» в левом верхнем углу.
- Нажатие на `SETTINGS/MODBUS/SERVICE` возвращает на `MAIN`.
- На `MAIN` нажатие не ломает состояние (no-op/остаётся на `MAIN`).
- Сборка проходит без ошибок линковки/типов.
