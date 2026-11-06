# Zonium VoxMachine — Talkbox / Vocoder (Zoniumgames)

Instrumento vocoder estilo talkbox (Daft Punk / Dabeull): arrastras un vocal, tocas el teclado MIDI y el vocal "canta" las notas.

## Cómo conseguir el VST3 para Windows (sin instalar nada)
1. Sube esta carpeta a un repositorio nuevo en GitHub (privado sirve).
2. Pestaña **Actions → Build Windows VST3 → Run workflow** (también corre solo al hacer push).
3. Al terminar (~10 min), descarga el artefacto **ZoniumVoxMachine-Windows-x64** (contiene el .vst3 y el Standalone).

Alternativa local: Visual Studio 2022 + CMake →
`cmake -B build -A x64` y `cmake --build build --config Release`.
(La primera configuración descarga JUCE 8.0.4 automáticamente.)

## Instalación
Copia la carpeta **`Zonium VoxMachine.vst3`** a `C:\Program Files\Common Files\VST3\`
(hacen falta permisos de administrador). En REAPER: Preferences → Plug-ins → VST → Re-scan.

## Uso en REAPER (como instrumento, sin routing)
1. Crea una pista nueva e inserta **Zonium VoxMachine** como instrumento (FX → VSTi / Instruments).
2. Arrastra tu vocal (WAV, AIFF, MP3, FLAC u OGG) a la ventana del plugin, o pulsa **LOAD VOCAL**.
3. Arma la pista con entrada MIDI (tu teclado) y toca: al pulsar la primera tecla el vocal empieza a sonar
   a través del vocoder; mientras mantengas notas sigue, y al soltar todo se detiene.
4. Para grabar tu interpretación, graba un clip MIDI en esa misma pista.

El vocal se guarda por ruta de archivo dentro del proyecto: no muevas ni borres el archivo.

## Controles
- **VOCODER**: Bands (más bandas = más nítido), Formant (cambia el timbre), Env Attack/Release (rapidez con que sigue a la voz),
  Clarity (brillo de las consonantes), Sibilance (las "s" y "t" pasan como ruido; solo suenan mientras tocas una nota).
- **CARRIER SYNTH**: Saw / Pulse / Pulse W. / Sub / Noise, Detune, Cutoff, Resonance. 8 voces.
- **CARRIER ENVELOPE**: ADSR, Glide y botón **MONO** (legato + glide para leads tipo funk).
- **OUTPUT**: Voice Lvl, Drive, Chorus, Output, Dry/Wet (0 % = vocal original, 100 % = solo talkbox). Botón **LOOP**: repite el vocal mientras tocas. Pitch bend ±2 semitonos.
- Presets: Dabeull Talkbox, Daft Robot, Funk Mono Lead, Dark Voice, Choir Pad, Bright Vocoder.

## Consejos para que suene bien
- Voz clara, cercana y sin reverb, con buen nivel .
- Consonantes marcadas y articular bien ayudan mucho: es lo que hace inteligible a un vocoder.
- Si suena apagado: sube Clarity, Sibilance o Cutoff.

## Test del DSP (opcional)
`g++ -O2 -std=c++17 tests/dsp_test.cpp -o dsp_test && ./dsp_test`

## Licencia
JUCE 8 es AGPLv3 o licencia comercial; si vas a vender el plugin necesitas la licencia comercial de JUCE.
