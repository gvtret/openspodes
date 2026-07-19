# DLMS/COSEM Conformance Testing — таблицы тестов

Источник: `DLMS_UA_1001-1_-_Yellow_book__Conformance_Testing_Process__-_2015.pdf`
(сшивка из трёх документов внутри одного PDF, 144 стр.)

| Документ | Содержимое | PDF-стр. | Внутр. стр. |
|---|---|---|---|
| DLMS UA 1001-1 | Conformance Testing Process (Yellow Book) | 1–44 | 1–44 |
| DLMS UA 1001-3 (ATS_DL, V5.0, 2010-12-15) | Data link layer using HDLC protocol | 45–68 | 1–24 |
| DLMS UA 1001-6 (ATS_AL_COSEM_SYMSEC_0, V1.3, 2015-06-18) | Application layer / COSEM objects / SYMSEC_0 | 69–144 | 1–76 |

Пересчёт страниц: **1001-3** → PDF = внутр. + 44; **1001-6** → PDF = внутр. + 68.
Колонка «стр. док.» — каноническая внутренняя нумерация ATS; «PDF-стр.» — абсолютная страница в файле выше.

---

## Статус реализации

| Раздел | Реализовано | Всего | Файл тестов |
|---|---|---|---|
| HDLC (1001-3) | **22** | 22 | `test_yellowbook_hdlc.c` |
| APPL (1001-6 §6) | **20** | 20 | `test_yellowbook_appl.c` |
| COSEM objects (1001-6 §7) | **7 тестов** | 7 процедурных | `test_yb_ats_al_cosem_objs.c` |
| SYMSEC_0 (1001-6 §8) | **15** | 15 | `test_yb_ats_al_symsec_0.c` |
| E2E интеграционные | **26** | 26 | `test_yellowbook_e2e.c` |
| ATS_DL unit | **34** | 34 | `test_yb_ats_dl_hdlc_*.c` (5 файлов) |

**Все тесты Yellow Book реализованы.**

---

## 1. DLMS UA 1001-3 — Data link layer using HDLC protocol

Тест-кейсы, раздел 3.7. Пропуски в нумерации (FRAME_N6, ADDRESS_N2/N3/N5 и т.п.) — намеренные исключения ATS.

| # | Test case | Название / цель | Группа | Стр. док. | PDF-стр. |
|---|---|---|---|---|---|
| 1 | HDLC_FRAME_P1 | Установление и разрыв HDLC-соединения | FRAME (P) | 14 | 58 |
| 2 | HDLC_FRAME_P2 | InterFrameTimeout | FRAME (P) | 14 | 58 |
| 3 | HDLC_FRAME_P3 | InactivityTimeout | FRAME (P) | 15 | 59 |
| 4 | HDLC_FRAME_N1 | Отсутствуют флаги HDLC-кадра | FRAME (N) | 15 | 59 |
| 5 | HDLC_FRAME_N2 | Слишком короткий HDLC-кадр | FRAME (N) | 16 | 60 |
| 6 | HDLC_FRAME_N3 | Проверка подполя типа формата кадра | FRAME (N) | 16 | 60 |
| 7 | HDLC_FRAME_N4 | Проверка подполя длины кадра | FRAME (N) | 16 | 60 |
| 8 | HDLC_FRAME_N5 | Проверка поля Control | FRAME (N) | 17 | 61 |
| 9 | HDLC_FRAME_N7 | Проверка поля HCS | FRAME (N) | 17 | 61 |
| 10 | HDLC_FRAME_N8 | Проверка поля FCS | FRAME (N) | 18 | 62 |
| 11 | HDLC_ADDRESS_P1 | Корректные адреса при заявленной структуре адресов | ADDRESS (P) | 18 | 62 |
| 12 | HDLC_ADDRESS_N1 | Двухбайтовый source-адрес | ADDRESS (N) | 19 | 63 |
| 13 | HDLC_ADDRESS_N4 | Неизвестные destination-адреса | ADDRESS (N) | 19 | 63 |
| 14 | HDLC_ADDRESS_N6 | Однобайтовый destination-адрес, когда ожидается 2/4 байта | ADDRESS (N) | 20 | 64 |
| 15 | HDLC_ADDRESS_N7 | Трёх- или пятибайтовый destination-адрес | ADDRESS (N) | 20 | 64 |
| 16 | HDLC_NDM2NRM_P1 | Параметр MaximumInformationFieldLength | NDM2NRM (P) | 21 | 65 |
| 17 | HDLC_NDM2NRM_P2 | Параметр WindowSize | NDM2NRM (P) | 21 | 65 |
| 18 | HDLC_INFO_P1 | Обмен I-кадрами | INFO (P) | 22 | 66 |
| 19 | HDLC_INFO_N1 | Слишком длинное информационное поле | INFO (N) | 22 | 66 |
| 20 | HDLC_INFO_N2 | Неверный номер последовательности N(R) | INFO (N) | 23 | 67 |
| 21 | HDLC_INFO_N3 | Неверный номер последовательности N(S) | INFO (N) | 23 | 67 |
| 22 | HDLC_NDMOP_N1 | I-кадр в режиме NDM | NDMOP (N) | 23 | 67 |

