# Бенчмарки

## Машина 1

Dell Latitude 5521, 11th Gen Intel Core i5-11400H @ 2.70GHz x12, RAM 16 GB, SSD SK hynix PC711 256GB.

## Машина 2

Mac M1 pro, Apple M1 Pro chip, RAM 16 GB, SSD 512 GB.

| Tokenization 10MB   |   English   |   Russian   |  Japanese   |   Chinese   |
|---------------------|-------------|-------------|-------------|-------------|
| fast                |  0.1 (x1)   |  0.2 (x1)   |  0.1 (x1)   |  0.1 (x1)   |
| hugging face        | 4.0 (x50.4) | 5.1 (x28.9) | 4.0 (x34.7) | 4.6 (x40.9) |
| linear              | 0.4 (x5.1)  | 0.3 (x1.6)  |  0.2 (x2)   | 0.2 (x1.7)  |
| torch               | 1.1 (x13.7) | 2.2 (x12.3) | 1.2 (x10.9) | 0.9 (x7.7)  |

| Tokenization 100MB   |   English    |   Russian    |   Japanese    |    Chinese    |
|----------------------|--------------|--------------|---------------|---------------|
| fast                 |   0.6 (x1)   |   1.1 (x1)   |   0.5 (x1)    |   0.5 (x1)    |
| hugging face         |  60.1 (x94)  | 68.0 (x59.6) | 60.8 (x125.6) | 71.1 (x132.7) |
| linear               |  5.3 (x8.3)  |  3.6 (x3.1)  |  2.2 (x4.5)   |  2.0 (x3.8)   |
| torch                | 10.7 (x16.7) | 23.0 (x20.2) | 12.9 (x26.6)  |   9.1 (x17)   |