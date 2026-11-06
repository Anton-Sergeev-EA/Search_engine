# Search Engine

## Русский

Я разрабатываю поисковую систему для текстовых файлов на C++20. Индексатор будет сохранять частоты слов в PostgreSQL, а поисковый сервер будет ранжировать документы по суммарной частоте слов запроса.

Реализованы обработка текста UTF-8, чтение INI, обход каталогов и последовательный индексатор PostgreSQL. Для сборки нужны CMake 3.20+, компилятор C++20, ICU, pkg-config и libpqxx 7.x.

## English

I am building a C++20 search engine for text files. The indexer will store word frequencies in PostgreSQL, and the search server will rank documents by the total frequency of the query terms.

UTF-8 processing, INI configuration, directory scanning, and a sequential PostgreSQL indexer are implemented. Building requires CMake 3.20+, a C++20 compiler, ICU, pkg-config, and libpqxx 7.x.

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
Программа выводит отсортированные абсолютные пути. Для записи в PostgreSQL используйте индексатор ниже.

Extensions are matched without ASCII case sensitivity. Symbolic links are
skipped. Access errors stop the scan with a nonzero exit code. The program
prints sorted absolute paths. Use the indexer below to write to PostgreSQL.

## Индексация / Indexing

Создайте базу PostgreSQL и пользователя с правами создания таблиц. Укажите
подключение и каталог в `config.ini`, затем запустите индексатор.

Create a PostgreSQL database and a user allowed to create tables. Enter the
connection settings and document directory in `config.ini`, then run the indexer.

```sh
./build/search_indexer config.ini
```

Таблицы создаются автоматически. Индекс выбранного каталога заменяется одной
транзакцией, включая удаление устаревших документов. Ошибка сохраняет прежний
индекс. Данные других каталогов сохраняются. Запись нескольких экземпляров
сериализуется блокировкой PostgreSQL. Пустой набор документов очищает индекс
выбранного каталога. Содержимое файлов должно быть в UTF-8.

Tables are created automatically. One transaction replaces the selected
directory's index, including stale documents. Failures preserve its previous
index. Other directories retain their data. PostgreSQL locks serialize writers.
An empty document set clears the selected directory's index. Files must use UTF-8.

Индексатор пока работает последовательно и держит частоты всех документов в
памяти. Поисковый сервер и пул потоков запланированы на следующие этапы.

The indexer currently runs sequentially and keeps all document word counts in
memory. The search server and thread pool are planned for subsequent stages.

## Проверки PostgreSQL / PostgreSQL checks

Для интеграционных тестов нужны Python 3 и серверные утилиты PostgreSQL
(`initdb`, `pg_ctl`, `psql`). Запускайте тесты от обычного пользователя.
Тест создаёт отдельную базу в новом каталоге `/tmp/search-pg-*`, использует
локальный Unix-сокет и останавливает сервер после проверок. Файлы тестового
кластера сохраняются для диагностики.

Integration tests require Python 3 and PostgreSQL server utilities
(`initdb`, `pg_ctl`, `psql`). Run them as a regular user. Tests create a
separate cluster in a new `/tmp/search-pg-*` directory, use a local Unix socket,
and stop the server after checks. Cluster files are retained for diagnostics.

```sh
cmake -S . -B build -DSEARCH_POSTGRES_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```
