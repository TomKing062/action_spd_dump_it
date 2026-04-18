# spd_dump_it
so this is the close-source spd_dump, the open-source one will keep achieved.

### [Prebuilt Program for Windows (Login Needed)](https://github.com/TomKing062/action_spd_dump_it/actions)

### [Prebuilt Program for Windows (No Login Needed; 404 Error Possible; Version May Not Be Latest)](https://nightly.link/TomKing062/action_spd_dump_it/workflows/build/main)

## Note

if you use spd_dump with auto-unlock-batches, download oldpath version.

## Diffs to 250726

### IO

#### Kick

* [Feature] introduce enhanced-kick (250907) *(Co-Authored-By @YC-nw)*
* [Fix] resolve enhanced-kick packet error (250927)
* [Change] `--kick` now equals `--kickto 2` (251123)
* [Fix] fix kick failure (251211)
* [Fix] fully stabilize kick (260109)
* [Change] kick timeout now falls back to `main()` (260205)

#### NAND flash check (FDL2 handshake)
* [Change] remove BSL_CMD_READ_FLASH_INFO check due to potential disconnection issues (250904)

* [Change] NAND flash check is now performed via check_partition() (251002)

---

### Program

* [Fix] argc handling issue during SPRD4 (250905)
* [Feature] add Ctrl+C handler during R/W operations (251002)
* [Feature] add logging for fdl1/spl; rawdata works on libusb [by commit](https://github.com/ilyakurdyukov/spreadtrum_flash/commit/ff12d48) (251030)
* [Fix] crash when `savepath != NULL` (251031)
* [Change] `GIT_VER` now uses commit count (251031)
* [Change] update `gen_tos` algorithm (251104, 260109)
* [Fix] correct spl size handling when using `-r` (251104)
* [Feature] add `dis_avb` (251013, 260108)
* [Fix] chsize, kick, and eMMC/UFS detection for ums9360/ums9632 (260103)
* [Fix] potential bug in `load_partitions` (260414)
* [Fix] `downloadnv` write operation (260205, 260418)

   `factorynv` and `calinv` are **not writable**
---

### New Features

* [Feature] `sendcmd type file`

  * supports "type-only" mode
  * if file exists: auto-fill data and length
  * can execute even if file does not exist (250921)

* [Feature] `sendpack file`

  * format: `(7e type length data crc 7e)`
  * requires file (250921)

* [Feature] `rawpack file`

  * format: `(type length data [ignored-crc])`
  * CRC and transcode handled internally
  * requires file (250921)

* [Feature] `mergenv-xml xml new_nv` (251211)

* [Feature] `mergenv-cfg cfg new_nv` (251211)

* [Feature] `g_w_force 0/1` to control `w_force` (260108)

* [Feature] support flashing PAC firmware (main branch only) (260222)

Supported forms:

```
spd_dump pac <PAC> reset
spd_dump exec_addr <addr> pac <PAC> reset
spd_dump exec_addr <addr> fdl <fdl1> <addr1> fdl <fdl2> <addr2> exec pac <PAC> reset
```

Notes:

* supports custom FDL during flashing
* only supports **partname-based partition table** (UBIFS / GPT)
* legacy **ID-based (RDA) table not supported**
* region selection (e.g. OPPO/Realme PAC) not supported

---

### Path Management

* [Change] default output directory:

  * `./YYMMDD_hhmmss`
  * `./YYMMDD_hhmmss/tmp` (260414)
