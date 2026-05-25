# Patches RNBO

Salva qui i file `.maxpat` del tuo progetto RNBO (solo riferimento al sorgente Max).

Dopo ogni modifica alla patch, riesporta il codice C++ nella cartella `export/` come descritto in [README.md](../README.md).

Il build si aspetta per default il file `export/rnbo_source.cpp`. Se usi un altro nome di export, imposta `RNBO_CLASS_FILE_NAME` in CMake.
