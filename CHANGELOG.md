# Changelog

Все значимые изменения этого проекта документируются в этом файле.

Формат основывается на [Keep a Changelog](https://keepachangelog.com/), проект следует [Semantic Versioning](https://semver.org/lang/ru/).

## [1.0.0] - 2026-07-11

### Добавлено

**Транспорт:**
- HDLC session layer: SNRM/UA с XID параметрами, N(S)/N(R) sequence tracking, LLC (`E6 E6 00`/`E6 E7 00`), DISC/DM
- Serial транспорт (UART/RS-85): 8N1, настраиваемый baud, RTS control
- Примеры: serial client/server с полным DLMS flow через HDLC

**Кодек:**
- Selective access encode/decode для ProfileGeneric (by date, by entry)
- `osp_client_get_with_selective_access()` — клиентская функция с фильтрацией

**Сервис:**
- Серверная фильтрация буфера ProfileGeneric по selective access
- GET request encode/decode теперь используют selective access параметры

**Примеры:**
- `linux_hal` — полный Linux HAL (TCP, OpenSSL, timer, random)
- `spodus_demo` — СПОДУС концентратор на 3 ПУ

**Документация:**
- `docs/ARCHITECTURE.md` — полное описание архитектуры
- Doxygen комментарии для IC классов: Data, Register, ProfileGeneric, Clock, AssociationLN, SAPAssignment, PushSetup, SecuritySetup, DisconnectControl
- `examples/serial_transport.h/.c` — документированный Serial транспорт

**Инфраструктура:**
- ASAN + UBSan CI job
- Coverage report (76.4% lines, 85.7% branches)
- `ENABLE_ASAN` cmake option

### Исправлено

- **UB**: `serialize.c` — int64 overflow при сдвиге на 56 бит ( замена на uint64_t)
- **UB**: `general_ciphering.c` — null pointer memcpy на octet strings
- **Bug**: `test_spodus_concentrator` — dangling pointer (data_obj была локальной)
- **Bug**: `test_errors` — dangling pointer (g_error_data_obj была локальной)
- **ASAN false positive**: `osp_obis_eq` — добавлен `__attribute__((noinline))`
- Coverage script: исправлен путь `build-cov` вместо `build-coverage`
- Coverage script: поддержка `.o` файлов (Linux) помимо `.obj` (Windows)
- `add_compile_options` перемещён перед `add_library` для корректного покрытия

### Лицензия
- Сменена с MIT/Apache-2.0 на GPL-3.0-or-later