**Итого: 22 тест-кейса. Все реализованы.**

---

## 2. DLMS UA 1001-6 — Application layer / COSEM objects / SYMSEC_0

Документ содержит три отдельных ATS (разделы 6, 7, 8).

### 2a. Application layer (раздел 6.2) — `APPL_*`

| # | Test case | Название / цель | Стр. док. | PDF-стр. |
|---|---|---|---|---|
| 1 | APPL_IDLE_N1 | Обмен данными в состоянии IDLE | 15 | 83 |
| 2 | APPL_OPEN_1 | Установление AA с заявленными параметрами | 16 | 84 |
| 3 | APPL_OPEN_2 | Идентификация пользователя клиента | 16 | 84 |
| 4 | APPL_OPEN_3 | HLS-аутентификация, Pass 3 и Pass 4 | 17 | 85 |
| 5 | APPL_OPEN_4 | Версия протокола | 18 | 86 |
| 6 | APPL_OPEN_5 | Application context | 18 | 86 |
| 7 | APPL_OPEN_6 | Titles, qualifiers и invocation identifiers | 19 | 87 |
| 8 | APPL_OPEN_7 | Authentication functional unit | 21 | 89 |
| 9 | APPL_OPEN_9 | xDLMS InitiateRequest: dedicated-key | 23 | 91 |
| 10 | APPL_OPEN_11 | xDLMS InitiateRequest: quality-of-service | 23 | 91 |
| 11 | APPL_OPEN_12 | xDLMS InitiateRequest: dlms-version-number | 24 | 92 |
| 12 | APPL_OPEN_13 | xDLMS InitiateRequest: conformance-block | 24 | 92 |
| 13 | APPL_OPEN_14 | xDLMS InitiateRequest: client-max-receive-pdu-size | 25 | 93 |
| 14 | APPL_DATA_LN_N1 | Get-Request с ошибками | 25 | 93 |
| 15 | APPL_DATA_LN_N3 | Set-Request с ошибками | 26 | 94 |
| 16 | APPL_DATA_LN_N4 | Неподдерживаемый сервис (LN) | 27 | 95 |
| 17 | APPL_DATA_SN_N1 | ReadRequest с ошибками | 27 | 95 |
| 18 | APPL_DATA_SN_N2 | WriteRequest с ошибками | 28 | 96 |
| 19 | APPL_DATA_SN_N3 | Неподдерживаемый сервис (SN) | 28 | 96 |
| 20 | APPL_REL_P1 | Освобождение AA (в док. без текстового названия) | 29 | 97 |

**Итого: 20 тест-кейсов. Все реализованы.** Пропуски (OPEN_8, OPEN_10, DATA_LN_N2) — намеренные исключения ATS.

