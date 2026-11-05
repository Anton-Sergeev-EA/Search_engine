# Search Engine

## Русский

Я разрабатываю поисковую систему для текстовых файлов на C++20. Индексатор будет сохранять частоты слов в PostgreSQL, а поисковый сервер будет ранжировать документы по суммарной частоте слов запроса.

Сейчас реализован модуль обработки текста UTF-8. Для сборки нужны CMake 3.20+, компилятор с поддержкой C++20 и библиотека ICU.

## English

I am building a C++20 search engine for text files. The indexer will store word frequencies in PostgreSQL, and the search server will rank documents by the total frequency of the query terms.

The UTF-8 text processing module is implemented. Building requires CMake 3.20+, a C++20 compiler, and ICU.

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Проверка каталога / Directory scan

Я добавил чтение INI и рекурсивный отбор документов по расширениям. Скопируйте
`config.example.ini` в `config.ini` и задайте свои параметры. Относительный путь
каталога отсчитывается от INI-файла. Пароль может быть пустым. Храните рабочий
INI с правами доступа только для владельца.

I added INI configuration and recursive document discovery by extension. Copy
`config.example.ini` to `config.ini` and enter your settings. Relative document
paths are resolved from the INI file location. Empty passwords are accepted.
Restrict access to the working INI file to its owner.

```sh
./build/search_scan config.ini
```

Расширения сравниваются без учёта регистра ASCII. Символические ссылки
пропускаются. Ошибка доступа прерывает обход с ненулевым кодом возврата.
Программа выводит отсортированные абсолютные пути. Подключение к PostgreSQL
будет добавлено на следующем этапе.

Extensions are matched without ASCII case sensitivity. Symbolic links are
skipped. Access errors stop the scan with a nonzero exit code. The program
prints sorted absolute paths. PostgreSQL integration is the next stage.
