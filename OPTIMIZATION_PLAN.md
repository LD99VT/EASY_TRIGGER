# EasyTrigger — полный код-ревью и план оптимизации

## Обзор архитектуры

Проект состоит из трёх слоёв:

| Слой | Папка | Назначение |
|------|-------|-----------|
| Ядро (`core`) | `Source/core/` | Вычисления таймкода, хранилище конфигурации |
| Движок (`engine`) | `Source/engine/` | Аудио/MIDI/сеть I/O, BridgeEngine |
| Интерфейс (`UI`) | `Source/` | JUCE-окно, логика триггеров |

Используется C++20, CMake, JUCE 8.0.4.

---

## Результаты ревью

### 1. Производительность — двойная загрузка атомика (исправлено)

**Файл:** `Source/engine/OscInput.cpp`, метод `parseFloatTime`

**Проблема.** `fps_` — `std::atomic<FrameRate>`. До исправления выполнялось два независимых `.load()`:
```cpp
// было
const auto fps    = frameRateToDouble(fps_.load(std::memory_order_relaxed));
const int  fpsInt = frameRateToInt   (fps_.load(std::memory_order_relaxed));   // второй load
```
Между двумя вызовами поток мог записать новое значение в `fps_`, что приводило к несогласованности `fpsDouble` и `fpsInt`.

**Исправление (применено).**
```cpp
const FrameRate fps   = fps_.load(std::memory_order_relaxed);
const auto fpsDouble  = frameRateToDouble(fps);
const int  fpsInt     = juce::jmax(1, frameRateToInt(fps));
```

---

### 2. Производительность — передача строк по значению (исправлено)

**Файл:** `Source/engine/OscInput.h` / `OscInput.cpp`, метод `start()`

**Проблема.** Три параметра `juce::String` передавались по значению, вызывая лишние копирования при каждом старте OSC-ввода:
```cpp
// было
bool start(int port, juce::String bindIp, FrameRate fps,
           juce::String addrStr, juce::String addrFloat, juce::String& errorOut);
```

**Исправление (применено).**
```cpp
bool start(int port, const juce::String& bindIp, FrameRate fps,
           const juce::String& addrStr, const juce::String& addrFloat, juce::String& errorOut);
```

---

### 3. Потокобезопасность — корректна, но требует документации

**Файлы:** `EngineLtcInput.h`, `EngineMtcInput.h`, `EngineMtcOutput.h`, `EngineArtnetOutput.h`

**Наблюдение.** В проекте используются две разные стратегии синхронизации:
- `std::atomic<uint64_t>` + `packTimecode/unpackTimecode` — для Timecode в LTC и ArtNet.
- `juce::SpinLock` + локальные переменные — для Timecode в MTC (из-за необходимости хранить несколько полей атомарно).

Обе стратегии корректны, но это нигде явно не задокументировано.

**Рекомендация.** Добавить комментарий в `EngineTimecodeCore.h`, объясняющий выбор:
```cpp
// Timecode is packed into a single uint64_t so that cross-thread
// reads/writes are always atomic (no torn reads).  Use packTimecode()
// / unpackTimecode() to access it.  Classes that need to read
// *multiple* related fields atomically (e.g. MtcInput: TC + fps +
// syncTimeMs) use juce::SpinLock instead.
```

---

### 4. Надёжность — адаптация битового периода в LTC-декодере

**Файл:** `Source/engine/vendor/timecode/EngineLtcInput.h`, метод `onEdgeDetected`

**Наблюдение.** Оценка периода бита использует экспоненциальное скользящее среднее с α = 0.05:
```cpp
bitPeriodEstimate = bitPeriodEstimate * 0.95 + measured * 0.05;
```
При α = 0.05 постоянная времени ≈ 19 бит (~¼ кадра). Это хорошо для подавления шума, но
медленно при переключении источника с другой частотой кадров. Начальное значение
`currentSampleRate / 2160.0` (~27 fps) минимизирует время сходимости.