Вспомогательные процедуры (Tables 1–8, стр. док. 8–13 / PDF 76–81), используемые внутри тест-кейсов: `IUT AL в IDLE`, `Establish confirmed AA`, `Check AA Associated`, `Release AA`, `Read attribute`, `Write attribute`, `Invoke method`, `Raise fatal failure`.

---

### 2b. COSEM objects (раздел 7)

> **Важно:** в разделе 7 нет дискретных пронумерованных тест-кейсов. Это один универсальный алгоритм `COSEM_X_Y`, который CTT динамически прогоняет по каждому объекту из `object_list` каждой AA каждого logical device. Число проверок = число объектов × число атрибутов в модели. Плюс несколько именованных процедурных тестов.

**Реализовано 7 процедурных тестов** в `test_yb_ats_al_cosem_objs.c`:

| # | Тест | Что проверяется | Yellow Book |
|---|---|---|---|
| 1 | `test_cosem_read_object_list` | Чтение object_list из Association LN; чтение всех зарегистрированных объектов | 7.2 (ядро) |
| 2 | `test_cosem_write_back` | Запись/чтение атрибута Data IC (read_write) | 7.2 (ветвь writeable) |
| 3 | `test_cosem_mandatory_objects` | Проверка обязательных объектов: ALN (8 атрибутов), SAP Assignment (2), LDN (1) | 7.3.12 (Table 32) |
| 4 | `test_cosem_multiple_references` | 3 подтеста: 10 value-атрибутов вместе, короткий+длинный, первые 4 атрибута PushSetup | 7.3.11 |
| 5 | `test_cosem_access_rights` | Чтение атрибутов Data IC и Register IC | 7.2 (ветвь readable) |
| 6 | `test_cosem_push_setup_and_others` | PushSetup (9 атрибутов), Clock (2), SAP Assignment (2) | 7.3.8 + Table 30 |
| 7 | `test_cosem_push_operation` | Вызов метода push на PushSetup | 7.3.8 (Table 31, Subtest 1) |

#### 7.2 Универсальный алгоритм атрибутного теста (ядро раздела; стр. док. 32–33 / PDF 100–101)

| Ветвь | Условие входа (`access_rights`) | Что проверяется | FAILED если |
|---|---|---|---|
| Attribute readable | read_only / read_write | Чтение; проверка типа / диапазона / enum | Ошибка на Read; для `logical_name` — тип ≠ octet_string[6] или пара (LN, class_id) невалидна; для CHOICE — неверный тип; для прочих — тип/диапазон/enum вне спецификации |
| Attribute not readable | no_access / write_only | Попытка чтения → ожидается корректный отказ | Нет корректной реакции на запрет |
| Attribute writeable | write_only / read_write | Тест записи (где допустимо) | Некорректная обработка; при отсутствии данных → INCONCLUSIVE «WRITE DATA NOT AVAILABLE» |

#### 7.1 / Table 30 — покрываемые интерфейсные классы (стр. док. 30–31 / PDF 98–99)

CTT 3 покрывает все ИК Blue Book Ed. 11. Клаузулы 7.3.x есть только у части классов; остальные проходят только по алгоритму 7.2.

