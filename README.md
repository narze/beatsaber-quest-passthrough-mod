# Passthrough

A Beat Saber mod for Meta Quest that shows your real room (passthrough) as the background, instead of the game environment. Notes, bombs, walls, sabers and the HUD stay visible.

## Supported versions

| Item | Version |
|---|---|
| Beat Saber | 1.40.8_7379 |
| Modloader | scotland2 0.1.7 (game patched with ModsBeforeFriday) |
| Dependencies | beatsaber-hook 6.4.2, paper2_scotland2 4.7.0, BSML 0.4.55, custom-types 0.18.4 |
| Tested on | Quest 3S, Horizon OS with OpenXR runtime 207 |

Other game versions are not supported. The mod uses game types from bs-cordl 4008.

## Requirements

The game APK must request passthrough. Without it, the OpenXR runtime hides `XR_FB_passthrough` and the mod logs `XR_FB_passthrough functions loaded: false`.

Do one of these:

- In ModsBeforeFriday, enable the "Passthrough to headset cameras" permission. Note: MBF uninstalls the game to re-patch it.
- Or use `tools/patch_apk.sh`. It adds `com.oculus.feature.PASSTHROUGH` to the installed APK and signs it with the MBF debug key, so `adb install -r` updates the game in place (app data and DLC stay):

  ```sh
  adb shell pm path com.beatgames.beatsaber        # find base.apk
  adb pull <path>/base.apk base.apk
  tools/patch_apk.sh base.apk passthrough.apk <mbf>/mbf-agent/src/debug_cert.pem
  adb install -r passthrough.apk
  ```

  Keep `base.apk` as a backup. To roll back: `adb install -r -d base.apk`.

After an install, Horizon OS can show a "Restore App" dialog when the game starts. Select **Open App**. Do not select **Restore**.

## Build

Tools: [qpm](https://github.com/QuestPackageManager/QPM.CLI), [mise](https://mise.jdx.dev) (installs cmake and ninja from `mise.toml`), and the Android NDK that qpm resolves.

```sh
qpm restore
qpm ndk resolve -d
./deploy.sh --no-log        # also builds; see Deploy
```

Unit tests run on the host (no headset):

```sh
./test/run.sh
```

To make an installable package (`Passthrough.qmod`, with the dependencies in `mod.json`):

```sh
qpm qmod zip
```

## Deploy

Connect the headset with USB (developer mode on, adb authorized), then:

```sh
./deploy.sh
```

The script builds the mod, pushes `libPassthrough.so` to `/sdcard/ModData/com.beatgames.beatsaber/Modloader/early_mods/`, and stops the game. Then start Beat Saber from the headset library. The script waits for the game and shows the mod log.

- `--launch`: start the game with adb. Since 2026-10-23, games started with `adb shell am start` crash after a few seconds on the test headset, also without this mod. Starts from the headset library do not crash.
- `--no-log`: exit after the push.

The mod must be an early mod. Normal mods load after the game creates its OpenXR instance, too late for the hooks.

## Settings

Settings are in the game in four places: Mod Settings > Passthrough, the "Passthrough" button in the main menu mod list, the "Passthrough" tab in the gameplay setup panel, and a panel at the left of the pause menu. Changes apply at once and are saved to `/sdcard/ModData/com.beatgames.beatsaber/Configs/Passthrough.json`.

| Setting | Default | What it does |
|---|---|---|
| Passthrough | on | Main switch. Off: the game looks as without the mod. |
| Hide song environment | on | Hides the song environment, its lights and their glow on notes and bombs. |
| Hide menu environment | on | Hides the main menu environment. |
| Distance fog | off | Keeps the game's fog when the song environment is hidden. Far notes then fade in from the room. |
| See-through below brightness | 0.015 | Pixels darker than this show the room. Higher: more see-through. If dark parts of objects become see-through, set it lower. |
| Room brightness (opacity) | 1.0 | Opacity of the passthrough image. |
| Debug: show see-through mask | off | Shows the mask in grayscale: white is the game, black is the room. Not saved. |

## How it works

- **OpenXR hooks** (`src/xr_hooks.cpp`): the mod hooks `xrGetInstanceProcAddr` of the OpenXR loader and wraps the functions it needs.
  - `xrCreateInstance`: adds `XR_FB_passthrough`, because the game does not enable it.
  - `xrCreateSession`: starts passthrough and creates a passthrough layer.
  - `xrEndFrame`: puts the passthrough layer under the game layer and sets source alpha blending on the game layer.
- **Alpha key** (`src/alpha_pass.cpp`): Beat Saber writes glow strength into the alpha channel, so that alpha is not coverage. Before each eye image goes to the compositor, a GL pass sets `alpha = smoothstep(low, high, max(r, g, b))`. Black pixels (the hidden environment) show the room. Lit objects are opaque.
- **Environment** (`src/environment.cpp`):
  - Disables the renderers of environment scenes and of the menu environment.
  - Removes the light layers (13, 14, 29) from the gameplay camera, because light scripts turn renderers on again.
  - Gives shaders a black bloom pre-pass texture. The game draws all lights into that texture, and notes, bombs and fog read it.
  - Turns the bloom fog off, unless "Distance fog" is on.
- **Settings UI** (`src/settings_ui.cpp`): BSML-Lite controls, the same in all four places.

## Known limitations

- The passthrough stays active when the "Passthrough" setting is off. Only its layer is removed from the frame.
- Some menu particles (layer 27) stay visible.
- The setting threshold is in linear color. A small change of the slider has a large effect on dark colors.
