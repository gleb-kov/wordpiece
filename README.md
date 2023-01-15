# WordPiece

Репозиторий содержит два самых быстрых алгоритма токенизации WordPiece.

1) Linear. Оптимальная асимптотика по времени -- $O(n)$, память -- $O(n)$ (с константой около 12-20), реализация эффективнее, чем в статье "Fast WordPiece" (2021, torch). Многопоточная реализация.

2) Fast. Асимптотика -- $O(nm)$, где $m$ -- максимальная длина слова в словаре. Многопоточная реализация, на практике эффективнее, чем другие реализации минимум в 2 раза.

## Linear Algorithm

Возьмем строку s#t1#t2#...tk, построим на ней суфмас. Решетки обязательно надо вставить (символ, который не встречается).

Наша задача для каждого суффикса s найти самую длинную ti которая равна префиксу этого суффикса
в суфмасе есть k интересных мест, там где начинаются ti, у каждой есть длина |ti|
хотим для всех позиций суфмаса найти самую длинную хорошую интересную позицию, такую что лцп до нее хотя бы длина этой позиции (такая интересная позиция будет называться хорошей, то есть та, до которой лцп хотя бы длина)
для этого найдем самую длинную интересную хорошую позицию слева и справа для каждой позиции в суфмасе независимо, дальше возьмем бОльшую из двух
ищем слева, справа аналогично. пусть есть две интересных позиции в суфмасе с индексами i<j, причем |i| >= |j|. В предположении что все ti различны, можно утверждать, что i уже никогда не будет хорошей для всех позиций суфмаса >j, поскольку лцп (i;j) строго меньше |i| (если |i| > |j|, то она просто не больше |j|, а если равны, то так как строки различные, то строго меньше). Это означает, что если мы фиксируем позицию суфмаса e, то среди интересных позиций слева от e нас интересуют только рекорды, то есть только убывающий стэк.
алгоритм: идем сканлайном слева направо, храним стэк хороших интересных позиций i1, i2, .. in, причем |i1| < |i2| < ... < |in|, изначально стэк пустой. пришли в очередную позицию e, пусть lcp(e-1, e) = x. Тогда надо выкинуть все элементы с конца стэка, что |in| > x (очевидно они стали плохими позициями). Теперь если e сама является хорошей позицией, то надо ее добавить в стэк. Очевидно что lcp(e-1, e) <= |e|, так что e будет максимумом в стэке, и инвариант возрастания сохраняется. в очередной позиции e самая длинная интересная хорошая справа позиция это in (потому что в стэке все хорошие позиции и только они).

## Fast Algorithm

Стоя на позиции i возьмем подстроку [i, i + m). Проверим ее наличие в словаре-хешмапе. Если нашлось совпадение, то сохраним токен в ответ и сдвинем позицию. Если совпадение не нашлось, но уберем последний символ из подстроки. Повторяем пока подстрока не пуста. Если повторы дошли до пустой подстроки, то добавим UNK в ответ и сдвинем позицию до начала следующего слова.

## Roadmap

1. довести maxmatch до wordpiece
2. интеграция в youtokentome
3. скрипт бенчмарков как в youtokentome
4. таблицы бенчмарков для 4х языков и размеры 500,1000МБ

## Benchmarks

### 10MB

```
fast: 0.3 sec
linear: 0.8 sec
torch: 2.5 sec
tensorflow: 4.0 sec
hugging face: 5.0 sec
```

### 100MB

```
fast: 2.3 sec
linear: 10.6 sec
torch: 24.4 sec
tensorflow: 40.1 sec
hugging face: 55.0 sec
```

### Machine

Бенчмарк запускался на:
- OS: Ubuntu 22.04;
- RAM: 16GB;
- CPU: 11th Gen Intel Core i5-11400H @ 2.70GHz x12;
- SSD: SK hynix PC711 256GB (SeqRead 2300 MB/s, SeqWrite 1320 MB/s, RndRead 36mcs, RndWrite 86mcs);

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && make -C build
```

To build with OpenMP: `-DCMAKE_USE_OPENMP=On`, Sanitizers: `-DCMAKE_USE_SANITIZERS`;

### Tests

Тесты расположены в файле `tests/tests.cpp`, результаты алгортмов сравниваются между собой.

### Prepare benchmark

```bash
apt install wget bzip2 perl cmake make
mkdir -p data
wget -O data/vocab.txt https://huggingface.co/bert-base-uncased/resolve/main/vocab.txt
wget -O data/enwiki.xml.bz2 https://www.dropbox.com/s/cnrhd11zdtc1pic/enwiki-20181001-corpus.xml.bz2?dl=1
wget -O data/xml2txt.pl https://www.dropbox.com/s/p3ta9spzfviovk0/xml2txt.pl
bzip2 -kdc data/enwiki.xml.bz2 > data/enwiki.xml
perl data/xml2txt.pl -nomath -notables data/enwiki.xml data/enwiki.txt
python3 -m venv venv && source venv/bin/activate
pip3 install -r tests/requirements.txt
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

For ARM arch use `tests/requirements-arm.txt` (no tensorflow).

### Run benchmark

```bash
source venv/bin/activate && make -C build && python3 tests/benchmark.py data/enwiki.txt data/vocab.txt 100
```
