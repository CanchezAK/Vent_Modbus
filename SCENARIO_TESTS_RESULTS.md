# Результаты сценарного тестирования

Метод: логическая проверка по коду (без запуска на железе). Ожидаемое поведение сверено с реализацией в компоненте Logic.

## Итоги
- S01 — PASS
- S02 — PASS
- S03 — PASS
- S04 — PASS
- S05 — PASS
- S06 — PASS
- S07 — PASS
- S08 — PASS
- S09 — PASS
- S09a — PASS
- S10 — PASS
- S11 — PASS
- S11a — PASS
- S12 — PASS
- S13 — PASS
- S14 — PASS
- S15 — PASS
- S16 — PASS
- S17 — PASS
- S18 — PASS

## Примечания
- Тесты относятся к [SCENARIO_TESTS.md](SCENARIO_TESTS.md).
- Формула расчёта: относительный масштаб по диапазону `desired..threshold` со смещением `config_fan_min_percent`.
- Регистр `config_fan_min_threshold` удалён.
- `alarm_filter` — только уведомление, на скорость не влияет.
- Для аппаратной валидации потребуется реальное устройство (входы двери/дыма, датчик SHT31, вентилятор).