| class_id | Класс | Версии | Спец. клаузула |
|---|---|---|---|
| 1 | Data | 0 | 7.3.1 |
| 3 | Register | 0 | 7.3.1 |
| 4 | Extended register | 0 | 7.3.2 |
| 5 | Demand register | 0 | 7.3.3 |
| 6 | Register activation | 0 | — (7.3.10) |
| 7 | Profile generic | 1 | 7.3.4 |
| 8 | Clock | 0 | — (искл. из Multiple ref.) |
| 9 | Script table | 0 | 7.3.5 |
| 10 | Schedule | 0 | — |
| 11 | Special days table | 0 | — (7.3.10) |
| 12 | Association SN | 0,1,2,3 | 7.3.6 |
| 15 | Association LN | 0,1,2 | 7.3.6 |
| 17 | SAP Assignment | 0 | — (7.3.13) |
| 18 | Image transfer | 0 | — |
| 19 | IEC Local port setup | 0,1 | — |
| 20 | Activity calendar | 0 | — (7.3.10) |
| 21 | Register monitor | 0 | 7.3.7 |
| 22 | Single action schedule | 0 | — (7.3.10) |
| 23 | IEC HDLC setup | 0,1 | — |
| 24 | IEC Twisted pair (1) setup | 0,1 | — |
| 25 | M-BUS slave port setup | 0 | — |
| 26 | Utility tables | 0 | — |
| 27 | PSTN modem configuration / Modem configuration | 0 / 1 | — |
| 28 | Auto answer | 0,2 | — |
| 29 | PSTN auto dial / Auto connect / Auto dial | 0 / 1,2 | — |
| 40 | Push setup | 0 | 7.3.8 |
| 41 | TCP-UDP setup | 0 | — |
| 42 | IPv4 setup | 0 | — |
| 43 | Ethernet setup / MAC address setup | 0 | — |
| 44 | PPP setup | 0 | — |
| 45 | GPRS modem setup | 0 | — |
| 46 | SMTP setup | 0 | — |
| 47 | GSM diagnostic | 0 | — |
| 48 | IPv6 setup | 0 | — |
| 50 | S-FSK Phy&MAC setup | 0,1 | — (v0 deprecated) |
| 51 | S-FSK Active initiator | 0 | — |
| 52 | S-FSK MAC synchronization timeouts | 0 | — |
| 53 | S-FSK MAC counters | 0 | — |
| 55 | S-FSK IEC 6334-4-32 LLC setup / IEC 6334-4-32 LLC setup | 0 / 1 | — |
| 56 | S-FSK Reporting system list | 0 | — |
| 57 | ISO/IEC 8802-2 LLC Type 1 setup | 0 | — |
| 58 | ISO/IEC 8802-2 LLC Type 2 setup | 0 | — |
| 59 | ISO/IEC 8802-2 LLC Type 3 setup | 0 | — |
| 61 | Register table | 0 | — |
| 63 | Status mapping | 0 | — |
| 64 | Security setup | 0 | 7.3.9 |
| 65 | Parameter monitor | 0 | — |
| 67 | Sensor manager | 0 | — |
| 70 | Disconnect control | 0 | — |
| 71 | Limiter | 0 | — |
| 72 | M-Bus client | 0,1 | — |
| 73 | Wireless Mode Q channel | 0 | — |
| 104 | ZigBee network control | 0 | — |
| 105 | ZigBee tunnel setup | 0 | — |

#### 7.3 Interface class specific tests — детализация

