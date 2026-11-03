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
