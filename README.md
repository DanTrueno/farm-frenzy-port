# Farm Frenzy (Весёлая ферма) — native port

*A fan-made, cross-platform reimplementation of the engine of Farm Frenzy (Melesta / Alawar, 2007).
It runs the original game data natively on macOS, Windows and Linux. No game files are included —
you need your own copy of the game.*

Неофициальный порт первой «Весёлой фермы» на современные системы. Движок переписан на C++17 + SDL3
по результатам изучения оригинального `farm.wrp.exe`. Графика, звуки, тексты и параметры уровней
берутся из файлов вашей копии игры во время работы.

> **Файлов игры в репозитории нет и быть не должно.** Игра — собственность Melesta и Alawar.
> Для запуска нужна ваша легальная копия «Весёлой фермы» (русская версия 2007 года):
> из неё понадобятся `Data/data.pack`, `Data/ru.pack` и, по желанию, `farm.wrp.exe` (тексты подсказок).

## Что работает

Логотипы, меню, профили, настройки (звук, музыка, полный экран), карта на 48 уровней, магазин,
все механики уровня: полив и трава, гуси/овцы/коровы с голодом и продуктами, склад, машина и рынок,
6 фабрик с улучшениями, медведи (тень перед падением, клетка, побег из клетки), собака и кошка,
цели и медали, обучающие подсказки, 16 наград, сохранение посреди уровня, облака.
Механики сверены с декомпиляцией и с [вики по игре](https://farm-frenzy.fandom.com/ru/wiki/Весёлая_ферма).

## Сборка

Нужны CMake 3.20+ и компилятор C++17. SDL3 подтянется сам (скачивается и собирается статически),
других зависимостей нет. Путь к игре можно не указывать — тогда положите папку `Data` рядом с
программой вручную.

### macOS

```bash
scripts/build_macos.sh "/путь/к/Веселая ферма"
```

Результат: `dist/Farm Frenzy.app` (универсальный: Apple Silicon + Intel, данные игры внутри).
Нужны Xcode Command Line Tools (`xcode-select --install`) и CMake (`brew install cmake`).

### Windows

Visual Studio 2022+ с компонентом «Разработка классических приложений на C++» и CMake:

```bat
scripts\build_windows.bat "C:\Games\Веселая ферма"
```

Результат: `dist\FarmFrenzy\FarmFrenzy.exe` и рядом `Data\`.

Кросс-сборка под Windows с macOS/Linux (MinGW-w64):

```bash
cmake -S . -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw64.cmake -DFF_VENDORED_SDL=ON
cmake --build build-win -j
```

### Linux

```bash
scripts/build_linux.sh "/путь/к/Веселая ферма"
```

Для сборки SDL3 нужны dev-пакеты X11/Wayland и звука (список — в `.github/workflows/build.yml`).

### Параметры CMake

| Параметр | Что делает |
|---|---|
| `FF_GAME_DIR` | папка игры: `data.pack`/`ru.pack` копируются к сборке, подсказки извлекаются из `farm.wrp.exe` (нужен Python 3) |
| `FF_HD_PACK` | путь к `hd.pack` с увеличенными текстурами (см. ниже) |
| `FF_VENDORED_SDL` | всегда собирать свой SDL3, даже если он есть в системе |
| `FF_ICON` | иконка приложения: `.icns` для macOS, `.ico` для Windows |

## Где игра ищет данные

1. переменная окружения `FF_DATA`;
2. `Data/` рядом с исполняемым файлом (на macOS — `Contents/Resources/Data` внутри `.app`);
3. `Data/` рядом с `.app`;
4. папка настроек пользователя (`…/Melesta/FarmFrenzy/Data`).

Профили и сохранения лежат в папке настроек SDL: на macOS `~/Library/Application Support/Melesta/FarmFrenzy/`,
на Windows `%APPDATA%\Melesta\FarmFrenzy\`.

## HD-текстуры (необязательно)

Текстуры можно увеличить нейросетью: на большом экране игра становится заметно чётче.
Порт подхватывает `Data/hd.pack`, если он есть, и сам выбирает между HD и оригиналом по размеру окна.

1. Выгрузить текстуры в PNG: `cmake --build build --target dumptex`, затем `dumptex <папка Data> <папка png>`.
2. Увеличить все PNG в 4 раза Real-ESRGAN, например консольной утилитой из [Upscayl](https://upscayl.org)
   с моделью `upscayl-standard-4x` (имена файлов: путь с `/`, заменённым на `@@`).
3. Собрать пак: `python3 tools/make_hd_pack.py <png> <увеличенные> hd.pack 3`
   (нужны Pillow и NumPy). Фоны при этом смягчаются, чтобы нейросеть не дорисовывала лишнего.

## Отладка

`FF_SKIP_LOGO=1`, `FF_UNLOCK_ALL=1` (все уровни на карте), `FF_NO_HD=1`, `FF_FIXED_DT=1`,
`FF_SHOT=файл.png FF_SHOT_FRAME=N` (сохранить кадр и выйти),
`FF_SCRIPT="кадр:команда;..."` — скриптовый ввод для автотестов. Команды: `click x y`, `key enter|esc`,
`text …`, `profile имя`, `level N`, `map`, `shop`, `collect`, `money N`, `stars N`, `bear`, `hitbear N`,
`shopset i n`, `award n`, `awards`, `savegame`, `continue`.

## Устройство

| Файл | Что делает |
|---|---|
| `src/vfs.*` | чтение `.pack` (это обычные zip) |
| `src/image.*` | текстуры: JNG (JPEG + альфа), DDS ARGB8888/4444/DXT5 |
| `src/platform.*` | окно, рендер, звук на SDL3 |
| `src/engine.*`, `src/ui.*` | XML, шрифты `.fnt`, спрайты, кнопки, прогресс-бары |
| `src/menus.cpp`, `src/map.cpp` | меню, профили, настройки, карта, магазин |
| `src/game*.cpp`, `src/buildings.cpp` | поле, животные, медведи, здания, рынок |
| `src/tips.*`, `src/awards.*`, `src/savegame.cpp` | подсказки, награды, сохранение уровня |
| `tools/` | вспомогательные скрипты (см. ниже) |

### tools/

- `ExportDecomp.java`, `DecompAt.java` — скрипты для Ghidra (headless): декомпилировать `farm.wrp.exe`.
- `organize.py` — назвать функции по строкам-сигнатурам и разложить вывод по классам; `show.py` — просмотр;
  `const.py` — прочитать константы по адресу; `tip_sites.py` — найти места показа подсказок.
- `extract_tips.py` — достать тексты подсказок из вашего `farm.wrp.exe` (вызывается сборкой).
- `dumptex.cpp`, `make_hd_pack.py` — выгрузка текстур и сборка `hd.pack`.

Результаты декомпиляции в репозиторий не входят: их можно получить этими скриптами из своей копии игры.

## Лицензия

Код порта распространяется под лицензией [MIT](LICENSE). Она не распространяется на игру
«Весёлая ферма» и её файлы — они принадлежат Melesta и Alawar.

## Сторонний код

- [SDL3](https://libsdl.org) — zlib license (скачивается при сборке)
- [stb_image, stb_vorbis, stb_image_write](https://github.com/nothings/stb) — public domain / MIT
- [pugixml](https://pugixml.org) — MIT
