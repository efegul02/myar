# myar

Birden fazla dosyayı **tek bir ikili (binary) arşiv dosyasında** birleştiren ve geri açan, C ile yazılmış küçük bir komut satırı aracı. Her dosya, **adını**, **boyutunu** ve **CRC-32 sağlama toplamını** içeren bir başlıkla saklanır.

> 🇬🇧 English version: [README.md](README.md)

## Demo

```text
$ make
cc -std=c11 -Wall -Wextra -Wpedantic -O2 -o myar src/main.c src/archive.c

$ ./myar pack backup.myar notes.txt photo.png data.csv
added: notes.txt (14 bytes)
added: photo.png (582113 bytes)
added: data.csv (12 bytes)

$ ./myar list backup.myar
        SIZE  CRC32     NAME
          14  01f43b66  notes.txt
      582113  10356ebc  photo.png
          12  bbbbb379  data.csv
3 file(s), 582139 bytes total

$ ./myar test backup.myar
OK      notes.txt
OK      photo.png
OK      data.csv

$ ./myar extract backup.myar restored/
extracted: restored/notes.txt (14 bytes)
extracted: restored/photo.png (582113 bytes)
extracted: restored/data.csv (12 bytes)
```

## Derleme ve çalıştırma

```sh
make              # ./myar dosyasını üretir
./myar --help
```

```sh
./myar pack    <arşiv> <dosya>...    # arşiv oluştur
./myar list    <arşiv>               # içeriği göster
./myar test    <arşiv>               # sadece sağlama toplamlarını doğrula
./myar extract <arşiv> [klasör]      # çıkar (var olan dosyaların üzerine yazmaz)
```

Gereksinimler: C11 derleyici ve POSIX sistem (Linux, macOS, WSL).

## Bellek güvenliği

**Valgrind ile 0 memory leak tespit edildi** (valgrind 3.22.0, `--leak-check=full --track-origins=yes --track-fds=yes`).
Tüm komutlar (`pack`, `list`, `test`, `extract`) ve hata yolları (aynı isimli dosya, kesik arşiv, geçersiz dosya, var olan dosyalar) 0 hata, sızıntısız bellek ve sızıntısız dosya tanıtıcısı ile çalışıyor.

```sh
make valgrind     # gerekli: sudo apt install valgrind
make test         # fonksiyonel testler
```

## Nasıl çalışır

### Mimari

```
src/main.c      komut satırı arayüzü (sadece argüman ayrıştırma)
src/archive.h   genel API: ar_pack / ar_list / ar_test / ar_extract
src/archive.c   arşiv formatı, serileştirme, CRC-32, dosya kopyalama
tests/          fonksiyonel testler + Valgrind betiği
docs/FORMAT.md  format şartnamesi
```

### Arşiv formatı

```
[ Arşiv başlığı: "MYAR" | versiyon | dosya_sayısı ]     12 bayt
[ Dosya başlığı: ad[256] | boyut (u64) | crc32 ]        268 bayt
[ dosya verisi ... ]                                    boyut bayt
[ Dosya başlığı ] [ dosya verisi ] ...
```

Ayrıntılı açıklama: [docs/FORMAT.md](docs/FORMAT.md)

### Algoritmalar ve tasarım kararları

- **Uzunluk ön ekli kayıtlar:** Her başlık verinin boyutunu tutar; okuyucu ayraç aramadan bir kayıttan diğerine atlar. İkili veri her bayt değerini içerebilir.
- **CRC-32 (IEEE 802.3):** Tablo tabanlı, bayt başına tek tablo erişimi. Başlık veriden önce yazıldığı için sağlama toplamı girdi dosyası üzerinde ilk geçişte hesaplanır.
- **Akışlı kopyalama:** Dosyalar 64 KiB'lık parçalarla kopyalanır; bellek kullanımı dosya boyutundan bağımsızdır ve hiç `malloc` gerekmez.
- **Açık little-endian serileştirme:** Alanlar struct yerine bayt bayt yazılır; format padding'e veya makinenin endianness'ına bağlı değildir.
- **64 bit boyut:** 4 GB'tan büyük dosyalar desteklenir.
- **Savunmacı okuma:** Magic ve versiyon kontrol edilir, giriş boyutu arşivin kalan boyutuyla karşılaştırılır, `/`, `\`, `.` ve `..` içeren adlar reddedilir, `fopen(..., "wbx")` var olan dosyanın üzerine yazmayı engeller.

## Sınırlamalar / fikirler

- Yalnızca normal dosyalar (klasör yok); dosya adı en fazla 255 bayt
- Sıkıştırma yok
- Fikirler: klasörleri özyinelemeli ekleme, değişken uzunluklu ad, tek dosya çıkarma, zlib ile sıkıştırma

## Lisans

[MIT](LICENSE)
