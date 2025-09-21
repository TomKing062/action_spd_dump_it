## spd_dump_it
so this is the close-source spd_dump, the open-source one will keep achieved.

#### [Prebuilt program for windows(login is needed)](https://github.com/TomKing062/action_spd_dump_it/actions)

### Diffs to 250726

1. [250904] change timeout to 3s, remove [BSL_CMD_READ_FLASH_INFO check during FDL2 handshake](https://github.com/TomKing062/spreadtrum_flash/commit/a76a03e1f4a814203d3e5eae3d1f8e38b14b9376#diff-ecc2b15491061308698809ccbc6cc4a5026f81036c8bc4cb60828abf284128b4R689)

2. [250905] fix argc issue during SPRD4

3. [250907] import enhanced-kick from async-full branch [Co-Authored-By @YC-nw]

4. [250921] `sendcmd type file` for type only, if file exists, data and length will be filled, this cmd can execute when file not exists

   `sendpack file` for (7e type length data crc 7e), file must exist

   `rawpack file` for (type length data [ignored-crc]), crc and transcode will be performed, file must exist
