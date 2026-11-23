# TUXIRO WARP ME

Herramienta VST3 + Standalone (Windows y macOS) que hace lo mismo que *Warp Me*:
encuentra **cada beat y "el 1"** de cualquier tema (aunque el tempo se mueva) y lo deja sobre una
**rejilla constante**, lista para Ableton Live (o cualquier DAW).

## Qué hace
- Carga WAV / AIFF / FLAC / MP3 / OGG (hasta 30 min) arrastrando el archivo o con **OPEN**.
- Analiza: tempo, un marcador por beat, posición del "1", deriva (steady / drifting…) y compases dudosos (**CHECK → NEXT**).
- Preview con metrónomo sobre la rejilla (para comprobar de oído) y vista **ORIGINAL** (tempo fijo) vs **WARPED** (rejilla real).
- Flechas `< 1` / `1 >` para mover el "1" un beat si no cae donde toca.
- **EXPORT AUDIO**: WAV/AIFF 24-bit a tempo constante (el BPM detectado, editable haciendo clic en el número),
  empezando en el 1, tono conservado (WSOLA). Se guarda en `Música/TUXIRO WARP ME` (cambiable en **SET**).
- **DRAG LAST EXPORT INTO LIVE**: arrastra el último audio exportado directamente a Ableton.

## Cómo usarlo en Ableton
1. Pon TUXIRO WARP ME en una pista de audio, abre un tema y pulsa **EXPORT AUDIO**.
2. Arrastra el archivo exportado a una pista y pon el tempo del set al BPM mostrado (el audio queda en rejilla, con el 1 en el compás 1).

## Limitaciones (versión 1.0)
- Un VST3 no puede escribir warp markers dentro de los clips de Live ni abrir sets; por eso no hay "SEND TO ABLETON" / "SAVE LIVE SET". El flujo es exportar audio en rejilla.
- La detección de beats es por análisis de señal (no red neuronal): muy fiable en house/techno/disco/funk con ritmo claro; en grabaciones muy libres usa CHECK y las flechas del "1".
- El "1" se elige por energía de kick/bajo: en 4x4 muy uniformes puede caer un beat desplazado (usa `< 1` / `1 >`).

## Subir a GitHub y compilar
```bash
git init
git add .
git commit -m "TUXIRO WARP ME 1.0"
git branch -M main
git remote add origin https://github.com/TU_USUARIO/TuxiroWarpMe.git
git push -u origin main
```
Al hacer push, **GitHub Actions** compila Windows y macOS (pestaña *Actions* → último run → *Artifacts*).
Para publicar una release con los zips: `git tag v1.0.0 && git push origin v1.0.0`.

## Instalar
- Windows: copia `TUXIRO WARP ME.vst3` a `C:\Program Files\Common Files\VST3\`.
- macOS: copia a `~/Library/Audio/Plug-Ins/VST3/` y quita la cuarentena: `xattr -cr "~/Library/Audio/Plug-Ins/VST3/TUXIRO WARP ME.vst3"`.
- Reescanea plugins en Ableton (Preferencias → Plug-ins → activar VST3).

## Compilar en local
`cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --config Release`
Test del motor sin JUCE: `g++ -O2 -std=c++17 Tests/CoreTest.cpp Source/BeatAnalyzer.cpp Source/TimeStretch.cpp -o coretest && ./coretest`

## Licencia / aviso
Usa JUCE (AGPLv3 o licencia comercial): si distribuyes los binarios, publica el código (AGPL) o compra licencia JUCE.
Proyecto independiente, sin código de Warp Me; "Warp Me" es marca de Lessons in Live: si vas a vender o distribuir el plugin, considera otro nombre.
