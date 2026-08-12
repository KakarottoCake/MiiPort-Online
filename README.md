# MiiPort Online

A performance-first Nintendo Switch homebrew for importing, exporting, browsing, and downloading Miis. It is based on Genwald's MiiPort and retains its local import/export workflow.

The Browse tab uses InfiniMii's console API rather than scraping HTML. Categories and search are requested only when selected; a selected Mii downloads as an 88-byte Switch `.charinfo` record and is then imported through MiiPort's existing duplicate-ID flow. See [docs/infinimii-adapter.md](docs/infinimii-adapter.md).

## Performance budget

- Catalog responses are capped at 96 KiB.
- Results use a controller-navigable 4×3 image grid and can page through the full provider result set with L/R.
- Exactly one catalog, thumbnail, or download request is active at a time; there is no background prefetch or crawl.
- Successful catalog pages are retained on SD for 10 minutes before another provider request is made.
- Only the visible page's 12 thumbnails can occupy GPU memory. Previews load serially, are cached on SD for 30 days, and the cache is pruned to 240 compact RGBA entries.
- The provider's fixed 64×64 RGBA previews avoid PNG/JPEG decoding work on console.

## Build

Run `scripts/build-local -j2`. This wrapper makes a temporary no-space copy before invoking devkitPro, so builds work even when the checkout directory contains spaces. It writes `MiiPort.nro` to the repository root.

QR-image importing is disabled by default because the catalog workflow downloads compact `.charinfo` files directly. Enable the legacy QR feature with `scripts/build-local ENABLE_QR=1` on systems with Switch libjpeg-turbo installed.

### Internal macOS test harness

`scripts/build-macos-test` builds `build-macos/MiiPortMacTest`, a non-release
command-line harness around the same production parser, cache, transport, and
request client. Running it without arguments uses a local fixture and makes no
network request. `build-macos/MiiPortMacTest --live --query mario` performs one
bounded live request for integration testing.

## Installation

Copy the locally built `MiiPort.nro` to `sd:/switch/MiiPort.nro`.


Some features require the setting 
```
[mii]
is_db_test_mode_enabled=u8!0x1
```
which can be set in `/atmosphere/config/system_settings.ini`
## Screenshots
<img alt="Import tab" src="https://user-images.githubusercontent.com/11589515/116328811-6dd88380-a78f-11eb-841d-b06d5ed3f587.jpg" width="65%">
<img alt="Export tab"  src="https://user-images.githubusercontent.com/11589515/116329846-d163b080-a791-11eb-917f-1d4921a54545.jpg" width="65%">


## Usage
Place Mii character files in `sd:/MiiPort/Miis/` with a file extension that corresponds to their format.  
currently supported import formats include:
- jpeg images of Mii QR codes
    - requires the mii QR key
- charinfo
    - can also use the `.bin` extension
- NFIF
    - can also use the `.dat` extension
- coredata
- storedata

## QR key
In order to import Miis from a qr code, you must supply the Mii QR key. This is needed to decrypt the Mii data stored in Mii QR codes. You can find this on the internet by searching for "Mii QR key" or "slot0x31KeyN".  
This program looks for the key in hex in the file `/MiiPort/qrkey.txt`.
It will accept it in a variety of formats such as:
`[0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA]`
or `AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA`
