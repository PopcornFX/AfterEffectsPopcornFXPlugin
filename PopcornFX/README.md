# After Effects PopcornFX Plugin

Integrates the **PopcornFX Runtime SDK** into **After Effects** as a Plugin.
* **Version:** `v2.19.3`
* **Platforms:** `Windows` (http://www.popcornfx.com/contact-us/) for more information.

## Setup

* **[Install](https://wiki.popcornfx.com/index.php?title=Announcements)** the PopcornFX v2 Editor **matching** that plugin version
  to create effects
* **[Install](https://www.popcornfx.com/docs/popcornfx-v2/plugins/after-effects-plugin/plugin-installation/) the After Effects PopcornFX Plugin** in your After Effects install folder

## Building the plugin from Community release
* Download the plugin archive or clone it from our Github repository : https://github.com/PopcornFX/AfterEffectsPopcornFXPlugin
* Run `download_3rd_party.bat` (Windows) or `download_3rd_party.sh` (MacOS) to download the PopcornFX SDK.
* Download and nstall QT (https://www.qt.io/download-qt-installer-oss), and set `QTDIR` environment variable to your installation path (Example: `C:\Qt\Qt5.15.2\5.15.2\msvc2019_64\`), the plugin is tested to compile on QT 5.15.X
* Download and install windows SDK 10.0.17134 (Direct download link: https://go.microsoft.com/fwlink/p/?linkid=870807)
* Launch `projects/AfterEffects_vs2019/PopcornFX_AfterEffectsPlugin.sln` (Windows) or `make -j -f projects/AfterEffects_macosx/PopcornFX_AfterEffectsPlugin.make` (MacOS), to build the plugin.
* Copy the `PopcornFX/` folder into the plugin folder in your After Effects installation.
  * On Mac, as a temporary mesure, you'll need to copy the binaries from `projects/intermediate/AfterEffects/GM/<arch>/<profile>/*` into `PopcornFX/`
* Run After Effects, you will have the plugin installed.

## Updating the plugin (minor or patch update)

* **(Minor update only)** Open PopcornFX Editor, **upgrade your project from the [project launcher](https://www.popcornfx.com/docs/popcornfx-v2/editor/project-launcher/)**
* **Remove the old plugin**
* [Install](https://www.popcornfx.com/docs/popcornfx-v2/plugins/after-effects-plugin/plugin-installation/) the latest released archive.
* **(Minor update only)** Open your After Effects project and reimport all effects, they will not load otherwise

## Quick Links: Documentation and Support

* [**Plugin** documentation](https://www.popcornfx.com/docs/popcornfx-v2/plugins/after-effects-plugin/) (Install, Import, Setup, Troubleshooting, etc..)
* [PopcornFX **Editor** documentation](https://www.popcornfx.com/docs/popcornfx-v2/)
* [PopcornFX Discord](https://discord.gg/4ka27cVrsf)

The PopcornFX Runtime SDK cannot be in any way: transfered, modified, or used
without this Plugin or outside After Effects, see [LICENSE.md](/LICENSE.md).

## License

See [LICENSE.md](/LICENSE.md).
