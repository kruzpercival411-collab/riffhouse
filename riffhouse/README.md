# Riffhouse Studio (VST3)

A rock songwriting instrument. It writes a full song (progressions, guitar, bass, drums), plays it with its own modelled guitar, bass and drum engines in sync with your DAW, and lets you drag any part out as MIDI.

## Build
Push this folder to a GitHub repo. The workflow in `.github/workflows/build.yml` builds a Windows VST3. Download it from the run's Artifacts and copy `Riffhouse Studio.vst3` into `C:\Program Files\Common Files\VST3`.

## Use in Ableton
1. Put Riffhouse Studio on a MIDI track. Press play: the song plays in time with Ableton (it loops).
2. Pick Style / Key / Mode / Structure / Voicing. Hit GENERATE SONG for a new one.
3. Click a section in the timeline, then RE-ROLL SECTION to rewrite just that part (all verses change together).
4. Drag GUITAR / BASS / DRUMS / FULL SONG MIDI onto tracks. Drums use General MIDI notes, so they work in Drum Rack.
5. To play your own or edited MIDI through the engines, turn Auto-play off and set MIDI IN PLAYS.

Songs are deterministic from saved seeds, so your song comes back when you reopen the project.