**Рекомендация.** При переключении источника (т. е. при вызове `start()`) рассмотреть
быстрое первичное усреднение (α = 0.3) на первые N = 5 бит, затем переход к α = 0.05.
Пример скелета:
```cpp
int fastAdaptFrames = 0;  // счётчик быстрых итераций

// в onEdgeDetected, после проверки диапазона:
const double alpha = (fastAdaptFrames < 5) ? 0.3 : 0.05;
if (fastAdaptFrames < 5) ++fastAdaptFrames;
bitPeriodEstimate = bitPeriodEstimate * (1.0 - alpha) + measured * alpha;
```

---

### 5. Корректность — `ConfigStore::loadJsonFile` не различает пустой файл и ошибку разбора

**Файл:** `Source/core/ConfigStore.cpp`

**Проблема.** Метод возвращает `std::nullopt` как для отсутствующего файла, так и для
файла с некорректным JSON. Вызывающий код не может отличить одно от другого.

```cpp
auto parsed = juce::JSON::parse(content);
if (parsed.isVoid())
    return std::nullopt;       // и ошибка разбора, и пустой файл — один результат
```

**Рекомендация.** Вернуть `juce::Result` или использовать `juce::JSON::parseWithResult()`:
```cpp
static std::optional<juce::var> loadJsonFile(const juce::File& file,
                                              juce::String* errorOut = nullptr)
{
    if (!file.existsAsFile()) return std::nullopt;
    juce::var parsed;
    juce::Result r = juce::JSON::parseWithResult(file.loadFileAsString(), parsed);
    if (r.failed())
    {
        if (errorOut) *errorOut = r.getErrorMessage();
        return std::nullopt;
    }
    return parsed;
}
```

---

### 6. Надёжность — `BridgeEngine::scanAudioDevices` блокирует главный поток

**Файл:** `Source/engine/BridgeEngine.cpp`

**Проблема.** Каждый вызов `scanAudioInputs()` / `scanAudioOutputs()` создаёт новый
`juce::AudioDeviceManager`, инициализирует все драйверы и сканирует устройства. На системах
с ASIO или большим числом устройств это занимает 100–500 мс на главном потоке.

**Рекомендация.** Сканирование уже выполняется в отдельном потоке (`AudioScanThread` в
`TriggerMainWindow`). Убедиться, что `scanAudioInputs/Outputs` вызываются *только* из него,
и добавить `jassert(!juce::MessageManager::existsAndIsCurrentThread())` в начало функции.

---

### 7. Читаемость — монолитный `TriggerContentComponent`

**Файл:** `Source/TriggerMainWindow.h` / `TriggerMainWindow.cpp`

**Проблема.** `TriggerContentComponent` содержит >100 полей и >60 методов: обработку UI,
логику триггеров, управление конфигурацией, работу с Resolume, таймеры.

**Рекомендация.** Поэтапный рефакторинг:

1. Выделить `TriggerTableModel` (методы `getNumRows`, `paintCell`, `paintRowBackground`,
   `refreshComponentForCell`, `cellClicked`, `cellDoubleClicked`) в отдельный класс.
2. Вынести `TriggerClip` и `DisplayRow` в отдельный заголовочный файл
   `Source/core/TriggerModel.h`.
3. Вынести логику триггеров (`evaluateAndFireTriggers`, `processEndActions`,
   `updateClipCountdowns`) в `Source/core/TriggerLogic.h/.cpp`.

---

### 8. Потенциальная ошибка — `MtcInput` SpinLock в деструкторе

**Файл:** `Source/engine/vendor/timecode/EngineMtcInput.h`

**Наблюдение.** `MtcInput::stop()` обнуляет `midiInput` и ждёт прекращения MIDI-колбэков,
но не берёт `tcLock`. Если MIDI-поток в этот момент держит `tcLock` в
`reconstructAndSync()`, деструктор завершится корректно. Однако если поток вдруг
успеет вызвать `reconstructAndSync()` после `midiInput = nullptr`, это приведёт к UB.

