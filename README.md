# winhide

Toggle jendela Windows: kalau sedang tampil -> minimize, kalau minimize -> tampil.
Satu file exe, tanpa background process. State tidak disimpan: setiap run dicek
langsung lewat `IsIconic`, jadi tidak ada state yang melenceng.

## Build

```powershell
.\build.ps1
```

Butuh `g++` (MSYS2 UCRT64, otomatis dicari di `C:\msys64\ucrt64\bin`).
Hasilnya `winhide.exe` statis (~1.3 MB), tanpa perlu DLL runtime.

## Pakai

```powershell
winhide.exe                  # toggle target default
winhide.exe --list           # lihat semua jendela yang bisa dipilih
winhide.exe --set            # pilih jendela lewat picker, simpan sebagai default
winhide.exe --set 4          # simpan nomor 4 dari --list
winhide.exe --show           # tampilkan + fokus
winhide.exe --hide           # minimize
winhide.exe --status         # target apa yang kepilih, kondisinya apa
winhide.exe --clear          # hapus target default
```

Sekali pakai tanpa disimpan:

```powershell
winhide.exe --process Discord.exe
winhide.exe --process Discord.exe --title "Stream - someuser" --hide
winhide.exe --process Discord.exe --title "Discord" --show
```

Exit code: `0` sukses, `1` target/config tidak ada, `2` argumen salah.

## Target itu jendela, bukan exe

Contoh: Discord punya beberapa jendela dari satu `Discord.exe` (jendela utama,
stream yang di-pop-out, kamera yang di-pop-out). Karena itu target disimpan
sebagai **process + judul jendela**:

```
process=Discord.exe
title=Stream - someuser
match=exact
class=Chrome_WidgetWin_1
```

`class` cuma penentu kalau ada lebih dari satu kandidat. Handle/PID tidak pernah
disimpan karena berubah tiap run, jadi matching di-resolve ulang setiap kali.

- `match=exact` (default): judul harus sama persis, case-insensitive.
- `--contains`: judul cukup mengandung teksnya, berguna untuk judul yang berubah
  (misal tab browser).
- `--title` dikosongkan: semua jendela dari process itu jadi target.

Kalau lebih dari satu jendela cocok, semuanya di-toggle bersama (ada yang tampil
-> minimize semua; semua minimize -> tampilkan semua). `--status` menunjukkan
persis jendela mana yang kepilih.

## Alt+Tab

Saat di-minimize, jendela juga didorong ke dasar z-order. Alt+Tab mengurutkan
jendela berdasarkan z-order, jadi target yang di-hide muncul paling akhir,
bukan menyelip di antara jendela yang sedang dipakai. Saat di-show, jendela
kembali ke depan dan fokus.

## Config

`winhide.cfg` ditulis di folder yang sama dengan exe, jadi foldernya portable.
Kalau foldernya read-only, arahkan lewat env var `WINHIDE_CONFIG`:

```powershell
$env:WINHIDE_CONFIG = "$env:APPDATA\winhide\winhide.cfg"
```

Formatnya `key=value` biasa, boleh diedit manual.

## Batasan

- Jendela milik proses elevated tidak bisa diutak-atik tanpa run as admin (UIPI).
- Hanya jendela di virtual desktop yang sedang aktif yang terdeteksi.
- Aplikasi UWP (Settings, Calculator, dll) prosesnya `ApplicationFrameHost.exe`;
  pakai `--title` untuk menargetkannya.
- `SetForegroundWindow` bisa ditolak Windows dalam kondisi tertentu; program
  sudah mencoba trik `AttachThreadInput`, tapi fokus tidak selalu dijamin.
