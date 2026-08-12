# MiiPort Online

MiiPort Online lets you find Miis on a Switch and add them to your local Mii database. It adds an online browser to Genwald's MiiPort, so you don't have to find a Mii on a website, download a file on another device, and move it to the console by hand.

The online browser uses InfiniMii. You can search by name, creator, description, or uploader. You can also browse Popular This Week, New, Top Miis, Official Miis, and Random Miis. Results show as a 4×3 image grid. Use the D-pad or control stick to move around, L and R to change pages, and A to view a Mii and download it.

The original local import and export features are still included.

## Install

1. Download [MiiPort Online 0.1.3-Online](https://github.com/KakarottoCake/MiiPort-Online/releases/tag/v0.1.3-Online).
2. Copy `MiiPort.nro` to `sd:/switch/`.
3. Start it from the Homebrew Menu.

Downloaded Miis are saved in `sd:/MiiPort/miis/Downloads/` and imported into the Mii database.

Some Mii database features may need this setting in `sd:/atmosphere/config/system_settings.ini`:

```ini
[mii]
is_db_test_mode_enabled=u8!0x1
```

## Build from source

You'll need devkitPro with libnx, libcurl, and the Switch build tools.

Clone the release branch with its submodules:

```sh
git clone --branch v0.1.3-Online --recurse-submodules https://github.com/KakarottoCake/MiiPort-Online.git
cd MiiPort-Online
./scripts/build-local -j2
```

The build writes `MiiPort.nro` to the project folder. Copy it to `sd:/switch/` to test it on a Switch or in Eden.

To build the optional QR import support, run:

```sh
./scripts/build-local ENABLE_QR=1 -j2
```

## macOS test harness

The macOS harness tests the catalog parser and network client without building a Switch app.

```sh
./scripts/build-macos-test
./build-macos/MiiPortMacTest
```

The first command builds the harness. The second runs the offline fixture test. Add `--live` to make one bounded request to InfiniMii.

## Screenshots

### Browse

![Browse screen](docs/screenshots/01-browse.png)

### Popular Miis

![Popular Mii grid](docs/screenshots/02-popular-grid.png)

### Official Miis

![Official Mii grid](docs/screenshots/03-official-grid.png)

### Mii details and download

![Mii details dialog](docs/screenshots/04-mii-details.png)

## Performance

- Only the current page is requested.
- One catalog, preview, or download request runs at a time.
- Preview images load one at a time and are cached on the SD card.
- Catalog pages are cached for 10 minutes.
- Only the current page's 12 preview images stay in GPU memory.

## License

The main project uses the ISC License. See [LICENSE.md](LICENSE.md). The Borealis submodule has its own GPLv3 license.