**Рекомендация.** Остановку следует выполнять так:
```cpp
void stop()
{
    if (midiInput != nullptr)
    {
        midiInput->stop();   // ждёт завершения текущего колбэка
        midiInput = nullptr; // только после полной остановки
    }
    isRunningFlag.store(false, std::memory_order_relaxed);
    currentDeviceIndex = -1;
}
```
Текущая реализация уже делает это правильно (`stop()` → `nullptr`). Убедиться, что
JUCE гарантирует: после `midiInput->stop()` колбэки более не вызываются — это является
частью контракта `juce::MidiInput` и документировано в JUCE API.

---

### 9. Читаемость — кодировка комментариев в vendor-заголовках

**Файлы:** `Source/engine/vendor/timecode/*.h`

**Проблема.** Файлы содержат BOM UTF-8, а многобайтовые символы Unicode в комментариях
(тире, стрелки, кавычки) в ряде редакторов (особенно Windows Notepad, некоторые версии
Visual Studio без BOM-настройки) могут отображаться как многобайтовые артефакты:
`=E2=80=93` вместо `–`, `=C2=B1` вместо `±`, `=E2=86=92` вместо `→`.

**Рекомендация.** Заменить Unicode-символы ASCII-эквивалентами в vendor-заголовках:

| Unicode | Код | Заменить на ASCII |
|---------|-----|-------------------|
| `–` (en dash) | U+2013 | `--` |
| `±` (plus-minus) | U+00B1 | `+/-` |
| `→` (arrow right) | U+2192 | `->` |
| `'` (right single quote) | U+2019 | `'` |

---

### 10. Производительность — `ArtnetInput::run()` занимает CPU при тишине

**Файл:** `Source/engine/vendor/timecode/EngineArtnetInput.h`

**Наблюдение.** `socket_->waitUntilReady(true, 100)` возвращает `false` через 100 мс при
отсутствии пакетов, после чего сразу начинается следующая итерация. Это создаёт цикл
без sleep.

**Текущее поведение** — уже корректно: `waitUntilReady` с таймаутом 100 мс блокирует поток.
Никаких изменений не требуется.

---

## Сводная таблица приоритетов

| # | Файл | Тип | Статус | Приоритет |
|---|------|-----|--------|-----------|
| 1 | `OscInput.cpp` | Потокобезопасность | ✅ Исправлено | Высокий |
| 2 | `OscInput.h/cpp` | Производительность | ✅ Исправлено | Средний |
| 3 | `EngineTimecodeCore.h` | Документация | 💡 Рекомендация | Низкий |
| 4 | `EngineLtcInput.h` | Надёжность | 💡 Рекомендация | Средний |
| 5 | `ConfigStore.cpp` | Корректность | 💡 Рекомендация | Средний |
| 6 | `BridgeEngine.cpp` | Надёжность | 💡 Рекомендация | Средний |
| 7 | `TriggerMainWindow` | Архитектура | 💡 Рекомендация | Низкий |
| 8 | `EngineMtcInput.h` | Корректность | ✅ Уже корректно | — |
| 9 | `vendor/timecode/*.h` | Читаемость | 💡 Рекомендация | Низкий |
| 10 | `EngineArtnetInput.h` | Производительность | ✅ Уже корректно | — |

---

## Следующие шаги

1. **Краткосрочно (1–2 спринта)**
   - Реализовать `ConfigStore::loadJsonFile` с `juce::JSON::parseWithResult()` (пункт 5).
   - Добавить `jassert` в `scanAudioDevices` (пункт 6).
   - Добавить документирующий комментарий про стратегии синхронизации (пункт 3).

2. **Среднесрочно (3–4 спринта)**
   - Быстрая первичная адаптация LTC-декодера (пункт 4).
   - Выделить `TriggerTableModel` (пункт 7, шаг 1).

3. **Долгосрочно**
   - Полный рефакторинг `TriggerContentComponent` по шагам из пункта 7.
   - Унификация кодировки в vendor-заголовках (пункт 9).
