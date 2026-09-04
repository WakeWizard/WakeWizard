# ESP-IDF 4.4.7 — Copyright and License Notices

WakeWizard's reviewed Arduino-ESP32 build is based on the ESP-IDF 4.4.7 generation.

Espressif states that its original ESP-IDF source code is licensed under Apache License 2.0.
ESP-IDF also contains third-party code under additional licenses, and source-file headers take
precedence over summary documentation.

The official ESP-IDF 4.4.7 copyright/license inventory identifies, among others, third-party
components under MIT, BSD-style/BSD-2-Clause/BSD-3-Clause, ISC, Boost Software License and
Apache-2.0 terms. Examples listed by Espressif include Newlib, lwIP, Mbed TLS, nghttp2, FatFS,
cJSON, libsodium, micro-ecc, SPIFFS, TinyCBOR and other components depending on configuration.

Do not interpret this file as saying that every ESP-IDF component is present in WakeWizard.
The WakeWizard linker-map review is used to distinguish linked runtime components from tools or
unused SDK content.

For authoritative terms for a given source/object, consult the corresponding ESP-IDF 4.4.7 source
header and license file.

Official ESP-IDF 4.4.7 documentation:
https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32/

Upstream source:
https://github.com/espressif/esp-idf

The full Apache License 2.0 text applicable to Espressif's Apache-licensed code is already present
in WakeWizard's root LICENSE when WakeWizard itself is distributed under Apache-2.0. Preserve any
additional third-party notices required by the particular ESP-IDF components distributed.
