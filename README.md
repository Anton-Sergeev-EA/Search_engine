# Search Engine

## Русский

Я разрабатываю поисковую систему для текстовых файлов на C++20. Индексатор будет сохранять частоты слов в PostgreSQL, а поисковый сервер будет ранжировать документы по суммарной частоте слов запроса.

Реализованы обработка текста UTF-8, чтение INI, обход каталогов и многопоточный индексатор PostgreSQL. Для сборки нужны CMake 3.20+, компилятор C++20, ICU, pkg-config, libpqxx 7.x и Boost 1.74+.

## English

I am building a C++20 search engine for text files. The indexer will store word frequencies in PostgreSQL, and the search server will rank documents by the total frequency of the query terms.

UTF-8 processing, INI configuration, directory scanning, and a parallel PostgreSQL indexer are implemented. Building requires CMake 3.20+, a C++20 compiler, ICU, pkg-config, libpqxx 7.x, and Boost 1.74+.

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

Индексатор распределяет чтение и подсчёт слов между потоками. Число потоков
зависит от числа файлов и доступных ядер, максимум восемь. При ошибке чтения
прежний индекс сохраняется. Частоты всех документов хранятся в памяти до записи.

The indexer reads files and counts words in parallel using up to eight workers,
based on the number of files and available CPUs. A read failure preserves the
previous index. Document word counts stay in memory until the database write.

## Поиск / Search

После индексации запустите поиск и вводите запросы по одному на строку.
Для выхода нажмите Ctrl+D. Для HTTP-режима задайте порт в необязательной
секции `[search]` (по умолчанию 8080). Сервер принимает запросы на локальном
адресе 127.0.0.1 и обрабатывает по одному соединению за раз.

After indexing, start the console and enter one query per line. Press Ctrl+D
to exit. For HTTP mode, set the port in the optional `[search]` section
(default 8080). The server listens on 127.0.0.1 and handles one connection at a time.

```sh
./build/search_query config.ini
./build/search_query config.ini --http
curl --data-binary 'привет мир' http://127.0.0.1:8080/search
```

Введите от одного до четырёх слов после нормализации. Повторы учитываются
один раз. В выдачу попадают до десяти файлов со всеми словами запроса.
Результаты отсортированы по убыванию суммы частот, при равенстве — по пути.
В каждой строке выводятся оценка и путь через табуляцию. HTTP принимает
UTF-8 в теле `POST /search` размером до 4096 байт.

Enter one to four words after normalization. Repeated terms count once.
The result includes up to ten files containing every search term, sorted by
total frequency in descending order, then by path. Each line contains a score
and a tab-separated path. Send a UTF-8 body of up to 4096 bytes to
`POST /search`.

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
