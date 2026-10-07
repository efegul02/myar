# Archive format (version 2)

All integers are **little-endian**. There is no padding anywhere.

```
+--------------------+
| Archive header     |  12 bytes
+--------------------+
| Entry header 1     |  268 bytes
| File data 1        |  `size` bytes
+--------------------+
| Entry header 2     |
| File data 2        |
+--------------------+
| ...                |
```

## Archive header (12 bytes)

| Offset | Size | Field        | Description                         |
|-------:|-----:|--------------|-------------------------------------|
| 0      | 4    | `magic`      | ASCII `MYAR`                        |
| 4      | 4    | `version`    | `uint32`, currently `2`             |
| 8      | 4    | `file_count` | `uint32`, number of entries         |

## Entry header (268 bytes)

| Offset | Size | Field   | Description                                              |
|-------:|-----:|---------|----------------------------------------------------------|
| 0      | 256  | `name`  | File name, NUL-terminated, unused bytes are `0`          |
| 256    | 8    | `size`  | `uint64`, length of the data that follows                |
| 264    | 4    | `crc32` | `uint32`, CRC-32 (IEEE 802.3) of the data                |

## Reading algorithm

1. Read 12 bytes, check `magic` and `version`.
2. Repeat `file_count` times:
   1. Read 268 bytes, force `name[255] = 0`, reject names containing `/`, `\`, or equal to `.` / `..`.
   2. Read exactly `size` bytes, compute the CRC-32, compare with `crc32`.

## Changes from version 1

- Explicit little-endian serialisation instead of `fwrite` of a raw struct.
- Added the per-file `crc32` field.