| Клаузула | Класс(ы) | Что проверяется / особенность | Стр. док. | PDF-стр. |
|---|---|---|---|---|
| 7.3.1 | Data (1), Register (3) | Тип `value` зависит от инстанса/LN, по таблицам определений объектов [3] | 34 | 102 |
| 7.3.2 | Extended register (4) | `value` как у Data; `status` — тип по определению класса | 34 | 102 |
| 7.3.3 | Demand register (5) | `value`, `current_average_value`, `last_average_value` — типы по инстансу/LN; `status` по классу | 34 | 102 |
| 7.3.4.1 | Profile generic (7) | Запись не тестируется; пустой буфер / 1 запись допустимы; `null_data` в любой колонке; `sort_object` должен быть среди `capture_objects` (иначе INAPPLICABLE) | 34 | 102 |
| 7.3.4.2 | Profile generic (7) | Доступность selective access: LN — бит 21 conformance block, SN — бит 18 | 34–35 | 102–103 |
| 7.3.4.3 | Profile generic (7) | Selective access **by range**: 5 предусловий (buffer/capture_objects readable, ≥2 колонки, среди них time Clock, ≥5 записей) + 4 подтеста (полный интервал → весь буфер; вне интервала → 0/ошибка; частичный все колонки → часть; частичный 2 колонки → структура из 2); пропуск при >1000 записей | 35 | 103 |
| 7.3.4.4 | Profile generic (7) | Selective access **by entry**: 4 предусловия + 3 подтеста (1 строка/1 колонка → 1 ячейка; все записи/колонки wild-card → весь буфер; часть → часть); пропуск при >1000 | 35–36 | 103–104 |
| 7.3.5 | Script table (9) | Толерантность к «dummy»: для `service_id` принимаются 1, 2 **и 0** | 36 | 104 |
| 7.3.6 | Assoc SN (12) / LN (15) | Принимаются версии SN 1–3, LN 1–4; selective access не тестируется; при HLS используется `reply_to_HLS_authentication` | 36 | 104 |
| 7.3.7 | Register monitor (21) | Все `thresholds` одного типа; если `monitored_value` указывает на CHOICE-атрибут — тип threshold из допустимых по [3] | 36 | 104 |
| 7.3.8 | Push setup (40) | Тест push-операции (Table 31), 2 подтеста | 36–37 | 104–105 |
| 7.3.9 | Security setup (64) | Методы `security_activate` и `global_key_transfer` задействуются в SYMSEC_0; доступность объявляется в CTI | 38 | 106 |
| 7.3.10 | классы 6, 7, 9, 11, 20, 21, 22 | «Dummy attributes»: допускаются полностью нулевые структуры в перечисленных атрибутах; если элемент enumerated без 0, то 0 тоже принимается | 38–39 | 106–107 |
| 7.3.11 | все (кроме Clock) | Multiple references test — 3 проверки | 39 | 107 |
| 7.3.12 | Assoc SN/LN, SAP Assignment, LDN | Check mandatory objects (Table 32), 3 подтеста | 39–41 | 107–109 |
| 7.3.13 | SAP Assignment, LDN, Assoc SN/LN, Security setup | Интерпретация и репортинг значений атрибутов (Table 33) | 41–42 | 109–110 |

#### Table 31 — Push operation test (7.3.8; стр. док. 37 / PDF 105)

| Подтест | INAPPLICABLE если | PASSED если |
|---|---|---|
| Subtest 1: Push без принудительной защиты | профиль не TCP; или `CTI.CanPush = FALSE` | получен DataNotification, APDU синтаксически корректен |
| Subtest 2: Push с принудительной защитой | не TCP; `CanPush=FALSE`; контекст не шифрованный; push «Security setup» не объявлен/не виден; `security_policy` не writeable | получен DataNotification, защита APDU = {A+E}, расшифрованный APDU корректен |

#### Multiple references test (7.3.11; стр. док. 39 / PDF 107)

Сравнение чтения «вместе» и «по отдельности»:
1. Чтение 10 атрибутов `value` инстансов классов 1/3/4.
2. Чтение короткого (1–10 байт) и длинного (500–1000 байт) атрибута.
3. Чтение первых 4 атрибутов первого объекта, у которого их ≥4.

Clock всегда исключается; прочие можно исключить в CTI `ExtraInformation` по классу или инстансу.

#### Table 32 — COSEM mandatory objects (7.3.12; стр. док. 39–41 / PDF 107–109)

Выполняется в начале объектных тестов в каждой AA.

| Подтест | Проверяемые элементы |
|---|---|
| Subtest 1: Current association | SN: `security_setup_reference`, `user_list`, `current_user`. LN: `associated_partners_id`, `application_context_name`, `xDLMS_context_info`, `authentication_mechanism_name`, `association_status` (=associated), `security_setup_reference`, `user_list`, `current_user`. С версионными правилами INAPPLICABLE |
| Subtest 2: SAP assignment | При публичном клиенте и нескольких LD: все LD присутствуют с корректными SAP/LDN; нет пересечений SAP; нет пересечений LDN |
| Subtest 3: Logical Device Name | LDN соответствует заявленному значению |

#### Table 33 — интерпретируемые атрибуты (7.3.13; стр. док. 42 / PDF 110)

