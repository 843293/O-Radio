from pathlib import Path
root = Path(__file__).resolve().parents[1]
html = (root / 'web/index.html').read_text().replace('/* IMPORTER */', (root / 'web/station_import.js').read_text())
(root / 'include/web_ui.h').write_text('#pragma once\n#include <Arduino.h>\nstatic const char kWebUi[] PROGMEM = R"RADIOHTML(' + html + ')RADIOHTML";\n')
