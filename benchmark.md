# Бенчмарки

## Машина 1

Dell Latitude 5521, 11th Gen Intel Core i5-11400H @ 2.70GHz x12, RAM 16 GB, SSD SK hynix PC711 256GB.

## Машина 2

Mac M1 pro, Apple M1 Pro chip, RAM 16 GB, SSD 512 GB.

| Tokenization 10MB   |   English   |   Russian   |  Japanese   |   Chinese   |
|---------------------|-------------|-------------|-------------|-------------|
| fast                | 1.3 (x3.2)  | 0.3 (x1.1)  |  0.1 (x1)   |  0.1 (x1)   |
| hugging face        | 4.1 (x10.4) | 5.3 (x18.5) | 4.0 (x41.9) | 4.7 (x59.3) |
| linear              |  0.4 (x1)   |  0.3 (x1)   | 0.2 (x1.8)  |  0.2 (x2)   |
| torch               | 1.1 (x2.9)  |  2.3 (x8)   | 1.3 (x13.1) | 0.9 (x11.1) |

| Tokenization 100MB   |   English    |   Russian    |   Japanese    |    Chinese    |
|----------------------|--------------|--------------|---------------|---------------|
| fast                 |   1.3 (x1)   |   2.2 (x1)   |   0.4 (x1)    |   0.3 (x1)    |
| hugging face         | 55.0 (x42.4) | 64.8 (x29.6) | 55.2 (x149.9) | 66.2 (x242.4) |
| linear               |  4.9 (x3.8)  |  3.5 (x1.6)  |  1.8 (x4.9)   |  1.7 (x6.4)   |
| torch                | 10.9 (x8.4)  | 22.5 (x10.3) | 12.2 (x33.2)  |  9.0 (x32.8)  |