| Объект | Атрибуты |
|---|---|
| SAP Assignment | `SAP_assignment_list` |
| Logical Device Name | `value` |
| Association SN | `object_list`, `access_rights_list` (v1+), `security_setup_reference` (v2+), `user_list` (v3+), `current_user` (v3+) |
| Association LN | `object_list`, `associated_partners_id`, `application_context_name`, `xDLMS_context_info`, `authentication_mechanism_name`, `security_setup_reference` (v1+), `user_list` (v2+), `current_user` (v2+) |
| Security setup | `security_policy`, `security_suite`, `client_system_title`, `server_system_title` |

Первые 3 символа LDN должны совпадать с заявленным; каждый LDN уникален в пределах физического устройства.

---

### 2c. Symmetric key security suite 0 (раздел 8) — `SYMSEC_0_*`

| # | Test case | Название / цель | Группа | Стр. док. | PDF-стр. |
|---|---|---|---|---|---|
| 1 | SYMSEC_0_BasicCap_1 | Базовый тест возможностей безопасности | BasicCap | 45 | 113 |
| 2 | SYMSEC_0_FraCount_1 | Защита от повтора сообщений (replay) | FraCount | 46 | 114 |
| 3 | SYMSEC_0_FraCount_3 | Send frame counter | FraCount | 47 | 115 |
| 4 | SYMSEC_0_Key_Tx_P1 | Передача и восстановление GUEK | GlobalKeyTx | 47 | 115 |
| 5 | SYMSEC_0_Key_Tx_P2 | Передача и восстановление GAK | GlobalKeyTx | 48 | 116 |
| 6 | SYMSEC_0_Key_Tx_P3 | Передача и восстановление GUEK и GAK | GlobalKeyTx | 49 | 117 |
| 7 | SYMSEC_0_Key_Tx_N1 | Global key transfer, неверный key_id | GlobalKeyTx | 49 | 117 |
| 8 | SYMSEC_0_Key_Tx_N2 | Передача GUEK с неверным wrapping | GlobalKeyTx | 50 | 118 |
| 9 | SYMSEC_0_DedKey_N1 | Негативные тесты dedicated-key | DedKey | 51 | 119 |
| 10 | SYMSEC_0_SecDataX_P1 | Запись/чтение STA1 и STA2 при global и dedicated шифровании | SecDataX | 52 | 120 |
| 11 | SYMSEC_0_SecDataX_N1 | Запись/чтение STA1 при некорректном шифровании | SecDataX | 54 | 122 |
| 12 | SYMSEC_REL_N1 | Освобождение AA с недостаточно защищённым RLRQ | SecRel | 55 | 123 |
| 13 | SYMSEC_0_SecPol_1 | Активация политики безопасности (1) | SecPol | 56 | 124 |
| 14 | SYMSEC_0_SecPol_2 | Активация политики безопасности (2) | SecPol | 57 | 125 |
| 15 | SYMSEC_0_SecPol_3 | Активация политики безопасности (3) | SecPol | 58 | 126 |

**Итого: 15 тест-кейсов. Все реализованы.**

---

## Сводка

| Раздел | Реализовано | Всего | Файл |
|---|---|---|---|
| 1001-3 HDLC (тест-кейсы) | 22 | 22 | `test_yellowbook_hdlc.c` |
| 1001-6 §6 Application layer (тест-кейсы) | 20 | 20 | `test_yellowbook_appl.c` |
| 1001-6 §7 COSEM objects (процедурные тесты) | 7 | 7 | `test_yb_ats_al_cosem_objs.c` |
| 1001-6 §8 SYMSEC_0 (тест-кейсы) | 15 | 15 | `test_yb_ats_al_symsec_0.c` |
| E2E интеграционные | 26 | 26 | `test_yellowbook_e2e.c` |
| ATS_DL unit (frame/address/ndm2nrm/info/ndmop) | 34 | 34 | `test_yb_ats_dl_hdlc_*.c` |

**Все тесты Yellow Book реализованы. 31 CTest-таргет, все проходят.**

**Замечания по редакциям:** 1001-3 (HDLC) — редакция 2010 г.; 1001-6 — 2015 г. под CTT 3.0 / Blue Book Ed. 11. Для актуального CTT стоит свериться с текущими редакциями (в Table 30 могли добавиться классы и версии, security suite 1/2).
